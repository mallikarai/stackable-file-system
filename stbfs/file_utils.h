#ifndef FILE_UTILS_H  
#define FILE_UTILS_H    /* prevents the file from being included twice. */

#include <crypto/skcipher.h>
#include <linux/scatterlist.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/crypto.h>

extern char *enc_key; 

struct skcipher_def {
    struct scatterlist sg;
    struct crypto_skcipher *tfm;
    struct skcipher_request *req;
    struct crypto_wait wait;
};


static int encypt_decrypt_func(char *key, char *data, int bytes, int flag, char *ivdata){
    int err = 0; 
    struct skcipher_def sk;
    struct crypto_skcipher *skcipher = NULL;
    struct skcipher_request *req = NULL;
    
    printk("Encryption key: %s\n", enc_key);
    printk("inside encypt_decrypt_func\n");
    skcipher = crypto_alloc_skcipher("ctr(aes)", 0, 0);
    if (IS_ERR(skcipher)) {
        printk("could not allocate skcipher handle\n");
        return PTR_ERR(skcipher);
    }

    req = skcipher_request_alloc(skcipher, GFP_KERNEL);
    if (!req) {
        printk("could not allocate skcipher request\n");
        err = -ENOMEM;
        goto out;
    }

    skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
        crypto_req_done,&sk.wait);


    if (crypto_skcipher_setkey(skcipher, key, 32)) {
        printk("key could not be set\n");
        err = -EAGAIN;
        goto out;
    }

    sk.tfm = skcipher;
    sk.req = req;

    /* We encrypt one block */
    sg_init_one(&sk.sg, data, bytes);
    skcipher_request_set_crypt(req, &sk.sg, &sk.sg, bytes, ivdata);
    crypto_init_wait(&sk.wait);

    /* encrypt data */
    if (flag ==1){
        err = crypto_wait_req(crypto_skcipher_encrypt(sk.req), &sk.wait);
        printk("Encryption triggered successfully\n");
    }
    else if (flag ==2){
        err  = crypto_wait_req(crypto_skcipher_decrypt(sk.req), &sk.wait);
        printk("Decryption triggered successfully\n");
    }
     
    if (err)
        goto out;  

    out:
        if (skcipher)
            crypto_free_skcipher(skcipher);
        if (req)
            skcipher_request_free(req);
        return err;

}

static inline int clear_file_bytes(char *old_file){
    int err = 0, bytes_left=0, buf_size = 0;
    ssize_t write_bytes = 0;
    char *read_buf = NULL;
    struct file *old_filp = NULL; 

    printk("inside clear_file_bytes method \n");
    printk("Old file %s\n", old_file);
    
    old_filp = filp_open(old_file, O_WRONLY, 0775);
    if (old_filp == NULL || IS_ERR(old_filp)) {
        printk("error occurred while opening user output file\n");
        err =  (int) PTR_ERR(old_filp);
        goto out;
    }

    old_filp->f_pos = 0;
    bytes_left = old_filp->f_inode->i_size;
    printk("%d\n", bytes_left); 

    if (bytes_left >= PAGE_SIZE)
        buf_size = PAGE_SIZE;
    else 
        buf_size = bytes_left;
        
    read_buf = kmalloc(buf_size, GFP_KERNEL);
    if (read_buf == NULL) { 
        err = -ENOMEM;
        goto out;
    }

    memset(read_buf, 0, buf_size);

    while(bytes_left >0){
        printk("Looping through file to write empty bytes\n");
        write_bytes = kernel_write(old_filp, read_buf, buf_size, &(old_filp->f_pos));
        if (write_bytes <0){
            printk("error occured while writing bytes to file.\n");
            err  = -EIO;
            goto out;
        }
        bytes_left = bytes_left-write_bytes;
        printk("bytes_left: %d\n",bytes_left);
    }
    printk("Successfully cleared bytes in file\n");

    out: 
        if (read_buf!=NULL){
            kfree(read_buf);
        }
        if(old_filp!=NULL && !IS_ERR(old_filp)){
            filp_close(old_filp, NULL);
        }

        return err; 
}


