// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 1998-2020 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2020 Stony Brook University
 * Copyright (c) 2003-2020 The Research Foundation of SUNY
 */

#include "stbfs.h"
#include <linux/module.h>

/*
 * There is no need to lock the stbfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
char *enc_key; 

static int stbfs_read_super(struct super_block *sb, void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb; 
	struct path lower_path;
	char *dev_name = (char *) raw_data;
	struct inode *inode;
	struct stbfs_sb_info *sbinfo;
	
	if (!dev_name) {
		printk(KERN_ERR
		       "stbfs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		printk(KERN_ERR	"stbfs: error accessing "
		       "lower directory '%s'\n", dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct stbfs_sb_info), GFP_KERNEL);
	sbinfo = STBFS_SB(sb);
	sbinfo->root_path = kmalloc(PATH_MAX, GFP_KERNEL);
	strcpy(sbinfo->root_path, dev_name);
	printk("ROOT PATH in main : %s\n", sbinfo->root_path);
	if (!STBFS_SB(sb)) {
		printk(KERN_CRIT "stbfs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out_free;
	}

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	stbfs_set_lower_super(sb, lower_sb);

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &stbfs_sops;
	sb->s_xattr = stbfs_xattr_handlers;

	sb->s_export_op = &stbfs_export_ops; /* adding NFS support */

	/* get a new inode and allocate our root dentry */
	inode = stbfs_iget(sb, d_inode(lower_path.dentry));
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &stbfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	stbfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		printk(KERN_INFO
		       "stbfs: mounted on top of %s type %s\n",
		       dev_name, lower_sb->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	kfree(sbinfo->root_path);
	atomic_dec(&lower_sb->s_active);
	kfree(STBFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);

out:
	return err;
}

struct dentry *stbfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	void *lower_path_name = (void *) dev_name;
	if (raw_data == NULL)
		printk("No enc_key\n");
	else{
		enc_key  = kmalloc(50, GFP_KERNEL);
		strcpy(enc_key,raw_data+4);
		printk("Raw data enc key : %s",enc_key);
	}
	return mount_nodev(fs_type, flags, lower_path_name,
			   stbfs_read_super);
}

static void exit_fs(struct super_block *sb){
	
	struct stbfs_sb_info *sbinfo;
	printk("Freeing memory before exiting stbfs file system\n");

	if (enc_key!=NULL)
		kfree(enc_key);
	
	sbinfo = STBFS_SB(sb);
	if (sbinfo->root_path)
		kfree(sbinfo->root_path);

	generic_shutdown_super(sb);
}


static struct file_system_type stbfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= STBFS_NAME,
	.mount		= stbfs_mount,
	.kill_sb	= exit_fs,
	.fs_flags	= 0,
};
MODULE_ALIAS_FS(STBFS_NAME);

static int __init init_stbfs_fs(void)
{
	int err;

	pr_info("Registering stbfs " STBFS_VERSION "\n");

	err = stbfs_init_inode_cache();
	if (err)
		goto out;
	err = stbfs_init_dentry_cache();
	if (err)
		goto out;
	err = register_filesystem(&stbfs_fs_type);
out:
	if (err) {
		stbfs_destroy_inode_cache();
		stbfs_destroy_dentry_cache();
	}
	return err;
}

static void __exit exit_stbfs_fs(void)
{
	stbfs_destroy_inode_cache();
	stbfs_destroy_dentry_cache();
	unregister_filesystem(&stbfs_fs_type);
	pr_info("Completed stbfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu/)");
MODULE_DESCRIPTION("Wrapfs " STBFS_VERSION
		   " (http://stbfs.filesystems.org/)");
MODULE_LICENSE("GPL");

module_init(init_stbfs_fs);
module_exit(exit_stbfs_fs);
