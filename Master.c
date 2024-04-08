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


typedef struct {
	int frameno;
	int isvalid;
	int count;
}PTB_Entry;

typedef struct {
	pid_t pid;
	int m;
	int f_cnt;
	int f_allo;
}PCB;

typedef struct 
{
	int current;
	int flist[];
}FreeList;

int k,m,f;
int flag = 0;
key_t freekey,pagetbkey;
key_t readykey, MessageQueue2key, MessageQueue3key;
key_t PCBkey;

int ptbid, freelid;
int readyid, MessageQueue2id, MessageQueue3id;
int PCBid;

void PCB_Print(PCB p)
{
	printf("PID = %d m = %d f_cnt = %d\n",p.pid,p.m,p.f_cnt);

}
int MAX(int a, int b)
{
	return (a>b)?a:b;
}

void MY_EXIT(int status);

void createFreeList()
{
	int i;
	freekey = ftok("mmu.c",156);
	if(freekey == -1)
	{	
		perror("freekey");
		MY_EXIT(EXIT_FAILURE);
	}
	freelid = shmget(freekey, sizeof(FreeList)+f*sizeof(int), 0666 | IPC_CREAT | IPC_EXCL);
	if(freelid == -1)
	{	
		perror("free-shmget");
		MY_EXIT(EXIT_FAILURE);
	}

	FreeList *ptr = (FreeList*)(shmat(freelid, NULL, 0));
	if(*((int *)ptr) == -1)
	{
		perror("freel-shmat");
		MY_EXIT(EXIT_FAILURE);
	}
	for(i=0;i<f;i++)
	{
		ptr->flist[i] = i;
	}
	ptr->current = f-1;

	if(shmdt(ptr) == -1)
	{
		perror("freel-shmdt");
		MY_EXIT(EXIT_FAILURE);
	}
}

void createPageTables()
{
	int i;
	pagetbkey = ftok("mmu.c",1100);
	if(pagetbkey == -1)
	{	
		perror("pagetbkey");
		MY_EXIT(EXIT_FAILURE);
	}
	ptbid = shmget(pagetbkey, m*sizeof(PTB_Entry)*k, 0666 | IPC_CREAT | IPC_EXCL);
	if(ptbid == -1)
	{	
		perror("PCB-shmget");
		MY_EXIT(EXIT_FAILURE);
	}

	PTB_Entry *ptr = (PTB_Entry*)(shmat(ptbid, NULL, 0));
	if(*(int *)ptr == -1)
	{
		perror("PCB-shmat");
		MY_EXIT(EXIT_FAILURE);
	}

	for(i=0;i<k*m;i++)
	{
		ptr[i].frameno = 0;
		ptr[i].isvalid = 0;
	}

	if(shmdt(ptr) == -1)
	{
		perror("PCB-shmdt");
		MY_EXIT(EXIT_FAILURE);
	}
}


void createMessageQueues()
{
	readykey = ftok("mmu.c",1200);
	if(readykey == -1)
	{	
		perror("readykey");
		MY_EXIT(EXIT_FAILURE);
	}
	readyid = msgget(readykey, 0666 | IPC_CREAT| IPC_EXCL);
	if(readyid == -1)
	{
		perror("ready-msgget");
		MY_EXIT(EXIT_FAILURE);
	}

	MessageQueue2key = ftok("mmu.c",1300);
	if(MessageQueue2key == -1)
	{	
		perror("MessageQueue2key");
		MY_EXIT(EXIT_FAILURE);
	}
	MessageQueue2id = msgget(MessageQueue2key, 0666 | IPC_CREAT| IPC_EXCL );
	if(MessageQueue2id == -1)
	{
		perror("MessageQueue2-msgget");
		MY_EXIT(EXIT_FAILURE);
	} 

	MessageQueue3key = ftok("mmu.c",1400);
	if(MessageQueue3key == -1)
	{	
		perror("MessageQueue3key");
		MY_EXIT(EXIT_FAILURE);
	}
	MessageQueue3id = msgget(MessageQueue3key, 0666 | IPC_CREAT| IPC_EXCL);
	if(MessageQueue3id == -1)
	{
		perror("MessageQueue3-msgget");
		MY_EXIT(EXIT_FAILURE);
	} 
}

void createPCBs()
{
	int i;
	PCBkey = ftok("mmu.c",1600);
	if(PCBkey == -1)
	{	
		perror("PCBkey");
		MY_EXIT(EXIT_FAILURE);
	}
	PCBid = shmget(PCBkey, sizeof(PCB)*k, 0666 | IPC_CREAT | IPC_EXCL );
	if(PCBid == -1)
	{	
		perror("PCB-shmget");
		MY_EXIT(EXIT_FAILURE);
	}

	PCB *ptr = (PCB*)(shmat(PCBid, NULL, 0));	
	if(*(int *)ptr == -1)
	{
		perror("PCB-shmat");
		exit(EXIT_FAILURE);
	}
	
	int totpages = 0;
	for(i=0;i<k;i++)
	{
		ptr[i].pid = i;
		ptr[i].m = rand()%m + 1;
		ptr[i].f_allo = 0;
		ptr[i].f_cnt = 0;
		totpages +=  ptr[i].m;
	}

	int allo_frame = 0;
	printf("tot = %d, k = %d, f=  %d\n",totpages,k,f);
	int MAX = 0,MAXi = 0;
	
	for(i=0;i<k;i++)
	{
		int allo = (int)round(ptr[i].m*(f-k)/(float)totpages) + 1;
		if(ptr[i].m > MAX)
		{
			MAX = ptr[i].m;
			MAXi = i;
		}
		allo_frame = allo_frame + allo;
		ptr[i].f_cnt = allo;
		
	}
	ptr[MAXi].f_cnt += f - allo_frame; 

	for(i=0;i<k;i++)
	{
		PCB_Print(ptr[i]);
	}

	if(shmdt(ptr) == -1)
	{
		perror("freel-shmdt");
		exit(EXIT_FAILURE);
	}
}

