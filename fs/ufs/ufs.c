/* ufs.c - Unix File System */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004, 2005  Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <common.h>
#include <asm/byteorder.h>
#include <ufs.h>

/* Calculate in which group the inode can be found.  */
#define inode_group(inode,sblock) ()

#define UFS_BLKSZ(sblock) (__le32_to_cpu (sblock->bsize))

#define INODE(data,field) (data->ufs_type == UFS1 ? \
                           data->inode.  field : data->inode2.  field)
#define INODE_ENDIAN(data,field,bits1,bits2) (data->ufs_type == UFS1 ? \
                           __le##bits1_to_cpu (data->inode.field) : \
                           __le##bits2_to_cpu (data->inode2.field))
#define INODE_SIZE(data) INODE_ENDIAN (data,size,32,64)
#define INODE_MODE(data) INODE_ENDIAN (data,mode,16,16)
#define INODE_BLKSZ(data) (data->ufs_type == UFS1 ? 32 : 64)
#define INODE_DIRBLOCKS(data,blk) INODE_ENDIAN \
                                   (data,blocks.dir_blocks[blk],32,64)
#define INODE_INDIRBLOCKS(data,blk) INODE_ENDIAN \
                                     (data,blocks.indir_blocks[blk],32,64)

/* The blocks on which the superblock can be found.  */
static int sblocklist[] = { 128, 16, 0, 512, -1 };

struct ufs_sblock
{
  uint8_t unused[16];
  /* The offset of the inodes in the cylinder group.  */
  uint32_t inoblk_offs;
  
  uint8_t unused2[4];
  
  /* The start of the cylinder group.  */
  uint32_t cylg_offset;
  
  uint8_t unused3[20];
  
  /* The size of a block in bytes.  */
  int32_t bsize;
  uint8_t unused4[48];
  
  /* The size of filesystem blocks to disk blocks.  */
  uint32_t log2_blksz;
  uint8_t unused5[80];
  
  /* Inodes stored per cylinder group.  */
  uint32_t ino_per_group;
  
  /* The frags per cylinder group.  */
  uint32_t frags_per_group;
  
  uint8_t unused7[1180];
  
  /* Magic value to check if this is really a UFS filesystem.  */
  uint32_t magic;
};

/* UFS inode.  */
struct ufs_inode
{
  uint16_t mode;
  uint16_t nlinks;
  uint16_t uid;
  uint16_t gid;
  int64_t size;
  uint64_t atime;
  uint64_t mtime;
  uint64_t ctime;
  union
  {
    struct
    {
      uint32_t dir_blocks[UFS_DIRBLKS];
      uint32_t indir_blocks[UFS_INDIRBLKS];
    } blocks;
    uint8_t symlink[(UFS_DIRBLKS + UFS_INDIRBLKS) * 4];
  };
  uint32_t flags;
  uint32_t nblocks;
  uint32_t gen;
  uint32_t unused;
  uint8_t pad[12];
};

/* UFS inode.  */
struct ufs2_inode
{
  uint16_t mode;
  uint16_t nlinks;
  uint32_t uid;
  uint32_t gid;
  uint32_t blocksize;
  int64_t size;
  int64_t nblocks;
  uint64_t atime;
  uint64_t mtime;
  uint64_t ctime;
  uint64_t create_time;
  uint32_t atime_sec;
  uint32_t mtime_sec;
  uint32_t ctime_sec;
  uint32_t create_time_sec;
  uint32_t gen;
  uint32_t kernel_flags;
  uint32_t flags;
  uint32_t extsz;
  uint64_t ext[2];
  union
  {
    struct
    {
      uint64_t dir_blocks[UFS_DIRBLKS];
      uint64_t indir_blocks[UFS_INDIRBLKS];
    } blocks;
    uint8_t symlink[(UFS_DIRBLKS + UFS_INDIRBLKS) * 8];
  };

  uint8_t unused[24];
};

/* Directory entry.  */
struct ufs_dirent
{
  uint32_t ino;
  uint16_t direntlen;
  uint8_t filetype;
  uint8_t namelen;
};

/* Information about a "mounted" ufs filesystem.  */
struct ufs_data
{
  struct ufs_sblock sblock;
  grub_disk_t disk;
  union
  {
    struct ufs_inode inode;
    struct ufs2_inode inode2;
  };
  enum
    {
      UFS1,
      UFS2,
      UNKNOWN
    } ufs_type;
  int ino;
  int linknest;
};

