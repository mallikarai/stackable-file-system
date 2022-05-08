// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 1998-2020 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2020 Stony Brook University
 * Copyright (c) 2003-2020 The Research Foundation of SUNY
 */

#include "stbfs.h"

/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int stbfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct path lower_path;
	struct dentry *lower_dentry;
	int err = 1;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	stbfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!(lower_dentry->d_flags & DCACHE_OP_REVALIDATE))
		goto out;
	err = lower_dentry->d_op->d_revalidate(lower_dentry, flags);
out:
	stbfs_put_lower_path(dentry, &lower_path);
	return err;
}

static void stbfs_d_release(struct dentry *dentry)
{
	/* release and reset the lower paths */
	stbfs_put_reset_lower_path(dentry);
	free_dentry_private_data(dentry);
	return;
}

const struct dentry_operations stbfs_dops = {
	.d_revalidate	= stbfs_d_revalidate,
	.d_release	= stbfs_d_release,
};
