//pipe for IPC
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/wait.h>

int main()
{
	int fd[2];
	char *message="Hello, this is a message!\n";
	char buffer[100];
	if(pipe(fd)<0)
	{
		printf("error opening pipe\n");
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
		close(fd[0]);
		write(fd[1],message, strlen(message)+1);
		printf("Child wrote the message to pipe!\n");
		close(fd[1]);
	}
	else
	{
		close(fd[1]);
		read(fd[0],buffer, 100);
		printf("Parent read the message: %s", buffer);
		close(fd[0]);
	}
	return 0;
}