/* Forward declaration.  */
static grub_err_t ufs_find_file (struct ufs_data *data,
				      const char *path);


static int
ufs_get_file_block (struct ufs_data *data, unsigned int blk)
{
  struct ufs_sblock *sblock = &data->sblock;
  unsigned int indirsz;
  
  /* Direct.  */
  if (blk < UFS_DIRBLKS)
    return INODE_DIRBLOCKS (data, blk);
  
  blk -= UFS_DIRBLKS;
  
  indirsz = UFS_BLKSZ (sblock) / INODE_BLKSZ (data);
  /* Single indirect block.  */
  if (blk < indirsz)
    {
      uint32_t indir[UFS_BLKSZ (sblock)];
      grub_disk_read (data->disk, INODE_INDIRBLOCKS (data, 0),
		      0, sizeof (indir), (char *) indir);
      return indir[blk];
    }
  blk -= indirsz;
  
  /* Double indirect block.  */
  if (blk < UFS_BLKSZ (sblock) / indirsz)
    {
      uint32_t indir[UFS_BLKSZ (sblock)];
      
      grub_disk_read (data->disk, INODE_INDIRBLOCKS (data, 1),
		      0, sizeof (indir), (char *) indir);
      grub_disk_read (data->disk,  indir[blk / indirsz],
		      0, sizeof (indir), (char *) indir);
      
      return indir[blk % indirsz];
    }


  grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
	      "ufs does not support tripple indirect blocks");
  return 0;
}


/* Read LEN bytes from the file described by DATA starting with byte
   POS.  Return the amount of read bytes in READ.  */
static ssize_t
ufs_read_file (struct ufs_data *data,
		    void (*read_hook) (grub_disk_addr_t sector,
				       unsigned offset, unsigned length),
		    int pos, size_t len, char *buf)
{
  struct ufs_sblock *sblock = &data->sblock;
  int i;
  int blockcnt;

  /* Adjust len so it we can't read past the end of the file.  */
  if (len > INODE_SIZE (data))
    len = INODE_SIZE (data);

  blockcnt = (len + pos + UFS_BLKSZ (sblock) - 1) / UFS_BLKSZ (sblock);
  
  for (i = pos / UFS_BLKSZ (sblock); i < blockcnt; i++)
    {
      int blknr;
      int blockoff = pos % UFS_BLKSZ (sblock);
      int blockend = UFS_BLKSZ (sblock);
      
      int skipfirst = 0;
      
      blknr = ufs_get_file_block (data, i);
      if (grub_errno)
	return -1;
      
      /* Last block.  */
      if (i == blockcnt - 1)
	{
	  blockend = (len + pos) % UFS_BLKSZ (sblock);
	  
	  if (!blockend)
	    blockend = UFS_BLKSZ (sblock);
	}
      
      /* First block.  */
      if (i == (pos / (int) UFS_BLKSZ (sblock)))
	{
	  skipfirst = blockoff;
	  blockend -= skipfirst;
	}
      
      /* XXX: If the block number is 0 this block is not stored on
	 disk but is zero filled instead.  */
      if (blknr)
	{
	  data->disk->read_hook = read_hook;
	  grub_disk_read (data->disk,
			  blknr << __le32_to_cpu (data->sblock.log2_blksz),
			  skipfirst, blockend, buf);
	  data->disk->read_hook = 0;
	  if (grub_errno)
	    return -1;
	}
      else
	memset (buf, UFS_BLKSZ (sblock) - skipfirst, 0);

      buf += UFS_BLKSZ (sblock) - skipfirst;
    }
  
  return len;
}


/* Read inode INO from the mounted filesystem described by DATA.  This
   inode is used by default now.  */
static grub_err_t
ufs_read_inode (struct ufs_data *data, int ino)
{
  struct ufs_sblock *sblock = &data->sblock;
  
  /* Determine the group the inode is in.  */
  int group = ino / __le32_to_cpu (sblock->ino_per_group);
  
  /* Determine the inode within the group.  */
  int grpino = ino % __le32_to_cpu (sblock->ino_per_group);
  
  /* The first block of the group.  */
  int grpblk = group * (__le32_to_cpu (sblock->frags_per_group));
  
  if (data->ufs_type == UFS1)
    {
      struct ufs_inode *inode = &data->inode;
      
      grub_disk_read (data->disk,
		      (((__le32_to_cpu (sblock->inoblk_offs) + grpblk)
			<< __le32_to_cpu (data->sblock.log2_blksz)))
		      + grpino / 4,
		      (grpino % 4) * sizeof (struct ufs_inode),
		      sizeof (struct ufs_inode),
		      (char *) inode);
    }
  else
    {
      struct ufs2_inode *inode = &data->inode2;
      
      grub_disk_read (data->disk,
		      (((__le32_to_cpu (sblock->inoblk_offs) + grpblk)
			<< __le32_to_cpu (data->sblock.log2_blksz)))
		      + grpino / 2,
		      (grpino % 2) * sizeof (struct ufs2_inode),
		      sizeof (struct ufs2_inode),
		      (char *) inode);
    }
  
  data->ino = ino;
  return grub_errno;
}


