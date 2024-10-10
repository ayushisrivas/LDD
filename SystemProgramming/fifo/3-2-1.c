//process 1 for bidirectional fifo communication
//
//

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<fcntl.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"
#define BUFFER_SIZE 1024

int main()
{
	char buffer[BUFFER_SIZE];
	char *message="Hello, this is a message from process 1.\n";
	int fd1=mkfifo(FIFO1, 0666);
	if(fd1<0)
	{
		perror("Could not create first fifo\n");
		return 1;
	}
	int fd2=mkfifo(FIFO2, 0666);
	if(fd2<0)
	{
		perror("Could not create second fifo\n");
		return 1;
	}
	fd1=open(FIFO1, O_RDONLY);
	fd2=open(FIFO2, O_WRONLY);

	write(fd2, message, strlen(message));
	printf("process 1: sent message to process 2.\n");
	read(fd1, buffer, BUFFER_SIZE);
	printf("process 1: message read from process 2: %s",buffer);
	
	close(fd1);
	close(fd2);

	unlink(FIFO1);
	unlink(FIFO2);

	return 0;
}



