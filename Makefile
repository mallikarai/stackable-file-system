obj-m += stbfsctl.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: stbfsctl

stbfsctl: stbfsctl.c
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi stbfsctl.c -o stbfsctl 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f stbfsctl