void Clear_Resources()
{
	if(shmctl(freelid,IPC_RMID, NULL) == -1)
	{
		perror("shmctl-freel");
	}
	if(shmctl(ptbid,IPC_RMID, NULL) == -1)
	{
		perror("shmctl-ptb");
	}
	if(msgctl(readyid, IPC_RMID, NULL) == -1)
	{
		perror("msgctl-ready");
	}
	if(shmctl(PCBid,IPC_RMID, NULL) == -1)
	{
		perror("shmctl-PCB");
	}
	if(msgctl(MessageQueue3id, IPC_RMID, NULL) == -1)
	{
		perror("msgctl-MessageQueue3");
	}
	if(msgctl(MessageQueue2id, IPC_RMID, NULL) == -1)
	{
		perror("msgctl-MessageQueue2");
	}
	return ;
}

void MY_EXIT(int status)
{
	Clear_Resources();
	exit(status);
}


void createProcesses()
{
	PCB *ptr = (PCB*)(shmat(PCBid, NULL, 0));
	if(*(int *)ptr == -1)
	{
		perror("PCB-shmat");
		MY_EXIT(EXIT_FAILURE);
	}

	int i,j;
	for(i=0;i<k;i++)
	{
		int rlen = rand()%(8*ptr[i].m) + 2*ptr[i].m + 1;
		char rstring[m*20*40];
		int l = 0;
		for(j=0;j<rlen;j++)
		{
			int r;
			r = rand()%ptr[i].m;
			float p = (rand()%100)/100.0;
			if(p < 0.2)
			{
				r = rand()%(1000*m) + ptr[i].m;
			}
			l += sprintf(rstring+l,"%d|",r);
		}
		printf("Reference string = %s\n",rstring);
		if(fork() == 0)
		{
			char Buffer1[20],Buffer2[20],Buffer3[20];
			sprintf(Buffer1,"%d",i);
			sprintf(Buffer2,"%d",readykey);
			sprintf(Buffer3,"%d",MessageQueue3key);
			execlp("./process","./process",Buffer1,Buffer2,Buffer3,rstring,(char *)(NULL));
			exit(0);

		}
		sleep(1);	
	}

}
int pid,sched_pid,mmu_pid;

void timetoend(int sig)
{
	sleep(1);
	kill(sched_pid, SIGTERM);
	kill(mmu_pid, SIGUSR2);
	sleep(2);
	flag = 1;

}
int main(int argc, char const *argv[])
{
	srand(time(NULL));
	signal(SIGUSR1, timetoend);
	signal(SIGINT, MY_EXIT);
	if(argc < 4)
	{
		printf("master k m f\n");
		MY_EXIT(EXIT_FAILURE);
	}
	k = atoi(argv[1]);
	m = atoi(argv[2]);
	f = atoi(argv[3]);
	pid = getpid();
	if(k <= 0 || m <= 0 || f <=0 || f < k)
	{
		printf("Invalid input\n");
		MY_EXIT(EXIT_FAILURE);
	}

	createPageTables();
	createFreeList();
	createPCBs();
	createMessageQueues();

	if((sched_pid = fork()) == 0)
	{
		char Buffer1[20],Buffer2[20],Buffer3[20],Buffer4[20];
		sprintf(Buffer1,"%d",readykey);
		sprintf(Buffer2,"%d",MessageQueue2key);
		sprintf(Buffer3,"%d",k);
		sprintf(Buffer4,"%d",pid);
		execlp("./scheduler","./scheduler",Buffer1,Buffer2,Buffer3,Buffer4,(char *)(NULL));
		exit(0);
	}
	printf("Schedular Generated\n");


	if((mmu_pid = fork()) == 0)
	{
		char Buffer1[20],Buffer2[20],Buffer3[20],Buffer4[20],Buffer5[20],Buffer6[20],Buffer7[20];
		sprintf(Buffer1,"%d",MessageQueue2id);
		sprintf(Buffer2,"%d",MessageQueue3id);
		sprintf(Buffer3,"%d",ptbid);
		sprintf(Buffer4,"%d",freelid);
		sprintf(Buffer5,"%d",PCBid);
		sprintf(Buffer6,"%d",m);
		sprintf(Buffer7,"%d",k);
		execlp("./mmu","./mmu",Buffer1,Buffer2,Buffer3,Buffer4,Buffer5,Buffer6,Buffer7,(char *)(NULL));
		exit(0);
	}
	printf("MMU Generated\n");

	createProcesses();
	if(flag == 0) pause();
	Clear_Resources();
	return 0;
}