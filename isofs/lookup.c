/*
   Copyright (C) 1997, 1998 Free Software Foundation, Inc.
   Written by Thomas Bushnell, n/BSG.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <string.h>
#include <dirent.h>
#include "isofs.h"

/* Forward */
static error_t
dirscanblock (void *, char *, size_t, struct dirrect **, struct rrip_lookup *);

static int
isonamematch (char *dirname, size_t dnamelen, char *username, size_t unamelen)
{
  /* Special representations for `.' and `..' */
  if (dnamelen == 1 && dirname[0] == '\0')
    return unamelen == 1 && username[0] == '.';

  if (dnamelen == 1 && dirname[0] == '\1')
    return unamelen == 2 && username[0] == '.' && username[1] == '.';

  if (unamelen > dnamelen)
    return 0;

  if (!strncasecmp (dirname, username, unamelen))
    {
      /* A prefix has matched.  Check if it's acceptable. */
      if (dnamelen == unamelen)
	return 1;

      /* User has ommitted the version number */
      if (dirname[unamelen] == ';')
	return 1;

      /* User has ommitted an empty extension */
      if (dirname[unamelen] == '.'
	  && (dirname[unamelen+1] == '\0' || dirname[unamelen+1] == ';'))
	return 1;
    }

  return 0;
}

/* Implement the diskfs_lookup callback from the diskfs library.  See
   <hurd/diskfs.h> for the interface specification. */
error_t
diskfs_lookup_hard (struct node *dp, const char *name, enum lookup_type type,
		    struct node **npp, struct dirstat *ds, struct protid *cred)
{
  error_t err = 0;
  struct dirrect *record;
  int namelen;
  int spec_dotdot;
  void *buf;
  void *blockaddr;
  struct rrip_lookup rr;

  if ((type == REMOVE) || (type == RENAME))
    assert (npp);

  if (npp)
    *npp = 0;

  spec_dotdot = type & SPEC_DOTDOT;
  type &= ~SPEC_DOTDOT;

  namelen = strlen (name);

  /* This error is constant, but the errors for CREATE and REMOVE depend
     on whether NAME exists. */
  if (type == RENAME)
    return EROFS;

  buf = disk_image + (dp->dn->file_start << store->log2_block_size);

  for (blockaddr = buf;
       blockaddr < buf + dp->dn_stat.st_size;
       blockaddr += logical_sector_size)
    {
      err = dirscanblock (blockaddr, name, namelen, &record, &rr);

      if (!err)
	break;

      if (err != ENOENT)
	return err;
    }

  if ((!err && type == REMOVE)
      || (err == ENOENT && type == CREATE))
    err = EROFS;

  if (err)
    return err;

  /* Load the inode */
  if (namelen == 2 && name[0] == '.' && name[1] == '.')
    {
      if (dp == diskfs_root_node)
	err = EAGAIN;
      else if (spec_dotdot)
	{
	  /* renames and removes can't get this far. */
	  assert (type == LOOKUP);
	  diskfs_nput (dp);
	  err = load_inode (npp, record, &rr);
	}
      else
	{
	  /* We don't have to do the normal rigamarole, because
	     we are permanently read-only, so things are necessarily
	     quiescent.  Just be careful to honor the locking order. */
	  mutex_unlock (&dp->lock);
	  err = load_inode (npp, record, &rr);
	  mutex_lock (&dp->lock);
	}
    }
  else if (namelen == 1 && name[0] == '.')
    {
      *npp = dp;
      diskfs_nref (dp);
    }
  else
    err = load_inode (npp, record, &rr);

  release_rrip (&rr);
  return err;
}


/* Scan one logical sector of directory contents (at address BLKADDR)
   for NAME of length NAMELEN.  Return its address in *RECORD. */
static error_t
dirscanblock (void *blkaddr, char *name, size_t namelen,
	      struct dirrect **record, struct rrip_lookup *rr)
{
  struct dirrect *entry;
  void *currentoff;
  size_t reclen;
  size_t entry_namelen;
  int matchrr;
  int matchnormal;

