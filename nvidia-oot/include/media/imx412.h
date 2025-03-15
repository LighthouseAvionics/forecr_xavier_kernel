/* SPDX-License-Identifier: GPL-2.0 */
/*
 * imx412.h - imx412 sensor header
 *
 * Copyright (c) 2020, RidgeRun. All rights reserved.
 * Copyright (c) 2025, Lighthouse Avionics. All rights reserved.
 *
 * Contact us: support@ridgerun.com
 * Contact us: tyler@lighthouseavionics.com
 *
 */

#ifndef __IMX412_H__
#define __IMX412_H__

/* imx412 - sensor parameters */
#define IMX412_MIN_GAIN		                (0)
#define IMX412_MAX_GAIN		                (978)
#define IMX412_ANALOG_GAIN_C0		        (1024)
#define IMX412_SHIFT_8_BITS			        (8)
#define IMX412_MIN_FRAME_LENGTH		        (256)
#define IMX412_MAX_FRAME_LENGTH		        (65535)
#define IMX412_MIN_COARSE_EXPOSURE	        (11) // Adjusted from 1 to 11 to mitigate blinking orange light
#define IMX412_MAX_COARSE_DIFF		        (10)
#define IMX412_MASK_LSB_2_BITS			    0x0003
#define IMX412_MASK_LSB_8_BITS			    0x00ff

/* imx412 sensor register address */
#define IMX412_MODEL_ID_ADDR_MSB		    0x0000
#define IMX412_MODEL_ID_ADDR_LSB		    0x0001
#define IMX412_ANALOG_GAIN_ADDR_MSB		    0x0204
#define IMX412_ANALOG_GAIN_ADDR_LSB		    0x0205
#define IMX412_DIGITAL_GAIN_ADDR_MSB		0x020e
#define IMX412_DIGITAL_GAIN_ADDR_LSB		0x020f
#define IMX412_FRAME_LENGTH_ADDR_MSB		0x0340
#define IMX412_FRAME_LENGTH_ADDR_LSB		0x0341
#define IMX412_COARSE_INTEG_TIME_ADDR_MSB	0x0202
#define IMX412_COARSE_INTEG_TIME_ADDR_LSB	0x0203
#define IMX412_FINE_INTEG_TIME_ADDR_MSB		0x0200
#define IMX412_FINE_INTEG_TIME_ADDR_LSB		0x0201
#define IMX412_GROUP_HOLD_ADDR		        0x0104

#endif /* __IMX412_H__ */
