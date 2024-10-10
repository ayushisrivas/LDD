//process 2 to implement bidirectional fifo

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"
#define BUFFER_SIZE 1024

int main()
{
	char *message= "Hello from second process to first process!\n";
	char buffer[BUFFER_SIZE];
	int fd1=open(FIFO1, O_WRONLY);
	int fd2=open(FIFO2, O_RDONLY);
	

	write(fd1, message, strlen(message)+1);
	printf("process 2: Message sent to process 1\n");
	read(fd2, buffer, BUFFER_SIZE);
	printf("process 2: Message read from process 1: %s",buffer);

	close(fd1);
	close(fd2);

	return 0;
}

