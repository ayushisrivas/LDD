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
	struct mq_attr attr;

	attr.mq_flags=0;
	attr.mq_maxmsg=MAX_MSG;
	attr.mq_msgsize=MAX_SIZE;
	attr.mq_curmsgs=0;


	mq=mq_open(MQ_NAME, O_CREAT| O_RDWR, 0644, &attr);
	if(mq <0)
	{
		perror("Failed creating message queue\n");
		return 1;
	}

	char *message= "Process 1: hello!\n";
	if(mq_send(mq, message, strlen(message)+1, 0)<0)
	{
		perror("failed to send message ]\n");
		return 1;
	}
	printf("message added to the queue\n");


	sleep(20);
	mq_close(mq);
	mq_unlink(MQ_NAME);
	printf("MQ removed successfully.\n");

	return 0;
}

