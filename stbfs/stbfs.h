// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 1998-2020 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2020 Stony Brook University
 * Copyright (c) 2003-2020 The Research Foundation of SUNY
 */

#ifndef _STBFS_H_
#define _STBFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>

/* the file system name */
#define STBFS_NAME "stbfs"

/* stbfs root inode number */
#define STBFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* operations vectors defined in specific files */
extern const struct file_operations stbfs_main_fops;
extern const struct file_operations stbfs_dir_fops;
extern const struct inode_operations stbfs_main_iops;
extern const struct inode_operations stbfs_dir_iops;
extern const struct inode_operations stbfs_symlink_iops;
extern const struct super_operations stbfs_sops;
extern const struct dentry_operations stbfs_dops;
extern const struct address_space_operations stbfs_aops, stbfs_dummy_aops;
extern const struct vm_operations_struct stbfs_vm_ops;
extern const struct export_operations stbfs_export_ops;
extern const struct xattr_handler *stbfs_xattr_handlers[];

extern int stbfs_init_inode_cache(void);
extern void stbfs_destroy_inode_cache(void);
extern int stbfs_init_dentry_cache(void);
extern void stbfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *stbfs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags);
extern struct inode *stbfs_iget(struct super_block *sb,
				 struct inode *lower_inode);
extern int stbfs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path);

/* file private data */
struct stbfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

/* stbfs inode data in memory */
struct stbfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

/* stbfs dentry data in memory */
struct stbfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};

/* stbfs super-block data in memory */
struct stbfs_sb_info {
	struct super_block *lower_sb;
	char *root_path;
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * stbfs_inode_info structure, STBFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct stbfs_inode_info *STBFS_I(const struct inode *inode)
{
	return container_of(inode, struct stbfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define STBFS_D(dent) ((struct stbfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define STBFS_SB(super) ((struct stbfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define STBFS_F(file) ((struct stbfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *stbfs_lower_file(const struct file *f)
{
	return STBFS_F(f)->lower_file;
}

static inline void stbfs_set_lower_file(struct file *f, struct file *val)
{
	STBFS_F(f)->lower_file = val;
}

/* inode to lower inode. */
static inline struct inode *stbfs_lower_inode(const struct inode *i)
{
	return STBFS_I(i)->lower_inode;
}

static inline void stbfs_set_lower_inode(struct inode *i, struct inode *val)
{
	STBFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *stbfs_lower_super(
	const struct super_block *sb)
{
	return STBFS_SB(sb)->lower_sb;
}

static inline void stbfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{
	STBFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void stbfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&STBFS_D(dent)->lock);
	pathcpy(lower_path, &STBFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&STBFS_D(dent)->lock);
	return;
}
static inline void stbfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
	return;
}
static inline void stbfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&STBFS_D(dent)->lock);
	pathcpy(&STBFS_D(dent)->lower_path, lower_path);
	spin_unlock(&STBFS_D(dent)->lock);
	return;
}
static inline void stbfs_reset_lower_path(const struct dentry *dent)
{
	spin_lock(&STBFS_D(dent)->lock);
	STBFS_D(dent)->lower_path.dentry = NULL;
	STBFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&STBFS_D(dent)->lock);
	return;
}
static inline void stbfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	spin_lock(&STBFS_D(dent)->lock);
	pathcpy(&lower_path, &STBFS_D(dent)->lower_path);
	STBFS_D(dent)->lower_path.dentry = NULL;
	STBFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&STBFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	inode_lock_nested(d_inode(dir), I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	inode_unlock(d_inode(dir));
	dput(dir);
}
#endif	/* not _STBFS_H_ */
