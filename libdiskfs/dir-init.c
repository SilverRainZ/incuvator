/* 
   Copyright (C) 1994 Free Software Foundation

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

#include "priv.h"

/* Locked node DP is a new directory; add whatever links are necessary
   to give it structure; its parent is the (locked) node PDP. 
   This routine may not call diskfs_lookup on PDP.  The new directory
   must be clear within the meaning of diskfs_dirempty.  */
error_t
diskfs_init_dir (struct node *dp, struct node *pdp)
{
  struct dirstat *ds = alloca (diskfs_dirstat_size);
  struct node *foo;
  error_t err;
  
  /* New links */
  if (pdp->dn_stat.st_nlink == diskfs_link_max - 1)
    return EMLINK;

  dp->dn_stat.st_nlink++;	/* for `.' */
  dp->dn_set_ctime = 1;
  err = diskfs_lookup (dp, ".", CREATE, &foo, ds, cred);
  assert (err == ENOENT);
  err = diskfs_direnter (dp, ".", dp, ds, cred);
  if (err)
    {
      np->dn_stat.st_nlink--;
      np->dn_set_ctime = 1;
      return err;
    }

  pdp->dn_stat.st_nlink++;	/* for `..' */
  pdp->dn_set_ctime = 1;
  err = diskfs_lookup (np, "..", CREATE, &foo, ds, cred);
  assert (err == ENOENT);
  err = diskfs_direnter (np, "..", pdp, ds, cred);
  if (err)
    {
      pdp->dn_stat.st_nlink--;
      pdp->dn_set_ctime = 1;
      return err;
    }
  diskfs_node_update (dp, 1);
  return 0;
}
