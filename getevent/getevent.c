#include<stdio.h>
#include<fcntl.h>
#include<errno.h>
#include<stdlib.h>
#include<linux/input.h>

#define KEY_DEV "/dev/input/event0" 


static int ts_fd = -1; 
static struct input_event data;

static int init_device(char *TS_DEV) 
{ 
    if((ts_fd = open(TS_DEV, O_RDONLY)) < 0) 
    { 
        printf("Error open %s\n\n", TS_DEV); 
        return -1; 
    } 
    return ts_fd; 
}

static int test_key() 
{ 
    if(init_device(KEY_DEV) < 0) 
        return -1; 
    while(1) 
    { 
        read(ts_fd, &data, sizeof(data)); 
        if (data.type == EV_KEY) 
            printf(" type: EV_KEY, event = %d, value = %d\n",data.code, data.value); 
    } 
    return 0; 
}

int main() 
{ 
    static int i; 
	printf("getevent starting\n"); 

    test_key(); 

    return 0; 
}