  for (currentoff = blkaddr;
       currentoff < blkaddr + logical_sector_size;
       currentoff += reclen)
    {
      entry = (struct dirrect *) currentoff;

      reclen = entry->len;

      /* Validate reclen */
      if (reclen == 0
	  || reclen < sizeof (struct dirrect)
	  || currentoff + reclen > blkaddr + logical_sector_size)
	break;

      entry_namelen = entry->namelen;

      /* More validation */
      if (reclen < sizeof (struct dirrect) + entry_namelen)
	break;

      /* Check to see if the name maches the directory entry. */
      if (isonamematch (entry->name, entry_namelen, name, namelen))
	matchnormal = 1;
      else
	matchnormal = 0;

      /* Now scan for RRIP fields */
      matchrr = rrip_match_lookup (entry, name, namelen, rr);

      /* Ignore RE entries */
      if (rr->valid & VALID_RE)
	{
	  release_rrip (rr);
	  continue;
	}

      /* See if the name matches */
      if (((rr->valid & VALID_NM) && matchrr)
	  || (!(rr->valid & VALID_NM) && matchnormal))
	{
	  /* We've got it.  Return success */
	  *record = entry;
	  return 0;
	}

      release_rrip (rr);
    }

  /* Wasn't there. */
  *record = 0;
  return ENOENT;
}

