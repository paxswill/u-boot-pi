/*
 * (C) Copyright 2012 Will Ross <paxswill@paxswill.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA*
 */

#include <common.h>
#include <command.h>
#include <part.h>
#include <fs.h>
#include <ufs.h>

int do_ufs_fsload(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	/* TODO */
}

U_BOOT_CMD(ufsload, 6, 0, do_ufs_fsload,
	"load binary file from a UFS/UFS2 filesystem",
	"<interface> <dev:part> [addr] [filename] [bytes]\n"
	"	- load binary file 'filename' form 'dev' on 'interface'\n"
	"	  to address 'addr' from UFS/UFS2 filesystem.\n");

static int do_ufs_ls(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	/* TODO */
}

U_BOOT_CMD(ufsls, 4, 1, do_ufs_ls,
	"list files in a directory (default /)",
	"<interface> <dev[:part]> [directory]\n"
	"	-list files from 'dev' on 'interface' in a 'directory'");

