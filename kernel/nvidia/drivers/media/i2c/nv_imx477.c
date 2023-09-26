/*
 * imx477.c - imx477 sensor driver
 *
 * Copyright (C) 2022, Leopardimaging Inc.
 * Based on Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra_v4l2_camera.h>
#include <media/tegracam_core.h>
#include <media/imx477.h>

#include "../platform/tegra/camera/camera_gpio.h"
#include "imx477_mode_tbls.h"

#define IMX477_SENSOR_INTERNAL_CLK_FREQ   840000000

static const struct of_device_id imx477_of_match[] = {
	{.compatible = "sony,imx477",},
	{},
};

MODULE_DEVICE_TABLE(of, imx477_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

struct imx477 {
	struct i2c_client *i2c_client;
	struct v4l2_subdev *subdev;
	u16 fine_integ_time;
	u32 frame_length;
	struct camera_common_data *s_data;
	struct tegracam_device *tc_dev;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	.use_single_rw = true,
#else
	.use_single_read = true,
	.use_single_write = true,
#endif
};

static inline void imx477_get_frame_length_regs(imx477_reg * regs,
						u32 frame_length)
{
	regs->addr = IMX477_FRAME_LENGTH_ADDR_MSB;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = IMX477_FRAME_LENGTH_ADDR_LSB;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void imx477_get_coarse_integ_time_regs(imx477_reg * regs,
						     u32 coarse_time)
{
	regs->addr = IMX477_COARSE_INTEG_TIME_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX477_COARSE_INTEG_TIME_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void imx477_get_gain_reg(imx477_reg * reg, u16 gain)
{
	reg->addr = IMX477_ANALOG_GAIN_ADDR_MSB;
	reg->val = (gain >> IMX477_SHIFT_8_BITS) & IMX477_MASK_LSB_2_BITS;

	(reg + 1)->addr = IMX477_ANALOG_GAIN_ADDR_LSB;
	(reg + 1)->val = (gain) & IMX477_MASK_LSB_8_BITS;
}

static inline int imx477_read_reg(struct camera_common_data *s_data,
				  u16 addr, u8 * val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xff;

	return err;
}

static inline int imx477_write_reg(struct camera_common_data *s_data,
				   u16 addr, u8 val)
{
	int err = 0;

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(s_data->dev, "%s: i2c write failed, 0x%x = %x",
			__func__, addr, val);

	return err;
}

static int imx477_write_table(struct imx477 *priv, const imx477_reg table[])
{
	return regmap_util_write_table_8(priv->s_data->regmap, table, NULL, 0,
					 IMX477_TABLE_WAIT_MS,
					 IMX477_TABLE_END);
}

static int imx477_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	int err;

	dev_dbg(dev, "%s: Setting group hold control to: %u\n", __func__, val);

	err = imx477_write_reg(s_data, IMX477_GROUP_HOLD_ADDR, val);
	if (err) {
		dev_err(dev, "%s: Group hold control error\n", __func__);
		return err;
	}

	return 0;
}

static int imx477_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = s_data->dev;
	const struct sensor_mode_properties *mode =
	    &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
	int err = 0, i = 0;
	imx477_reg gain_reg[2];
	s16 gain;

	dev_dbg(dev, "%s: Setting gain control to: %lld\n", __func__, val);

	if (val < mode->control_properties.min_gain_val)
		val = mode->control_properties.min_gain_val;
	else if (val > mode->control_properties.max_gain_val)
		val = mode->control_properties.max_gain_val;

	/* Gain Formula:
	   Gain = (IMX477_GAIN_C0 - (IMX477_GAIN_C0 * gain_factor / val))
	 */
	gain =
	    (s16) (IMX477_ANALOG_GAIN_C0 -
		   (mode->control_properties.gain_factor *
		    IMX477_ANALOG_GAIN_C0 / val));

	if (gain < IMX477_MIN_GAIN)
		gain = IMX477_MAX_GAIN;
	else if (gain > IMX477_MAX_GAIN)
		gain = IMX477_MAX_GAIN;

	dev_dbg(dev, "%s: val: %lld (/%d) [times], gain: %u\n",
		__func__, val, mode->control_properties.gain_factor, gain);

	imx477_get_gain_reg(gain_reg, (u16) gain);

	for (i = 0; i < ARRAY_SIZE(gain_reg); i++) {
		err = imx477_write_reg(s_data, gain_reg[i].addr,
				       gain_reg[i].val);
		if (err) {
			dev_err(dev, "%s: gain control error\n", __func__);
			break;
		}
	}

	return err;
}