error_t
diskfs_get_directs (struct node *dp,
		    int entry,
		    int nentries,
		    char **data,
		    u_int *datacnt,
		    vm_size_t bufsiz,
		    int *amt)
{
  volatile vm_size_t allocsize;
  struct dirrect *ep;
  struct dirent *userp;
  int i;
  void *dirbuf, *bufp;
  char *datap;
  volatile int ouralloc = 0;
  error_t err;

  /* Allocate some space to hold the returned data. */
  allocsize = bufsiz ? round_page (bufsiz) : vm_page_size * 4;
  if (allocsize > *datacnt)
    {
      vm_allocate (mach_task_self (), (vm_address_t *) data, allocsize, 1);
      ouralloc = 1;
    }

  err = diskfs_catch_exception ();
  if (err)
    {
      if (ouralloc)
	vm_deallocate (mach_task_self (), (vm_address_t)*data, allocsize);
      return err;
    }

  /* Skip to ENTRY */
  dirbuf = disk_image + (dp->dn->file_start << store->log2_block_size);
  bufp = dirbuf;
  for (i = 0; i < entry; i ++)
    {
      struct rrip_lookup rr;

      ep = (struct dirrect *) bufp;
      rrip_lookup (ep, &rr, 0);

      /* Ignore and skip RE entries */
      if (rr.valid & VALID_RE)
	{
	  bufp = bufp + ep->len;
	  release_rrip (&rr);
	  continue;
	}

      if (bufp - dirbuf >= dp->dn_stat.st_size)
	{
	  /* Not that many entries in the directory; return nothing. */
	  if (allocsize > *datacnt)
	    vm_deallocate (mach_task_self (), (vm_address_t) data, allocsize);
	  *datacnt = 0;
	  *amt = 0;
	  return 0;
	}
      bufp = bufp + ep->len;

      /* If BUFP points at a null, then we have hit the last
	 record in this logical sector.  In that case, skip up to
	 the next logical sector. */
      if (*(char *)bufp == '\0')
	bufp = (void *) (((long) bufp & ~(logical_sector_size - 1))
			 + logical_sector_size);
    }

  /* Now copy entries one at a time */
  i = 0;
  datap = *data;
  while (((nentries == -1) || (i < nentries))
	 && (!bufsiz || datap - *data < bufsiz)
	 && ((void *) bufp - dirbuf < dp->dn_stat.st_size))
    {
      struct rrip_lookup rr;
      const char *name;
      size_t namlen, reclen;
      off_t file_start;

      ep = (struct dirrect *) bufp;

      /* Fetch Rock-Ridge information for this file */
      rrip_lookup (ep, &rr, 0);

      /* Ignore and skip RE entries */
      if (rr.valid & VALID_RE)
	{
	  bufp = bufp + ep->len;
	  release_rrip (&rr);
	  continue;
	}

      /* See if there's room to hold this one */
      name = rr.valid & VALID_NM ? rr.name : ep->name;
      namlen = rr.valid & VALID_NM ? strlen (name) : ep->namelen;

      /* Name frobnication */
      if (!(rr.valid & VALID_NM))
	{
	  if (namlen == 1 && name[0] == '\0')
	    {
	      name = ".";
	      namlen = 1;
	    }
	  else if (namlen == 1 && name[0] == '\1')
	    {
	      name = "..";
	      namlen = 2;
	    }
	  /* Perhaps downcase it too? */
	}

      reclen = sizeof (struct dirent) + namlen;
      reclen = (reclen + 3) & ~3;

      /* Expand buffer if necessary */
      if (datap - *data + reclen > allocsize)
	{
	  vm_address_t newdata;

	  vm_allocate (mach_task_self (), &newdata,
		       (ouralloc
			? (allocsize *= 2)
			: (allocsize = vm_page_size * 2)), 1);
	  bcopy ((void *) *data, (void *)newdata, datap - *data);

	  if (ouralloc)
	    vm_deallocate (mach_task_self (), (vm_address_t) *data,
			   allocsize / 2);

 	  datap = (char *) newdata + (datap - *data);
	  *data = (char *) newdata;
	  ouralloc = 1;
	}

      userp = (struct dirent *) datap;

      /* Fill in entry */

      err = calculate_file_start (ep, &file_start, &rr);
      if (err)
	{
	  diskfs_end_catch_exception ();
	  if (ouralloc)
	    vm_deallocate (mach_task_self (), (vm_address_t) *data, allocsize);
	  return err;
	}

      userp->d_fileno = file_start;
      userp->d_type = DT_UNKNOWN;
      userp->d_reclen = reclen;
      userp->d_namlen = namlen;
      bcopy (name, userp->d_name, namlen);
      userp->d_name[namlen] = '\0';

      /* And move along */
      release_rrip (&rr);
      datap = datap + reclen;
      bufp = bufp + ep->len;
      i++;

      /* If BUFP points at a null, then we have hit the last
	 record in this logical sector.  In that case, skip up to
	 the next logical sector. */
      if (*(char *)bufp == '\0')
	bufp = (void *) (((long) bufp & ~(logical_sector_size - 1))
			 + logical_sector_size);
    }

  diskfs_end_catch_exception ();

  /* If we didn't use all the pages of a buffer we allocated, free
     the excess.  */
  if (ouralloc
      && round_page (datap - *data) < round_page (allocsize))
    vm_deallocate (mach_task_self (), round_page (datap),
		   round_page (allocsize) - round_page (datap - *data));

  /* Return */
  *amt = i;
  *datacnt = datap - *data;
  return 0;
}

/* We have no dirstats at all. */
size_t diskfs_dirstat_size = 0;

void
diskfs_null_dirstat (struct dirstat *ds)
{
}

error_t
diskfs_drop_dirstat (struct node *dp, struct dirstat *ds)
{
  return 0;
}

/* These should never be called. */

error_t
diskfs_direnter_hard(struct node *dp,
		     const char *name,
		     struct node *np,
		     struct dirstat *ds,
		     struct protid *cred)
{
  abort ();
}

error_t
diskfs_dirremove_hard(struct node *dp,
		      struct dirstat *ds)
{
  abort ();
}

error_t
diskfs_dirrewrite_hard(struct node *dp,
		       struct node *np,
		       struct dirstat *ds)
{
  abort ();
}

int
diskfs_dirempty(struct node *dp,
		struct protid *cred)
{
  abort ();
}
