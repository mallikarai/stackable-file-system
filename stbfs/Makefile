STBFS_VERSION="0.1"

EXTRA_CFLAGS += -DSTBFS_VERSION=\"$(STBFS_VERSION)\"

obj-m += stbfs.o

stbfs-objs := dentry.o file.o inode.o main.o super.o lookup.o mmap.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: stbfs

# stbfsctl: stbfsctl.c
# 	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi stbfsctl.c -o stbfsctl 

stbfs:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