/* Lookup the symlink the current inode points to.  INO is the inode
   number of the directory the symlink is relative to.  */
static grub_err_t
ufs_lookup_symlink (struct ufs_data *data, int ino)
{
  char symlink[INODE_SIZE (data)];
  
  if (++data->linknest > UFS_MAX_SYMLNK_CNT)
    return grub_error (GRUB_ERR_SYMLINK_LOOP, "too deep nesting of symlinks");
  
  if (INODE_SIZE (data) < (UFS_DIRBLKS + UFS_INDIRBLKS
			  * INODE_BLKSZ (data)))
    strcpy (symlink, (char *) INODE (data, symlink));
  else
    {
      grub_disk_read (data->disk, 
		      (INODE_DIRBLOCKS (data, 0) 
		       << __le32_to_cpu (data->sblock.log2_blksz)),
		      0, INODE_SIZE (data), symlink);
      symlink[INODE_SIZE (data)] = '\0';
    }

  /* The symlink is an absolute path, go back to the root inode.  */
  if (symlink[0] == '/')
    ino = UFS_INODE;
  
  /* Now load in the old inode.  */
  if (ufs_read_inode (data, ino))
    return grub_errno;
  
  ufs_find_file (data, symlink);
  if (grub_errno)
    grub_error (grub_errno, "Can not follow symlink `%s'.", symlink);
  
  return grub_errno;
}


/* Find the file with the pathname PATH on the filesystem described by
   DATA.  */
static grub_err_t
ufs_find_file (struct ufs_data *data, const char *path)
{
  char fpath[strlen (path)];
  char *name = fpath;
  char *next;
  unsigned int pos = 0;
  int dirino;
  
  strncpy (fpath, path, strlen (path));
  
  /* Skip the first slash.  */
  if (name[0] == '/')
    {
      name++;
      if (!*name)
	return 0;
    }

  /* Extract the actual part from the pathname.  */
  next = strchr (name, '/');
  if (next)
    {
      next[0] = '\0';
      next++;
    }
  
  do
    {
      struct ufs_dirent dirent;
      
      if (strlen (name) == 0)
	return GRUB_ERR_NONE;
      
      if (ufs_read_file (data, 0, pos, sizeof (dirent),
			      (char *) &dirent) < 0)
	return grub_errno;
      
      {
	char filename[dirent.namelen + 1];

	if (ufs_read_file (data, 0, pos + sizeof (dirent),
				dirent.namelen, filename) < 0)
	  return grub_errno;
	
	filename[dirent.namelen] = '\0';
	
	if (!strcmp (name, filename))
	  {
	    dirino = data->ino;
	    ufs_read_inode (data, __le32_to_cpu (dirent.ino));
	    
	    if (dirent.filetype == UFS_FILETYPE_LNK)
	      {
		ufs_lookup_symlink (data, dirino);
		if (grub_errno)
		  return grub_errno;
	      }

	    if (!next)
	      return 0;

	    pos = 0;

	    name = next;
	    next = strchr (name, '/');
	    if (next)
	      {
		next[0] = '\0';
		next++;
	      }
	    
	    if (!(dirent.filetype & UFS_FILETYPE_DIR))
	      return grub_error (GRUB_ERR_BAD_FILE_TYPE, "not a directory");
	    
	    continue;
	  }
      }
      
      pos += __le16_to_cpu (dirent.direntlen);
    } while (pos < INODE_SIZE (data));
  
  grub_error (GRUB_ERR_FILE_NOT_FOUND, "file not found");
  return grub_errno;
}


