#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>


#define PSEND_TYPE 10
#define MMUTOPRO 20
#define INVALID_PAGE_REF -2
#define PAGEFAULT -1
#define PROCESS_OVER -9
#define PAGEFAULT_HANDLED 5
#define TERMINATED 10

int count = 0;
int *pffreq;
FILE *result_file;
int i;

typedef struct {
	int frameno;
	int isvalid;
	int count;
} PTB_Entry;

typedef struct {
	pid_t pid;
	int m;
	int f_cnt;
	int f_allo;
} PCB;

typedef struct
{
	int current;
	int flist[];
} FreeList;

struct MessageBuffer
{
	long mtype;
	int id;
	int pageno;
};

struct MMUtoPBUF
{
	long mtype;
	int frameno;
};

struct MMUtoSCH
{
	long mtype;
	char mbuf[1];
};


key_t freekey, pagetbkey;
key_t MessageQueue2key, MessageQueue3key;
key_t PCBkey;

int PTBid, freelid;
int MessageQueue2id, MessageQueue3id;
int PCBid;


int m,k;

int ReadRequest(int* id)
{
	struct MessageBuffer mbuf;
	int length;
	length = sizeof(struct MessageBuffer) - sizeof(long);
	memset(&mbuf, 0, sizeof(mbuf));

	int rst = msgrcv(MessageQueue3id, &mbuf, length, PSEND_TYPE, 0);
	if (rst == -1)
	{
		if(errno == EINTR)
			return -1;
		perror("Messagercv");
		exit(EXIT_FAILURE);
	}
	*id = mbuf.id;
	return mbuf.pageno;
}

void SendReply(int id, int frameno)
{
	struct MMUtoPBUF mbuf;
	mbuf.mtype = id + MMUTOPRO;
	mbuf.frameno = frameno;
	int length = sizeof(struct MessageBuffer) - sizeof(long);
	int rst = msgsnd(MessageQueue3id, &mbuf, length, 0);
	if (rst == -1)
	{
		perror("Messagesnd");
		exit(EXIT_FAILURE);
	}
}

void notifySched(int type)
{
	struct MMUtoSCH mbuf;
	mbuf.mtype = type;
	int length = sizeof(struct MessageBuffer) - sizeof(long);
	int rst = msgsnd(MessageQueue2id, &mbuf, length, 0);
	if (rst == -1)
	{
		perror("Messagesnd");
		exit(EXIT_FAILURE);
	}
}
PCB *PCBptr;
PTB_Entry *PTBptr;
FreeList *freeptr;

int handlePageFault(int id, int pageno)
{
	int i;
	if (freeptr->current == -1 || PCBptr[i].f_cnt <= PCBptr[i].f_allo)
	{
		int min = INT_MAX, mini = -1;
		int victim = 0;
		for (i = 0; i < PCBptr[i].m; i++)
		{
			if (PTBptr[id * m + i].isvalid == 1)
			{
				if (PTBptr[id * m + i].count < min)
				{
					min = PTBptr[id * m + i].count;
					victim = PTBptr[id * m + i].frameno;
					mini = i;
				}
			}
		}
		PTBptr[id * m + mini].isvalid = 0;
		return victim;
	}
	else
	{
		int fn = freeptr->flist[freeptr->current];
		freeptr->current -= 1;
		return fn;
	}
}

void Free_Pages(int i)
{

	int k = 0;
	for (k = 0; k < PCBptr[i].m; i++)
	{
		if (PTBptr[i * m + k].isvalid == 1)
		{
			freeptr->flist[freeptr->current + 1] = PTBptr[i * m + k].frameno;
			freeptr->current += 1;
		}
	}
}

