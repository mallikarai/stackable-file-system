// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 1998-2020 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2020 Stony Brook University
 * Copyright (c) 2003-2020 The Research Foundation of SUNY
 */

#include "stbfs.h"
#include "file_utils.h"
#include <linux/fs_struct.h>

struct dir_context *global_ctx; 

static ssize_t stbfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = stbfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));

	return err;
}

static ssize_t stbfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err;

	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	
	if(is_file_in_stb(file->f_path.dentry->d_parent)){
		printk("Cannot open file %s in .stb trash directory\n", file->f_path.dentry->d_name.name);
		err = -EPERM;
		goto out_err;
    }

	lower_file = stbfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(d_inode(dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry),
					file_inode(lower_file));
	}

	out_err:
		return err;
}


static int context_builder(struct dir_context *ctx,const char *name, int len, loff_t off, u64 x, unsigned int d_type ){
	char *file_uid=NULL, *search_file = NULL;
	char *extn=NULL;
	uid_t user_id = current_uid().val;
	int i, count=0, start = 0, position = 0, ret = 0;
	char *str_user_id = NULL; 

	// printk("Len of file : %ld and  len : %d\n", strlen(name), len);
	if (user_id == 0)
		return (*global_ctx->actor)(global_ctx, name, len, off, x, d_type); 

	if(len < 3){
		printk("File size less < 3\n");
		goto list;
	}

	search_file = kmalloc(256, GFP_KERNEL);
	if (search_file==NULL){
		printk("Error in search_file alloc \n");
		ret = -ENOMEM;
		goto list;
	}
	strncpy(search_file, name, len);
	search_file[len] = '\0';
	printk("search file name: %s \n", search_file);


	extn = strstr(search_file, ".enc");
    if (!extn){
		position = len;
        printk(".enc extn not found. position  : %d\n", position);
	}
	else{
		position  = extn - search_file;
		printk("position of .enc %d\n",position);
	}
		
	for (i=0; i < len; i++){
		if (search_file[i] == '-'){
			count++;
			if (count == 7){
				start = i+1;
				break;
			}
		}
	}

	printk("Start position of uid is : %d\n", start);
	if (count < 7){
		printk("Cannot validate file without user id\n");
		ret = 0; 
		goto list;
	} 
	
	file_uid = kmalloc(256, GFP_KERNEL);
	if (file_uid==NULL){
		printk("Error in file_uid alloc \n");
		ret = -ENOMEM;
		goto list;
	}
    
    strncpy(file_uid, search_file + start, position-start);
	file_uid[position-start] = '\0';

	printk("file_user_id = %s \n", file_uid);
	
	str_user_id = kmalloc(256, GFP_KERNEL);
	if (str_user_id==NULL){
		printk("Error in str_user_id alloc \n");
		ret = -ENOMEM;
		goto list;
	}

	sprintf(str_user_id, "%d", user_id);

	if(strcmp(str_user_id, file_uid) == 0 ){
		printk("user id %d has access to file: %s with id : %s\n", user_id, search_file, file_uid);
		ret = (*global_ctx->actor)(global_ctx, name, len, off, x, d_type); 
	 }   
	else
		ret = 0;
		
	list:
	if (file_uid!=NULL)
		kfree(file_uid);
	if (str_user_id!=NULL)
		kfree(str_user_id);
	if (search_file!=NULL)
		kfree(search_file);

	return ret;
}


static int stbfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;

	struct dir_context* new_ctx; 
    struct dir_context new_dir = {.actor = &context_builder, .pos = ctx->pos};
    new_ctx = &new_dir;
    global_ctx = ctx;

	lower_file = stbfs_lower_file(file);
	printk("dir  : %s \n",lower_file->f_path.dentry->d_name.name);

	if(strcmp(lower_file->f_path.dentry->d_name.name, ".stb") == 0)
    {
		printk("We are listing files inside .stb\n");
		err = iterate_dir(lower_file, new_ctx);
    }
    else{
		printk("Listing files inside non-stb dir\n");
		err = iterate_dir(lower_file, ctx);
    }

	file->f_pos = lower_file->f_pos;
	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));
	return err;
}

