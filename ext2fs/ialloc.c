/* Inode allocation routines.

   Copyright (C) 1995 Free Software Foundation, Inc.

   Converted to work under the hurd by Miles Bader <miles@gnu.ai.mit.edu>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

/* 
 *  linux/fs/ext2/ialloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  BSD ufs-inspired inode and directory allocation by 
 *  Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
 */

/*
 * The free inodes are managed by bitmaps.  A file system contains several
 * blocks groups.  Each group contains 1 bitmap block for blocks, 1 bitmap
 * block for inodes, N blocks for the inode table and data blocks.
 *
 * The file system contains group descriptors which are located after the
 * super block.  Each descriptor contains the number of the bitmap block and
 * the free blocks count in the block.  The descriptors are loaded in memory
 * when a file system is mounted (see ext2_read_super).
 */

#include "ext2fs.h"

/* ---------------------------------------------------------------- */

/* Free node NP; the on disk copy has already been synced with 
   diskfs_node_update (where NP->dn_stat.st_mode was 0).  It's
   mode used to be OLD_MODE.  */
void
diskfs_free_node (struct node *np, mode_t old_mode)
{
  char *bh;
  unsigned long block_group;
  unsigned long bit;
  struct ext2_group_desc *gdp;
  ino_t inum = np->dn->number;

  assert (!diskfs_readonly);

 printf ("freeing inode %u", inum);

  ext2_debug ("freeing inode %u", inum);

  spin_lock (&global_lock);

  if (inum < EXT2_FIRST_INO || inum > sblock->s_inodes_count)
    {
      ext2_error ("free_inode", "reserved inode or nonexistent inode");
      spin_unlock (&global_lock);
      return;
    }

  block_group = (inum - 1) / sblock->s_inodes_per_group;
  bit = (inum - 1) % sblock->s_inodes_per_group;

  gdp = group_desc (block_group);
  bh = bptr (gdp->bg_inode_bitmap);

  if (!clear_bit (bit, bh))
    ext2_warning ("ext2_free_inode", "bit already cleared for inode %u", inum);
  else
    {
      record_global_poke (bh);

      gdp->bg_free_inodes_count++;
      if (S_ISDIR (old_mode))
	gdp->bg_used_dirs_count--;
      record_global_poke (gdp);

      sblock->s_free_inodes_count++;
    }

  sblock_dirty = 1;
  spin_unlock (&global_lock);
  alloc_sync(0);
}

/* ---------------------------------------------------------------- */

/*
 * There are two policies for allocating an inode.  If the new inode is
 * a directory, then a forward search is made for a block group with both
 * free space and a low directory-to-inode ratio; if that fails, then of
 * the groups with above-average free space, that group with the fewest
 * directories already is chosen.
 *
 * For other inodes, search forward from the parent directory\'s block
 * group to find a free inode.
 */
ino_t
ext2_alloc_inode (ino_t dir_inum, mode_t mode)
{
  char *bh;
  int i, j, inum, avefreei;
  struct ext2_group_desc *gdp;
  struct ext2_group_desc *tmp;

  spin_lock (&global_lock);

repeat:
  gdp = NULL;
  i = 0;

  if (S_ISDIR (mode))
    {
      avefreei = sblock->s_free_inodes_count / groups_count;

/* I am not yet convinced that this next bit is necessary.
      i = inode_group_num(dir_inum);
      for (j = 0; j < groups_count; j++)
	{
	  tmp = group_desc (i);
	  if ((tmp->bg_used_dirs_count << 8) < tmp->bg_free_inodes_count)
	    {
	      gdp = tmp;
	      break;
	    }
	  else
	    i = ++i % groups_count;
	}
 */

      if (!gdp)
	{
	  for (j = 0; j < groups_count; j++)
	    {
	      tmp = group_desc (j);
	      if (tmp->bg_free_inodes_count
		  && tmp->bg_free_inodes_count >= avefreei)
		{
		  if (!gdp ||
		      (tmp->bg_free_blocks_count > gdp->bg_free_blocks_count))
		    {
		      i = j;
		      gdp = tmp;
		    }
		}
	    }
	}
    }
  else
    {
      /*
       * Try to place the inode in its parent directory
       */
      i = inode_group_num(dir_inum);
      tmp = group_desc (i);
      if (tmp->bg_free_inodes_count)
	gdp = tmp;
      else
	{
	  /*
	   * Use a quadratic hash to find a group with a
	   * free inode
	   */
	  for (j = 1; j < groups_count; j <<= 1)
	    {
	      i += j;
	      if (i >= groups_count)
		i -= groups_count;
	      tmp = group_desc (i);
	      if (tmp->bg_free_inodes_count)
		{
		  gdp = tmp;
		  break;
		}
	    }
	}
      if (!gdp)
	{
	  /*
	   * That failed: try linear search for a free inode
	   */
	  i = inode_group_num(dir_inum) + 1;
	  for (j = 2; j < groups_count; j++)
	    {
	      if (++i >= groups_count)
		i = 0;
	      tmp = group_desc (i);
	      if (tmp->bg_free_inodes_count)
		{
		  gdp = tmp;
		  break;
		}
	    }
	}
    }

  if (!gdp)
    {
      spin_unlock (&global_lock);
      return 0;
    }

  bh = bptr (gdp->bg_inode_bitmap);
  if ((inum =
       find_first_zero_bit ((unsigned long *) bh, sblock->s_inodes_per_group))
      < sblock->s_inodes_per_group)
    {
      if (set_bit (inum, bh))
	{
	  ext2_warning ("ext2_new_inode",
			"bit already set for inode %d", inum);
	  goto repeat;
	}
      record_global_poke (bh);
    }
  else
    {
      if (gdp->bg_free_inodes_count != 0)
	{
	  ext2_error ("ext2_new_inode",
		      "Free inodes count corrupted in group %d", i);
	  inum = 0;
	  goto sync_out;
	}
      goto repeat;
    }

  inum += i * sblock->s_inodes_per_group + 1;
  if (inum < EXT2_FIRST_INO || inum > sblock->s_inodes_count)
    {
      ext2_error ("ext2_new_inode",
		  "reserved inode or inode > inodes count - "
		  "block_group = %d,inode=%d", i, inum);
      inum = 0;
      goto sync_out;
    }

  gdp->bg_free_inodes_count--;
  if (S_ISDIR (mode))
    gdp->bg_used_dirs_count++;
  record_global_poke (gdp);

  sblock->s_free_inodes_count--;
  sblock_dirty = 1;

 sync_out:
  spin_unlock (&global_lock);
  alloc_sync (0);

  return inum;
}