int HandleMMURequest()
{
	PCBptr = (PCB*)(shmat(PCBid, NULL, 0));
	if (*(int *)PCBptr == -1)
	{
		perror("mmu: PCB-shmat");
		exit(EXIT_FAILURE);
	}
	PTBptr = (PTB_Entry*)(shmat(PTBid, NULL, 0));

	if (*(int *)PTBptr == -1)
	{
		perror("mmu: PTB: PCB-shmat");
		exit(EXIT_FAILURE);
	}

	freeptr = (FreeList*)(shmat(freelid, NULL, 0));
	if (*((int *)freeptr) == -1)
	{
		perror("freel-shmat");
		exit(EXIT_FAILURE);
	}

	int id = -1, pageno;
	pageno = ReadRequest(&id);
	if(pageno == -1 && id == -1)
	{
		return 0;
	}
	int i = id;
	if (pageno == PROCESS_OVER)
	{
		Free_Pages(id);
		notifySched(TERMINATED);
		return 0;
	}

	count ++;
	printf("Page reference : (%d,%d,%d)\n",count,id,pageno);
	fprintf(result_file,"Page reference : (%d,%d,%d)\n",count,id,pageno);
	if (PCBptr[id].m < pageno || pageno < 0)
	{
		printf("Invalid Page Reference : (%d %d)\n",id,pageno);
		fprintf(result_file,"Invalid Page Reference : (%d %d)\n",id,pageno);
		SendReply(id, INVALID_PAGE_REF);
		printf("Process %d: TRYING TO ACCESS INVALID PAGE REFERENCE %d\n", id, pageno);
		Free_Pages(id);
		notifySched(TERMINATED);
	}
	else
	{
		if (PTBptr[i * m + pageno].isvalid == 0)
		{
			//PAGE FAULT
			printf("Page Fault : (%d, %d)\n",id,pageno);
			fprintf(result_file,"Page Fault : (%d, %d)\n",id,pageno);
			pffreq[id] += 1;
			SendReply(id, -1);
			int fno = handlePageFault(id, pageno);
			PTBptr[i * m + pageno].isvalid = 1;
			PTBptr[i * m + pageno].count = count;
			PTBptr[i * m + pageno].frameno = fno;
			notifySched(PAGEFAULT_HANDLED);
		}
		else
		{
			// Frame is Found.
			SendReply(id, PTBptr[i * m + pageno].frameno);
			PTBptr[i * m + pageno].count = count;
		}
	}
	if(shmdt(PCBptr) == -1)
	{
		perror("PCBptr-shmdt");
		exit(EXIT_FAILURE);
	}
	if(shmdt(PTBptr) == -1)
	{
		perror("PTBptr-shmdt");
		exit(EXIT_FAILURE);
	}
	if(shmdt(freeptr) == -1)
	{
		perror("freel-shmdt");
		exit(EXIT_FAILURE);
	}
	return 0;
}
int flag = 1;
void handleTermination(int sig)
{
	flag = 0;
}

int main(int argc, char const *argv[])
{
	if (argc < 4)
	{
		printf("mmu m2key m3key PTBkey fkey PCBkey m k\n");
		exit(EXIT_FAILURE);
	}
	MessageQueue2id = atoi(argv[1]);
	MessageQueue3id = atoi(argv[2]);
	PTBid = atoi(argv[3]);
	freelid = atoi(argv[4]);
	PCBid = atoi(argv[5]);
	m = atoi(argv[6]);
	k = atoi(argv[7]);

	signal(SIGUSR2, handleTermination);
	pffreq = (int *)malloc(k*sizeof(int));
	for(i=0;i<k;i++)
	{
		pffreq[i] = 0;
	} 
	result_file = fopen("result.txt","w");

	while(flag)
	{
		HandleMMURequest();
	}

	printf("Page fault Count for each Process:\n");	
	fprintf(result_file,"Page fault Count for each Process:\n");
	printf("Process_Id\tFreq\n");
	fprintf(result_file,"Process Id\tFreq\n");

	for(i = 0;i<k;i++)
	{
		printf("%d\t\t%d\n",i,pffreq[i]);
		fprintf(result_file,"%d\t\t%d\n",i,pffreq[i]);
	}
	fclose(result_file);
	return 0;
}