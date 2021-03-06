/*
 *  * File         : testchardriver.c
 *  * Description  : ELE784 Lab1 charDriver tester
 *  *
 *  * Etudiants:  JORA09019100 (Alexandru Jora)
 *  *             MUKM28098503 (Mukandila Mukandila)
 *  */

#include "charDriver.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_NODE     "/dev/charDriver_Node"

// lazy terminal clearing
#define CLEAR_TERM  printf("\033[2J\033[1;1H")

// global file descriptor var
static int fd;

// prototypes
int menu(void);
int read_mode(void);
int write_mode(void);
int read_(void);
int write_(void);
int ioctl_call(void);

int main(void) {
    int opened = 0;
    while(1){
        int choice = menu();
        switch(choice){

            case 1:
                read_mode();
                break;

            case 2:
                write_mode();
                break;

            case 3:
                ioctl_call();
                break;

            case 4:
                printf("Exiting\n");
                exit(-1);
                break;

            default:
                printf("Command not recognized! Exiting\n");
                exit(-1);
        }
        if(fd)
            close(fd);
    }
}

int menu(void){
    CLEAR_TERM;
    int cmd;
    cmd = 0;
    printf("===========================\n");
    printf("charDriver test application\n");
    printf("===========================\n\n");
    printf("Choose a command to perform\n");
    printf("1. Read\n");
    printf("2. Write\n");
    printf("3. Send IOCTL command\n");
    printf("4. Exit\n");
    scanf("%d", &cmd);
    CLEAR_TERM;
    return cmd;
}

int read_(void){
    int numBytes, i, ret;
    int redo = 0;
    printf("How many bytes to read?\n");
    scanf("%d", &numBytes);
    char bufOut[numBytes];
    // flush buffer
    for(i=0; i<numBytes; i++){
        bufOut[i] = '\0';
    }
    if(fd){
        ret = read(fd, &bufOut, numBytes);
        // add string terminating char
        bufOut[numBytes] = '\0';
        //printf("Return value of read syscall: %i\n", ret);
        while(ret != numBytes && ret != 0){
            numBytes = numBytes - ret;
            ret = read(fd, &bufOut[ret -1], numBytes);
            //printf("Return value of read syscall: %i\n", ret);
        }
        printf("Read from buffer: %s\n\n", bufOut);
    }
    else{
        printf("Error using file descriptor!\n");
        exit(-2);
    }
    printf("Read again?\n");
    printf("1. Yes\n");
    printf("2. No\n");
    scanf("%d", &redo);
    if(redo == 1){
        CLEAR_TERM;
        read_();
    }
    else
        return 0;
}