/* ---------------------------------------------------------------- */

/* The user must define this function.  Allocate a new node to be of
   mode MODE in locked directory DP (don't actually set the mode or
   modify the dir, that will be done by the caller); the user
   responsible for the request can be identified with CRED.  Set *NP
   to be the newly allocated node.  */
error_t
diskfs_alloc_node (struct node *dir, mode_t mode, struct node **node)
{
  error_t err;
  int sex, block;
  struct node *np;
  ino_t inum;

  assert (!diskfs_readonly);

  inum = ext2_alloc_inode (dir->dn->number, mode);

  if (inum == 0)
    return ENOSPC;

  err = iget (inum, &np);
  if (err)
    return err;

  if (np->dn_stat.st_blocks)
    {
      ext2_warning("diskfs_alloc_node",
		   "Free inode %d had %d blocks", inum, np->dn_stat.st_blocks);
      np->dn_stat.st_blocks = 0;
      np->dn_set_ctime = 1;
    }

  /* Zero out the block pointers in case there's some noise left on disk.  */
  for (block = 0; block < EXT2_N_BLOCKS; block++)
    if (np->dn->info.i_data[block] != 0)
      {
	np->dn->info.i_data[block] = 0;
	np->dn_set_ctime = 1;
      }

  np->dn_stat.st_flags = 0;

  /*
   * Set up a new generation number for this inode.
   */
  spin_lock (&generation_lock);
  sex = diskfs_mtime->seconds;
  if (++next_generation < (u_long)sex)
    next_generation = sex;
  np->dn_stat.st_gen = next_generation;
  spin_unlock (&generation_lock);

  alloc_sync (np);

  *node = np;
  return 0;
}

/* ---------------------------------------------------------------- */

unsigned long 
ext2_count_free_inodes ()
{
#ifdef EXT2FS_DEBUG
  unsigned long desc_count, bitmap_count, x;
  struct ext2_group_desc *gdp;
  int i;

  spin_lock (&global_lock);

  desc_count = 0;
  bitmap_count = 0;
  gdp = NULL;
  for (i = 0; i < groups_count; i++)
    {
      gdp = group_desc (i);
      desc_count += gdp->bg_free_inodes_count;
      x = count_free (bptr (gdp->bg_inode_bitmap),
		      sblock->s_inodes_per_group / 8);
      ext2_debug ("group %d: stored = %d, counted = %lu",
		  i, gdp->bg_free_inodes_count, x);
      bitmap_count += x;
    }
  ext2_debug ("stored = %lu, computed = %lu, %lu",
	      sblock->s_free_inodes_count, desc_count, bitmap_count);
  spin_unlock (&global_lock);
  return desc_count;
#else
  return sblock->s_free_inodes_count;
#endif
}

/* ---------------------------------------------------------------- */

void 
ext2_check_inodes_bitmap ()
{
  int i;
  struct ext2_group_desc *gdp;
  unsigned long desc_count, bitmap_count, x;

  spin_lock (&global_lock);

  desc_count = 0;
  bitmap_count = 0;
  gdp = NULL;
  for (i = 0; i < groups_count; i++)
    {
      gdp = group_desc (i);
      desc_count += gdp->bg_free_inodes_count;
      x = count_free (bptr (gdp->bg_inode_bitmap),
		      sblock->s_inodes_per_group / 8);
      if (gdp->bg_free_inodes_count != x)
	ext2_error ("ext2_check_inodes_bitmap",
		    "Wrong free inodes count in group %d, "
		    "stored = %d, counted = %lu", i,
		    gdp->bg_free_inodes_count, x);
      bitmap_count += x;
    }
  if (sblock->s_free_inodes_count != bitmap_count)
    ext2_error ("ext2_check_inodes_bitmap",
		"Wrong free inodes count in super block, "
		"stored = %lu, counted = %lu",
		(unsigned long) sblock->s_free_inodes_count, bitmap_count);

  spin_unlock (&global_lock);
}