/* Mount the filesystem on the disk DISK.  */
static struct ufs_data *
ufs_mount (grub_disk_t disk)
{
  struct ufs_data *data;
  int *sblklist = sblocklist;
  
  data = malloc (sizeof (struct ufs_data));
  if (!data)
    return 0;
  
  /* Find a UFS1 or UFS2 sblock.  */
  data->ufs_type = UNKNOWN;
  while (*sblklist != -1)
    {
      grub_disk_read (disk, *sblklist, 0, sizeof (struct ufs_sblock),
		      (char *) &data->sblock);
      if (grub_errno)
	goto fail;
      
      if (__le32_to_cpu (data->sblock.magic) == UFS_MAGIC)
	{
	  data->ufs_type = UFS1;
	  break;
	}
      else if (__le32_to_cpu (data->sblock.magic) == UFS2_MAGIC)
	{
	  data->ufs_type = UFS2;
	  break;
	}
      sblklist++;
    }
  if (data->ufs_type == UNKNOWN)
    {
      grub_error (GRUB_ERR_BAD_FS, "not an ufs filesystem");
      goto fail;
    }

  data->disk = disk;
  data->linknest = 0;
  return data;

 fail:
  free (data);
  
  if (grub_errno == GRUB_ERR_OUT_OF_RANGE)
    grub_error (GRUB_ERR_BAD_FS, "not a ufs filesystem");
  
  return 0;
}


static grub_err_t
ufs_dir (grub_device_t device, const char *path, 
	       int (*hook) (const char *filename, int dir))
{
  struct ufs_data *data;
  struct ufs_sblock *sblock;
  unsigned int pos = 0;

  data = ufs_mount (device->disk);
  if (!data)
    return grub_errno;
  
  ufs_read_inode (data, UFS_INODE);
  if (grub_errno)
    return grub_errno;
  
  sblock = &data->sblock;
  
  if (!path || path[0] != '/')
    {
      grub_error (GRUB_ERR_BAD_FILENAME, "bad filename");
      return grub_errno;
    }
  
  ufs_find_file (data, path);
  if (grub_errno)
    goto fail;  
  
  if (!(INODE_MODE (data) & UFS_ATTR_DIR))
    {
      grub_error (GRUB_ERR_BAD_FILE_TYPE, "not a directory");
      goto fail;
    }
  
  while (pos < INODE_SIZE (data))
    {
      struct ufs_dirent dirent;
      
      if (ufs_read_file (data, 0, pos, sizeof (dirent),
			      (char *) &dirent) < 0)
	break;
      
      {
	char filename[dirent.namelen + 1];
	
	if (ufs_read_file (data, 0, pos + sizeof (dirent),
				dirent.namelen, filename) < 0)
	  break;
	
	filename[dirent.namelen] = '\0';
	if (hook (filename, dirent.filetype == UFS_FILETYPE_DIR))
	  break;
      }
      
      pos += __le16_to_cpu (dirent.direntlen);
    }

 fail:
  free (data);

  return grub_errno;
}


/* Open a file named NAME and initialize FILE.  */
static grub_err_t
ufs_open (struct grub_file *file, const char *name)
{
  struct ufs_data *data;
  data = ufs_mount (file->device->disk);
  if (!data)
    return grub_errno;
  
  ufs_read_inode (data, 2);
  if (grub_errno)
    {
      free (data);
      return grub_errno;
    }
    
  if (!name || name[0] != '/')
    {
      grub_error (GRUB_ERR_BAD_FILENAME, "bad filename");
      return grub_errno;
    }
  
  ufs_find_file (data, name);
  if (grub_errno)
    {
      grub_free (data);
      return grub_errno;
    }
  
  file->data = data;
  file->size = INODE_SIZE (data);

  return GRUB_ERR_NONE;
}


static ssize_t
ufs_read (grub_file_t file, char *buf, size_t len)
{
  struct ufs_data *data = 
    (struct ufs_data *) file->data;
  
  return ufs_read_file (data, file->read_hook, file->offset, len, buf);
}


static grub_err_t
ufs_close (grub_file_t file)
{
  free (file->data);
  
  return GRUB_ERR_NONE;
}


static grub_err_t
ufs_label (grub_device_t device __attribute ((unused)),
		char **label __attribute ((unused)))
{
  return GRUB_ERR_NONE;
}


static struct grub_fs ufs_fs =
  {
    .name = "ufs",
    .dir = ufs_dir,
    .open = ufs_open,
    .read = ufs_read,
    .close = ufs_close,
    .label = ufs_label,
    .next = 0
  };

GRUB_MOD_INIT(ufs)
{
  grub_fs_register (&ufs_fs);
#ifndef GRUB_UTIL
  my_mod = mod;
#endif
}

GRUB_MOD_FINI(ufs)
{
  grub_fs_unregister (&ufs_fs);
}