int read_mode(void){
    int read_mode;
    read_mode = 0;
    printf("Choose an opening mode\n");
    printf("1. Blocking\n");
    printf("2. Non-blocking\n");
    scanf("%d", &read_mode);
    CLEAR_TERM;

    switch(read_mode){
        case 1:
            fd = open(DEVICE_NODE, O_RDONLY);
            if(fd){
                printf("Device succesfully openend in O_RDONLY BLOCKINGmode\n\n");
            }
            else{
                printf("Error opening device in O_RDONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            read_();
            break;

        case 2:
            fd = open(DEVICE_NODE, O_RDONLY | O_NONBLOCK);
            if(fd){
                printf("Device succesfully openend in O_RDONLY NON_BLOCKING mode\n\n");
            }
            else{
                printf("Error opening device in O_RDONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            read_();
            break;
    }
    close(fd);
    return 0;
}

int write_(void){
    int numBytes, i, ret;
    int redo = 0;
    printf("How many bytes to write?\n");
    scanf("%d", &numBytes);
    char bufIn[numBytes + 1];
    // flush buffer
    for(i=0; i<numBytes; i++){
        bufIn[i] = '\0';
    }
    printf("Data to be written?\n");
    scanf("%s", bufIn);

    if(fd){
        ret = write(fd, &bufIn, numBytes);
        //printf("Return value of write syscall: %i\n", ret);
        while(ret != numBytes){
            numBytes = numBytes - ret;
            ret = write(fd, &bufIn[ret - 1], numBytes);
            //printf("Return value of write syscall: %i\n", ret);
        }
        printf("Write to buffer: %s\n\n", bufIn);
    }
    else{
        printf("Error using file descriptor!\n");
        exit(-2);
    }
    printf("Write again?\n");
    printf("1. Yes\n");
    printf("2. No\n");
    scanf("%d", &redo);
    if(redo == 1){
        CLEAR_TERM;
        write_();
    }
    else
        return 0;
}

int write_mode(void){
    int write_mode;
    write_mode = 0;
    printf("Choose an opening mode\n");
    printf("1. Blocking\n");
    printf("2. Non-blocking\n");
    scanf("%d", &write_mode);
    CLEAR_TERM;

    switch(write_mode){
        case 1:
            fd = open(DEVICE_NODE, O_WRONLY);
            if(fd){
                printf("Device succesfully openend in O_RWONLY BLOCKING mode\n\n");
            }
            else{
                printf("Error opening device in O_RWONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            write_();
            break;

        case 2:
            fd = open(DEVICE_NODE, O_WRONLY | O_NONBLOCK);
            if(fd){
                printf("Device succesfully openend in O_RWONLY NON_BLOCKING mode\n\n");
            }
            else{
                printf("Error opening device in O_RWONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            write_();
            break;
    }
    close(fd);
    return 0;
}

int ioctl_call(void){
    int ioctl_ret, ioctl_cmd, redo;
    unsigned int newBufSize;
    printf("Select an IOCTL command\n");
    printf("1. Get number of readers\n");
    printf("2. Get circular buffer data count\n");
    printf("3. Get circular buffer size\n");
    printf("4. Get magic number\n");
    printf("5. Set new circular buffer size\n");
    scanf("%d", &ioctl_cmd);
    CLEAR_TERM;
    if(fd)
        close(fd);

    switch(ioctl_cmd){
        case 1:
            fd = open(DEVICE_NODE, O_RDONLY);
            if(fd){
                printf("Device succesfully openend in O_RDONLY mode\n\n");
            }
            else{
                printf("Error opening device in O_RDONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            ioctl(fd, CHARDRIVER_GETNUMREADER, &ioctl_ret);
            printf("Number of readers is: %i\n\n", ioctl_ret);
            printf("Send another command?\n");
            printf("1. Yes\n");
            printf("2. No\n");
            scanf("%d", &redo);
            if(redo == 1){
                CLEAR_TERM;
                ioctl_call();
            }
            else
                return 0;
            break;

        case 2:
            fd = open(DEVICE_NODE, O_RDONLY);
            if(fd){
                printf("Device succesfully openend in O_RDONLY mode\n\n");
            }
            else{
                printf("Error opening device in O_RDONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            ioctl(fd, CHARDRIVER_GETNUMDATA, &ioctl_ret);
            printf("Circular buffer data count is: %i\n\n", ioctl_ret);
            printf("Send another command?\n");
            printf("1. Yes\n");
            printf("2. No\n");
            scanf("%d", &redo);
            if(redo == 1){
                CLEAR_TERM;
                ioctl_call();
            }
            else
                return 0;
            break;

        case 3:
            fd = open(DEVICE_NODE, O_RDONLY);
            if(fd){
                printf("Device succesfully openend in O_RDONLY mode\n\n");
            }
            else{
                printf("Error opening device in O_RDONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            ioctl(fd, CHARDRIVER_GETBUFSIZE, &ioctl_ret);
            printf("Circular buffer size is: %i\n\n", ioctl_ret);
            printf("Send another command?\n");
            printf("1. Yes\n");
            printf("2. No\n");
            scanf("%d", &redo);
            if(redo == 1){
                CLEAR_TERM;
                ioctl_call();
            }
            else
                return 0;
            break;

        case 4:
            fd = open(DEVICE_NODE, O_RDONLY);
            if(fd){
                printf("Device succesfully openend in O_RDONLY mode\n\n");
            }
            else{
                printf("Error opening device in O_RDONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            ioctl(fd, CHARDRIVER_GETMAGICNUMBER, &ioctl_ret);
            printf("Magic number is: %c\n\n", (char)ioctl_ret);
            printf("Send another command?\n");
            printf("1. Yes\n");
            printf("2. No\n");
            scanf("%d", &redo);
            if(redo == 1){
                CLEAR_TERM;
                ioctl_call();
            }
            else
                return 0;
            break;

        case 5:
            printf("Please input new circular buffer size\n");
            scanf("%d", &newBufSize);
            fd = open(DEVICE_NODE, O_WRONLY);
            if(fd){
                printf("Device succesfully openend in O_WRONLY mode\n\n");
            }
            else{
                printf("Error opening device in O_WRONLY mode\n");
                printf("Exiting...\n");
                exit(-1);
            }
            if(!ioctl(fd, CHARDRIVER_SETBUFSIZE, newBufSize))
                printf("Set new circular buffer size to: %i\n", newBufSize);
            else
                printf("Error resizing! Size too small!\n");
            printf("Send another command?\n");
            printf("1. Yes\n");
            printf("2. No\n");
            scanf("%d", &redo);
            if(redo == 1){
                CLEAR_TERM;
                ioctl_call();
            }
            else
                return 0;
            break;

        default:
            printf("**********************************\n");
            printf("Command not recognized! Try again!\n");
            printf("**********************************\n");
            ioctl_call();
    }
    return 0;
}
