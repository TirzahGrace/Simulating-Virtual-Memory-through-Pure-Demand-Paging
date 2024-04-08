#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>

#define FROM_PROCESS 10
#define TO_PROCESS 20
#define PAGE_FAULT_HANDLED 5
#define TERMINATED 10

typedef struct {
    long mtype;
    char mbuf[1];
} MMUToScheduler;

typedef struct {
    long mtype;
    int id;
} Message;

int sendMessage(int qid, Message *msg) {
    int result, length;
    length = sizeof(Message) - sizeof(long);
    result = msgsnd(qid, msg, length, 0);
    if (result == -1) {
        perror("Error in sending message");
        exit(EXIT_FAILURE);
    }
    return result;
}

int readMessage(int qid, long type, Message *msg) {
    int result, length;
    length = sizeof(Message) - sizeof(long);
    result = msgrcv(qid, msg, length, type, 0);
    if (result == -1) {
        perror("Error in receiving message");
        exit(EXIT_FAILURE);
    }
    return result;
}

int readMessageFromMMU(int qid, long type, MMUToScheduler *msg) {
    int result, length;
    length = sizeof(MMUToScheduler) - sizeof(long);
    result = msgrcv(qid, msg, length, type, 0);
    if (result == -1) {
        perror("Error in receiving message from MMU");
        exit(EXIT_FAILURE);
    }
    return result;
}

int main(int argc, char *argv[]) {
    int mq1Key, mq2Key, masterPid;
    
    if (argc < 5) {
        printf("Usage: %s mq1_key mq2_key k master_pid\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    mq1Key = atoi(argv[1]);
    mq2Key = atoi(argv[2]);
    int k = atoi(argv[3]);
    masterPid = atoi(argv[4]);

    Message sendMsg, recvMsg;
    int mq1 = msgget(mq1Key, 0666);
    int mq2 = msgget(mq2Key, 0666);
    
    if (mq1 == -1 || mq2 == -1) {
        perror("Failed to create message queue");
        exit(EXIT_FAILURE);
    }
    
    printf("Total number of processes: %d\n", k);

    int terminatedProcess = 0;
    
    while (terminatedProcess < k) {
        readMessage(mq1, FROM_PROCESS, &recvMsg);
        int currId = recvMsg.id;

        sendMsg.mtype = TO_PROCESS + currId;
        sendMessage(mq1, &sendMsg);

        MMUToScheduler mmuRecv;
        readMessageFromMMU(mq2, 0, &mmuRecv);

        if (mmuRecv.mtype == PAGE_FAULT_HANDLED) {
            sendMsg.mtype = FROM_PROCESS;
            sendMsg.id = currId;
            sendMessage(mq1, &sendMsg);
        } else if (mmuRecv.mtype == TERMINATED) {
            terminatedProcess++;
        } else {
            perror("Received wrong message from MMU");
            exit(EXIT_FAILURE);
        }
    }

    kill(masterPid, SIGUSR1);
    pause();
    printf("Scheduler terminating...\n");
    
    exit(EXIT_SUCCESS);
}
