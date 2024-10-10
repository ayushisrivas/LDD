//fifo - fork
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<fcntl.h>

int main()
{
	int fd;
	char *message="Hello, this is a message!\n";
	char buffer[100];
	if(mkfifo("my_fifo",0666)<0)
	{
		printf("error creating fifo\n");
		return 1;
	}

	pid_t pid=fork();
	if(pid<0)
	{
		printf("error creating fork\n");
		return 1;
	}
	else if(pid==0)
	{
		fd=open("my_fifo", O_WRONLY);
		write(fd,message, strlen(message)+1);
		printf("Child wrote the message to pipe!\n");
		close(fd);
	}
	else
	{
		fd=open("my_fifo",O_RDONLY);
		read(fd,buffer, sizeof(buffer));
		printf("Parent read the message: %s", buffer);
		close(fd);
	}
	return 0;
}