static int imx477_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct imx477 *priv = (struct imx477 *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	const struct sensor_mode_properties *mode =
	    &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

	int err = 0;
	imx477_reg fl_regs[2];
	u32 frame_length;
	int i;

	dev_dbg(dev, "%s: Setting framerate control to: %lld\n", __func__, val);

	frame_length = (u32) (IMX477_SENSOR_INTERNAL_CLK_FREQ *
			      (u64) mode->control_properties.framerate_factor /
			      mode->image_properties.line_length / val);

	if (frame_length < IMX477_MIN_FRAME_LENGTH)
		frame_length = IMX477_MIN_FRAME_LENGTH;
	else if (frame_length > IMX477_MAX_FRAME_LENGTH)
		frame_length = IMX477_MAX_FRAME_LENGTH;

	dev_dbg(dev,
		"%s: val: %llde-6 [fps], frame_length: %u [lines]\n",
		__func__, val, frame_length);

	imx477_get_frame_length_regs(fl_regs, frame_length);
	for (i = 0; i < 2; i++) {
		err = imx477_write_reg(s_data, fl_regs[i].addr, fl_regs[i].val);
		if (err) {
			dev_err(dev,
				"%s: frame_length control error\n", __func__);
			return err;
		}
	}

	priv->frame_length = frame_length;

	return err;
}

static int imx477_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct imx477 *priv = (struct imx477 *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	const struct sensor_mode_properties *mode =
	    &s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

	int err = 0;
	imx477_reg ct_regs[2];
	const s32 max_coarse_time = priv->frame_length - IMX477_MAX_COARSE_DIFF;
	const s32 fine_integ_time_factor = priv->fine_integ_time *
	    mode->control_properties.exposure_factor /
	    IMX477_SENSOR_INTERNAL_CLK_FREQ;
	u32 coarse_time;
	int i;

	dev_dbg(dev, "%s: Setting exposure control to: %lld\n", __func__, val);

	coarse_time = (val - fine_integ_time_factor)
	    * IMX477_SENSOR_INTERNAL_CLK_FREQ
	    / mode->control_properties.exposure_factor
	    / mode->image_properties.line_length;

	if (coarse_time < IMX477_MIN_COARSE_EXPOSURE)
		coarse_time = IMX477_MIN_COARSE_EXPOSURE;
	else if (coarse_time > max_coarse_time) {
		coarse_time = max_coarse_time;
		dev_dbg(dev,
			"%s: exposure limited by frame_length: %d [lines]\n",
			__func__, max_coarse_time);
	}

	dev_dbg(dev, "%s: val: %lld [us], coarse_time: %d [lines]\n",
		__func__, val, coarse_time);

	imx477_get_coarse_integ_time_regs(ct_regs, coarse_time);

	for (i = 0; i < 2; i++) {
		err = imx477_write_reg(s_data, ct_regs[i].addr, ct_regs[i].val);
		if (err) {
			dev_dbg(dev,
				"%s: coarse_time control error\n", __func__);
			return err;
		}
	}

	return err;
}

static struct tegracam_ctrl_ops imx477_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = imx477_set_gain,
	.set_exposure = imx477_set_exposure,
	.set_frame_rate = imx477_set_frame_rate,
	.set_group_hold = imx477_set_group_hold,
};

static int imx477_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power on\n", __func__);
	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	/*exit reset mode: XCLR */
	if (pw->reset_gpio) {
		gpio_set_value(pw->reset_gpio, 0);
		usleep_range(300000, 303000);
		gpio_set_value(pw->reset_gpio, 1);
		usleep_range(300000, 303000);
	}

	pw->state = SWITCH_ON;
	return 0;
}

static int imx477_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power off\n", __func__);

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (!err)
			goto power_off_done;
		else
			dev_err(dev, "%s failed.\n", __func__);
		return err;
	}
	/* enter reset mode: XCLR */
	usleep_range(300000, 303000);
	if (pw->reset_gpio)
		gpio_set_value(pw->reset_gpio, 0);

power_off_done:
	pw->state = SWITCH_OFF;

	return 0;
}

static int imx477_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	return 0;
}