static int stbfs_undelete(struct inode *dir, struct dentry *dentry){ 
	int err = 0, i=0, count = 0, start = 0, position = 0;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode = stbfs_lower_inode(dir);
	struct dentry *lower_dir_dentry;
	struct path lower_path, pwd, root;
	char *new_file = NULL, *filename=NULL, *tmp = NULL, *old_path = NULL, *path_buf = NULL;
	char *cwd, *buf = NULL , *extn=NULL;

	printk("Inside stbfs undelete\n");
	stbfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	if (lower_dentry->d_parent != lower_dir_dentry || d_unhashed(lower_dentry)) {
		err = -EINVAL;
		goto out;
	}

	path_buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (path_buf == NULL){
		printk("Error path_buf\n");
		err = -ENOMEM;
		goto out;
	}

	printk("before old path\n");
	old_path = d_path(&lower_path, path_buf, PATH_MAX);
	if (old_path == NULL){ 
		printk("Error old path\n");
		err = -ENOMEM;
		goto out;
	}

	printk("OLD PATH : %s\n", old_path);


	pwd = current->fs->pwd;
	path_get(&pwd);
	root=  current->fs->root;
	path_get(&root);
	buf = kmalloc(PATH_MAX,GFP_KERNEL);
	cwd = d_path(&pwd,buf,PATH_MAX);
	printk("Hello,the current working directory is %s\n",cwd);

	tmp = kmalloc(256, GFP_KERNEL);
	if (tmp==NULL){
		printk("Error tmp name \n");
		err = -ENOMEM;
		goto out;
	}

	strcpy(tmp, dentry->d_name.name); 
	printk("dentry name %s\n",tmp);

	extn = strstr(tmp, ".enc");
    if (!extn){
		position = strlen(tmp);
        printk(".enc extn not found. position  : %d\n", position);
	}
	else{
		position  = extn - tmp;
		printk("position of .enc %d\n",position);
	}


	for (i=0; i < strlen(tmp); i++){
		if (tmp[i] == '-'){
			count++;
			if (count == 6){
				start = i+1;
				break;
			}
		}
	}

	filename = kmalloc(256, GFP_KERNEL);
	if (filename==NULL){
		printk("Error file name \n");
		err = -ENOMEM;
		goto out;
	}

	strncpy(filename, tmp + start, position-start);
	filename[position-start] = '\0';
	printk("new file name : %s\n", filename); 
	printk("Len of filename : %ld\n", strlen(filename));

	new_file = kmalloc(PATH_MAX, GFP_KERNEL);
	if (new_file==NULL){
		printk("Error new file\n");
		err = -ENOMEM;
		goto out;
	}

	sprintf(new_file, "%s/%s", cwd, filename);
	printk("new file in cwd %s\n", new_file);

	err = file_read_write_helper(old_path, new_file, 2);
	if(err)
		goto out; 

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);
		
		
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;

	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(d_inode(dentry),
	stbfs_lower_inode(d_inode(dentry))->i_nlink);
	d_inode(dentry)->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */

	out:  
		if (buf!=NULL){
			kfree(buf);
		}
		if (path_buf!=NULL){
			kfree(path_buf);
		}  
		if(tmp!=NULL){
			kfree(tmp);
		}
		if(filename!=NULL){
			kfree(filename);
		}
		if(new_file!=NULL){
			kfree(new_file);
		}
						
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	stbfs_put_lower_path(dentry, &lower_path);
	return err;
}


static long stbfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	struct inode *inode = file->f_path.dentry->d_parent->d_inode;
	struct dentry *dentry = file->f_path.dentry;

    lower_file = stbfs_lower_file(file);
    printk("I am in the stbfs_unlocked\n");

	err = stbfs_undelete(inode, dentry);

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err)
		fsstack_copy_attr_all(file_inode(file),
				      file_inode(lower_file));

	return err;
}

#ifdef CONFIG_COMPAT
static long stbfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	lower_file = stbfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int stbfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = stbfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "stbfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!STBFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "stbfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &stbfs_vm_ops;

	file->f_mapping->a_ops = &stbfs_aops; /* set our aops */
	if (!STBFS_F(file)->lower_vm_ops) /* save for our ->fault */
		STBFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int stbfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;
	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct stbfs_file_info), GFP_KERNEL);
	if (!STBFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link stbfs's file struct to lower's */
	stbfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = stbfs_lower_file(file);
		if (lower_file) {
			stbfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		stbfs_set_lower_file(file, lower_file);
	}

	if (err)
		kfree(STBFS_F(file));
	else
		fsstack_copy_attr_all(inode, stbfs_lower_inode(inode));
	
	out_err:
		return err;
}

static int stbfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = stbfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int stbfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;

	lower_file = stbfs_lower_file(file);
	if (lower_file) {
		stbfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	kfree(STBFS_F(file));
	return 0;
}

static int stbfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = stbfs_lower_file(file);
	stbfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	stbfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int stbfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = stbfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * Wrapfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t stbfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = stbfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Wrapfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
stbfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = stbfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(d_inode(file->f_path.dentry),
					file_inode(lower_file));
out:
	return err;
}

/*
 * Wrapfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
stbfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = stbfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry),
					file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations stbfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= stbfs_read,
	.write		= stbfs_write,
	.unlocked_ioctl	= stbfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= stbfs_compat_ioctl,
#endif
	.mmap		= stbfs_mmap,
	.open		= stbfs_open,
	.flush		= stbfs_flush,
	.release	= stbfs_file_release,
	.fsync		= stbfs_fsync,
	.fasync		= stbfs_fasync,
	.read_iter	= stbfs_read_iter,
	.write_iter	= stbfs_write_iter,
};

/* trimmed directory options */
const struct file_operations stbfs_dir_fops = {
	.llseek		= stbfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= stbfs_readdir,
	.unlocked_ioctl	= stbfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= stbfs_compat_ioctl,
#endif
	.open		= stbfs_open,
	.release	= stbfs_file_release,
	.flush		= stbfs_flush,
	.fsync		= stbfs_fsync,
	.fasync		= stbfs_fasync,
};
