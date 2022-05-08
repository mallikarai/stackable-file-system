#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<sys/ioctl.h>

#define UNDELTE _IOWR(124,124,int32_t*)

int main(int argc, char * const argv[])
{
    char *help_msg = "./ioctl_test -u FILE";

    if (!(argc==3 ||  argc==5)){
        printf("Incorrect arguements.\n");
        goto invalid;
    }

    printf("\nOpening Driver\n");
    char *infile = NULL;
    int fd, opt=0;
    char input_filepath[272];

    while((opt = getopt(argc, argv, "hu:"))!=-1)
    {
        switch(opt)
        {
            case 'h':
                    printf("%s\n",help_msg);
            case 'u':
                    infile = optarg;
                    break;
            case ':':
            printf("Option %c expects argument\n", optopt);
                    goto invalid;
            break;
            case '?':
                    printf ("Unknown option character.\n");
                    goto invalid;
        }
    }


    strcpy(input_filepath, "/mnt/stbfs/.stb/");
    strcat(input_filepath, infile);
    printf("Input filepath : %s\n", input_filepath);
    fd = open(input_filepath, O_RDWR);
    if(fd < 0) {
        printf("Cannot open device file...\n");
        return 0;
    }

    printf("Writing Value to Driver\n");
    ioctl(fd, UNDELTE, 124);

    printf("Closing Driver\n");
    close(fd);
    exit(0);
    invalid :
    printf("The command should be in the format:\n%s\n",help_msg);
    exit(1);
}