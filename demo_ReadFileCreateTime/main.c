

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


 long GetFileModifyTime(const char *path)
{
	int ret = 0;
	struct stat file_stat;

	ret = stat(path, &file_stat);
	if(ret != 0)
	{
		perror("stat file failed!");
		return 2;
	}
	printf("%ld", file_stat.st_mtime);
	return ret;
}


int main(int argc, char *argv[])
{
	long ret = 0;
    if(argc < 2)
    {
    	printf("Usage: ./test [file]\n");
    	return 1;
    }

    ret = GetFileModifyTime(argv[1]);

    return ret;
 }