static int imx477_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	const char *mclk_name;
	struct clk *parent;
	int err = 0;

	mclk_name = pdata->mclk_name ?
		    pdata->mclk_name : "extperiph1";
	pw->mclk = devm_clk_get(dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(dev, "unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	parent = devm_clk_get(dev, "pllp_grtba");
	if (IS_ERR(parent))
		dev_err(dev, "devm_clk_get failed for pllp_grtba");
	else
		clk_set_parent(pw->mclk, parent);

	pw->reset_gpio = pdata->reset_gpio;

	pw->state = SWITCH_OFF;
	return err;
}

static struct camera_common_pdata *imx477_parse_dt(struct tegracam_device
						   *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *np = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	struct camera_common_pdata *ret = NULL;
	int err = 0;
	int gpio;

	if (!np)
		return NULL;

	match = of_match_device(imx477_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev,
					sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "reset-gpios not found\n");
		goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err)
		dev_dbg(dev, "mclk name not present, "
			"assume sensor driven externally\n");

	board_priv_pdata->has_eeprom = of_property_read_bool(np, "has-eeprom");

	return board_priv_pdata;

error:
	devm_kfree(dev, board_priv_pdata);

	return ret;
}

static int imx477_set_mode(struct tegracam_device *tc_dev)
{
	struct imx477 *priv = (struct imx477 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;

	int err = 0;

	dev_dbg(tc_dev->dev, "%s:\n", __func__);

	err = imx477_write_table(priv, mode_table[IMX477_MODE_COMMON]);
	if (err)
		return err;

	err = imx477_write_table(priv, mode_table[s_data->mode]);
	if (err)
		return err;

	return 0;
}

static int imx477_start_streaming(struct tegracam_device *tc_dev)
{
	int err = 0;
	struct imx477 *priv = (struct imx477 *)tegracam_get_privdata(tc_dev);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);

	err = imx477_write_table(priv, mode_table[IMX477_START_STREAM]);
	usleep_range(300000, 303000);
	return err;
}

static int imx477_stop_streaming(struct tegracam_device *tc_dev)
{
	int err;
	struct imx477 *priv = (struct imx477 *)tegracam_get_privdata(tc_dev);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	err = imx477_write_table(priv, mode_table[IMX477_STOP_STREAM]);
    usleep_range(300000, 303000);
	return err;
}

static struct camera_common_sensor_ops imx477_common_ops = {
	.numfrmfmts = ARRAY_SIZE(imx477_frmfmt),
	.frmfmt_table = imx477_frmfmt,
	.power_on = imx477_power_on,
	.power_off = imx477_power_off,
	.write_reg = imx477_write_reg,
	.read_reg = imx477_read_reg,
	.parse_dt = imx477_parse_dt,
	.power_get = imx477_power_get,
	.power_put = imx477_power_put,
	.set_mode = imx477_set_mode,
	.start_streaming = imx477_start_streaming,
	.stop_streaming = imx477_stop_streaming,
};

static int imx477_board_setup(struct imx477 *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	int err = 0;

	// Skip mclk enable as this camera has an internal oscillator
	err = camera_common_mclk_enable(s_data);
	if (err) {
		dev_err(dev,
			"Error %d turning on mclk\n", err);
		return err;
	}

	err = imx477_power_on(s_data);
	if (err) {
		dev_err(dev, "error during power on sensor (%d)\n", err);
		goto done;
	}

	imx477_power_off(s_data);

done:
	return err;
}

static int imx477_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops imx477_subdev_internal_ops = {
	.open = imx477_open,
};

static int imx477_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	struct imx477 *priv;
	int err;

	dev_dbg(dev, "probing v4l2 sensor at addr 0x%0x\n", client->addr);

	if (!IS_ENABLED(CONFIG_OF) || !client->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct imx477), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	tc_dev = devm_kzalloc(dev, sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;
	strncpy(tc_dev->name, "imx477", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &imx477_common_ops;
	tc_dev->v4l2sd_internal_ops = &imx477_subdev_internal_ops;
	tc_dev->tcctrl_ops = &imx477_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	err = imx477_board_setup(priv);
	if (err) {
		dev_err(dev, "board setup failed\n");
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_dbg(dev, "detected imx477 sensor\n");

	return 0;
}

static int imx477_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct imx477 *priv = (struct imx477 *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);

	return 0;
}

static const struct i2c_device_id imx477_id[] = {
	{"imx477", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, imx477_id);

static struct i2c_driver imx477_i2c_driver = {
	.driver = {
		   .name = "imx477",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(imx477_of_match),
		   },
	.probe = imx477_probe,
	.remove = imx477_remove,
	.id_table = imx477_id,
};

module_i2c_driver(imx477_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for Sony IMX477");
MODULE_AUTHOR("Weicen Zhou <weicenz@leopardimaging.com>");
MODULE_LICENSE("GPL v2");
