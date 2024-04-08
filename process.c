#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/msg.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <sys/shm.h>

#define MAX_PAGES 1000
#define TO_SCHEDULAR 10
#define FROM_SCHEDULAR 20  
#define TO_MMU 10
#define FROM_MMU 20 

int pg_no[MAX_PAGES] ;
int no_of_pages;

typedef struct MMU_MSG_BUF_SEND {
	long    Message_Type;         
	int id;
	int pageno;
} MMU_MSG_BUF_SEND;

typedef struct MMU_MSG_BUF_RECV {
	long    Message_Type;         
	int frameno;
} MMU_MSG_BUF_RECV;

typedef struct MYMSGBUFFER {
	long    Message_Type;          
	int id;
} MYMSGBUFFER;

void Convert_Ref_Page_No(char * refs)
{
	const char s[2] = "|";
	char *token;
	token = strtok(refs, s);
	while ( token != NULL )
	{
		pg_no[no_of_pages] = atoi(token);
		no_of_pages++;
		token = strtok(NULL, s);
	}
}

int SEND_MSG_MMU( int qid, struct MMU_MSG_BUF_SEND *qbuf )
{
	int result, length;
	length = sizeof(struct MMU_MSG_BUF_SEND) - sizeof(long);

	if ((result = msgsnd( qid, qbuf, length, 0)) == -1)
	{
		perror("Error in sending message");
		exit(1);
	}
	return (result);
}
int READ_MSG_MMU( int qid, long type, struct MMU_MSG_BUF_RECV *qbuf )
{
	int result, length;
	length = sizeof(struct MMU_MSG_BUF_RECV) - sizeof(long);

	if ((result = msgrcv( qid, qbuf, length, type,  0)) == -1)
	{
		perror("Error in receiving message");
		exit(1);
	}
	return (result);
}

int SEND_MSG( int qid, struct MYMSGBUFFER *qbuf )
{
	int result, length;
	length = sizeof(struct MYMSGBUFFER) - sizeof(long);

	if ((result = msgsnd( qid, qbuf, length, 0)) == -1)
	{
		perror("Error in sending message");
		exit(1);
	}
	return (result);
}
int READ_MSG( int qid, long type, struct MYMSGBUFFER *qbuf )
{
	int  result, length;
	length = sizeof(struct MYMSGBUFFER) - sizeof(long);
	if ((result = msgrcv( qid, qbuf, length, type,  0)) == -1)
	{
		perror("Error in receiving message");
		exit(1);
	}
	return (result);
}

int main(int argc, char *argv[]) 
{
	if (argc < 5)
	{
		perror("Please give 5 arguments :id,Message_Queue1,Message_Queue3,ref_string \n");
		exit(1);
	}
	int id, Message_Queue1_k, Message_Queue3_k;
	id = atoi(argv[1]);
	Message_Queue1_k = atoi(argv[2]);
	Message_Queue3_k = atoi(argv[3]);
	no_of_pages = 0;
	Convert_Ref_Page_No(argv[4]);
	int Message_Queue1, Message_Queue3;
	Message_Queue1 = msgget(Message_Queue1_k, 0666);
	Message_Queue3 = msgget(Message_Queue3_k, 0666);
	if (Message_Queue1 == -1)
	{
		perror("Message Queue1 creation failed");
		exit(1);
	}
	if (Message_Queue3 == -1)
	{
		perror("Message Queue3 creation failed");
		exit(1);
	}
	printf("Process id= %d\n", id);

	//sending to scheduler
	MYMSGBUFFER msg_send;
	msg_send.Message_Type = TO_SCHEDULAR;
	msg_send.id = id;
	SEND_MSG(Message_Queue1, &msg_send);

	//Wait until msg receive from scheduler
	MYMSGBUFFER msg_recv;
	READ_MSG(Message_Queue1, FROM_SCHEDULAR + id, &msg_recv);

	MMU_MSG_BUF_SEND mmu_send;
	MMU_MSG_BUF_RECV mmu_recv;
	int cpg = 0; //counter for page number array
	while (cpg < no_of_pages)
	{
		// sending msg to mmu the page number
		mmu_send.Message_Type = TO_MMU;
		mmu_send.id = id;
		mmu_send.pageno = pg_no[cpg];
		SEND_MSG_MMU(Message_Queue3, &mmu_send);
		printf("process: id: %d: Sent request for %d page number\n",id, pg_no[cpg]);

		READ_MSG_MMU(Message_Queue3, FROM_MMU + id, &mmu_recv);

		if (mmu_recv.frameno >= 0)
		{
			printf("Frame number from MMU received for process %d: %d\n" , id, mmu_recv.frameno);
			cpg++;
		}
		else if (mmu_recv.frameno == -1) 
		{
			printf("Page fault occured for process %d\n", id);
		}
		else if (mmu_recv.frameno == -2)
		{
			printf("Invalid page reference for process %d terminating ...\n", id) ;
			exit(1);
		}
	}
	printf("Process %d Terminated successfly\n", id);
	mmu_send.pageno = -9;
	mmu_send.id = id;
	mmu_send.Message_Type = TO_MMU;
	SEND_MSG_MMU(Message_Queue3, &mmu_send);

	exit(1);
	return 0;
}