static inline int file_read_write_helper(char *old_file, char *new_file, int edc_flag){

    int err = 0, bytes_left=0;
    ssize_t read_bytes = 0, write_bytes = 0;
    char *read_buf = NULL;
    struct file *new_filp = NULL, *old_filp = NULL; 
    char *ivdata= NULL;
    printk("inside file_read_write_helper method \n");
    printk("Old file %s\n", old_file);
    printk("new file %s\n", new_file);
    printk("edc flag %d\n", edc_flag);
    
    old_filp = filp_open(old_file, O_RDONLY, 0775);
    if (old_filp == NULL || IS_ERR(old_filp)) {
        printk("error occurred while opening user output file\n");
        err =  (int) PTR_ERR(old_filp);
        goto out;
    }

    printk("before new file %s\n", new_file);
    new_filp = filp_open(new_file, O_WRONLY | O_CREAT , 0775);
    if (new_filp == NULL || IS_ERR(new_filp)) {
        printk("error occurred while opening user output file \n");
        err =  (int) PTR_ERR(new_filp);
        goto out;
    }

    printk("after new file \n");

    old_filp->f_pos = 0;
    new_filp->f_pos = 0; 
    bytes_left = old_filp->f_inode->i_size;
    printk("%d\n", bytes_left); 

    read_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (read_buf == NULL) { 
        err = -ENOMEM;
        goto out;
    }

    ivdata = kmalloc(16, GFP_KERNEL);
    if (!ivdata) {
        // parital_op_flag = 1;
        printk("could not allocate ivdata\n");
        goto out;
    }

    strncpy(ivdata, "1234567890123456", 16);
    ivdata[16] = '\0';
    printk("ivdata : %s\n", ivdata);
    printk("Len of ivdata : %ld\n", strlen(ivdata));

    while(bytes_left >0){
        printk("Looping through files and copying/ecrypting/decrypting buffers\n");
        read_bytes = kernel_read(old_filp, read_buf, PAGE_SIZE, &(old_filp->f_pos));
        if (read_bytes <0){
            printk("unable to read bytes.\n");
            // parital_op_flag = 1;
            err  = -EIO;
            goto out;
        }
        bytes_left = bytes_left-read_bytes;

        if (edc_flag!=0){
            printk("Attempting to encrypt/decrypt file.\n");
            err = encypt_decrypt_func(enc_key, read_buf, read_bytes, edc_flag, ivdata);
            if (err!=0){
                // parital_op_flag = 1;
                printk("Error occured while performing file encryption/decryption.\n");
                goto out;
            }
        }
        
        printk("write bytes to temp output file\n");
        write_bytes = kernel_write(new_filp, read_buf, read_bytes, &(new_filp->f_pos));
        if (write_bytes <0){
            printk("error occured while writing bytes to temp file.\n");
            // parital_op_flag = 1;
            err  = -EIO;
            goto out;
        }
        printk("bytes_left: %d\n",bytes_left);
    }

    if (bytes_left>0){
        // parital_op_flag = 1;
        printk("Error occured during file encryption/decryption/copy.\n");
        err = -ENOSYS;
        goto out;
    }
    printk("Successfully performed file encryption/decryption/copy.\n");


    out: 
        if (read_buf!=NULL){
            kfree(read_buf);
        }
        if (ivdata!=NULL){
            kfree(ivdata);
        }

        if(new_filp!=NULL && !IS_ERR(new_filp)){
            filp_close(new_filp, NULL);
        }   
        if(old_filp!=NULL && !IS_ERR(old_filp)){
            filp_close(old_filp, NULL);
        }

        return err; 
}


static inline bool is_file_in_stb(struct dentry *dentry){
    if (strcmp(dentry->d_name.name, ".stb")==0){
		printk("File is present in .stb trash directory\n");
        return true;
	}
    printk("File is outside .stb trash directory\n");
    return false;
}


#endif /* FILE_UTILS_H */



