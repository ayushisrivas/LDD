#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include<sys/wait.h>
#include<sys/types.h>
int main()
{
	pid_t pid = fork();
	if(pid<0)
	{
		printf("error creating fork\n");
		return 1;
	}
	if(pid==0)
	{
		printf("hello from child : %u", getpid());
	}
	else
	{
		printf("hello from parent : %u",getpid());
		wait(NULL);
	}
	return 0;
}

