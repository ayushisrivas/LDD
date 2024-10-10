
//o Problem: Write a program using POSIX message queues (mq_open, mq_send, and mq_receive). One process creates a message queue, sends a message, and another process receives and prints the message.
//


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<mqueue.h>
#include<sys/wait.h>
#include<sys/stat.h>

#define MQ_NAME "/message_queue"
#define MAX_MSG 10
#define MAX_SIZE 100

int main()
{
	mqd_t mq;
	//struct mq_attr attr;
	char buffer[MAX_SIZE];
	//attr.mq_flags=0;
	//attr.mq_maxmsg=MAX_MSG;
	//attr.mq_maxsize=MAX_SIZE;
	//attr.mq_curmsgs=0;


	mq=mq_open(MQ_NAME, O_RDONLY, 0644, NULL);
	if(mq <0)
	{
		perror("Failed opening message queue\n");
		return 1;
	}

	if(mq_receive(mq, buffer, MAX_SIZE, NULL)<0)
	{
		perror("MQ couldnt receive message.\n");
		return 1;
	}

	printf("Message received: %s", buffer);

	mq_close(mq);
	return 0;
}

