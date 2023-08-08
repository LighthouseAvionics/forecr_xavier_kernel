/*
 * Copyright (c) 2016-2019, NVIDIA Corporation. All rights reserved.
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

#include "pva_mailbox_t23x.h"
#include "pva_interface_regs_t23x.h"
#include "pva_ccq_t23x.h"

struct pva_version_config pva_t23x_config = {
	.read_mailbox = pva_read_mailbox_t23x,
	.write_mailbox = pva_write_mailbox_t23x,
	.read_status_interface = read_status_interface_t23x,
	.ccq_send_task = pva_ccq_send_task_t23x,
	.submit_cmd_sync_locked = pva_send_cmd_sync_locked,
	.submit_cmd_sync = pva_send_cmd_sync,
	.irq_count = 9,
};
