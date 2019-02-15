
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/vfs.h>

int main(int argc, char *argv[])
{
	long ret = 0;
	struct statfs diskinfo;
    long long availSize = 0;
    char *sd_dev_path = NULL;
#if 0
    if(argc < 2)
    {
    	printf("Usage: ./readSDfreesize [file]\n");
    	return 1;
    }
#endif
    if(access("/dev/mmcblk0", F_OK) == 0)
    {
        sd_dev_path = "/dev/mmcblk0";
    }
    else if(access("/dev/mmcblk0p1", F_OK) == 0)
    {
        sd_dev_path = "/dev/mmcblk0p1";
    }
    if(sd_dev_path != NULL)
    {

        if(statfs("/mnt/extsd", &diskinfo) == 0)
        {
            availSize = (unsigned long long)diskinfo.f_bfree * (unsigned long long)diskinfo.f_bsize;
            availSize = availSize >>20; //MB
            printf("%lld", availSize);
            return 0;
        }else
        {
            return 3;
        }
    }
    return 2;
}
