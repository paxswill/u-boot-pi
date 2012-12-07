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

#ifndef _UFS_H_
#define _UFS_H_


#define UFS_MAGIC		0x11954
#define UFS2_MAGIC		0x19540119
#define UFS_INODE		2
#define UFS_FILETYPE_DIR	4
#define UFS_FILETYPE_LNK	10
#define UFS_MAX_SYMLNK_CNT	8

#define UFS_DIRBLKS	12
#define UFS_INDIRBLKS	3

#define UFS_ATTR_DIR	040000

int ufs_set_blk_dev(block_dev_desc_t *dev_desc, disk_partition_t *info);
int ufs_ls(const char *dirname);
int ufs_open(const char *filename);
int ufs_read(const char *filename, unsigned len);
void ufs_close(void);
int ufs_mount(unsigned part_length);

#endif /* _UFS_H_ */
