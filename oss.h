/*  Author: Chase Richards
    Project: Homework 5 CS4760
    Date April 4, 2020
    Filename: oss.h  */

#ifndef OSS_H
#define OSS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <math.h>

void displayHelpMessage(); //-h getopt option
void sigHandler(int sig); //Signal Handle(ctrl c and timeout)
void removeAllMem(); //Removes all sharedmemory
FILE* openLogFile(char *file); //Opens the output log file
FILE* filePtr;
void manager(int);

int genProcPid(int *pidArr, int totalPids); //Generates the pid (0,1,2,3,4,..17) 


//Shared memory keys and shared memory segment ids
const key_t resDescKey = 122032;
const key_t clockKey = 202123;
const key_t messageKey = 493343;
int resDescSegment, clockSegment, msgqSegment;

/* ---------------------------------Messaging Setup-------------------------------------- */

//Child send to oss
const int requestResource = 0;
const int releaseResource = 1;
const int terminateProcess = 2;
//Oss sends to child
const int grantedRequest = 3;
const int denyRequest = 4;

typedef struct
{
    long typeofMsg;
    int process;
    int resource;
    int sendingProcess;
    int msgDetails;
} msg;

void messageToProcess(int receiver, int response);
int requestToOss(int process, int procPid, int resource);
int releaseToOss(int process, int procPid, int resource);
void terminateToOss(int process, int procPid);
void waiting(int process);

/* ------------------------------Simulated Clock Setup----------------------------------- */

//Shared memory clock
typedef struct
{
    unsigned int sec;
    unsigned int nanosec;
} clksim;


//Increment the clock so if it reaches a second in nanoseconds it changes accordingly
void clockIncrementor(clksim *simTime, int incrementor)
{
    simTime-> nanosec += incrementor;
    if(simTime-> nanosec >= 1000000000)
    {
        simTime-> sec += 1;
        simTime-> nanosec -= 1000000000;
    }
}

//For adding and subtracting the virtual clock times
clksim addTime(clksim time1, clksim time2)
{
    clksim sum = {.sec = time1.sec + time2.sec, .nanosec = time1.nanosec + time2.nanosec};
    if(sum.nanosec >= 1000000000)
    {
        sum.nanosec -= 1000000000;
        sum.sec += 1;
    }
    return sum;
}

clksim subTime(clksim time1, clksim time2)
{
    clksim sub = {.sec = time1.sec - time2.sec, .nanosec = time1.nanosec - time2.nanosec};
    if(sub.nanosec < 0)
    {
        sub.nanosec += 1000000000;
        sub.sec -= 1;
    }
    return sub;
}

clksim nextProcessStartTime(clksim maxTime, clksim curTime);

/* ------------------------------Resource Descriptor Setup----------------------------------- */

/* Resource Decriptor is a fixed size struct containing info on managing the resources in oss */
typedef struct
{
    //Resources Vector
    int totalResourcesVector[20];
    //Allocation vector
    int allocatedVector[20];
    //Shared or not vector
    int resSharedVector[20];
    //Request matrix of resources and the processes doing the requesting
    int requestingMatrix[18][20];
    //Allocated matrix of resources and the processes who have the resources allocated to them
    int allocatedMatrix[18][20];
} resDesc;

/* Constructor for the resource descriptor: initial instances of each resource class are a random num
   between 1 and 10 (inclusively). Create 20 resource descriptors out of which 20 +- 5% are shared */
void resDescConstruct(resDesc *descPtr)
{
    int i, j; //loops
    srand(time(NULL));
    int randNum = rand() % ((25 + 1) - 15) + 15;
    //Initialize the matrixes to all zeros
    for(i = 0; i < 18; i++)
    {
        for(j = 0; j < 20; j++)
        {
            descPtr-> requestingMatrix[i][j] = 0;
            descPtr-> allocatedMatrix[i][j] = 0;
        }
    }
    for(i = 0; i < 20; i++)
    {
        //20 +- 5% chance of it being a shared resource
        int randNum = rand() % ((6 + 1) - 4) + 4;
        int randNum1 = rand() % (randNum) + 1;
        if(randNum1 != 1)
        {
            //Random Number between 1-10 inclusive
            int randNum2 = rand() % 10 + 1;
            descPtr-> totalResourcesVector[i] = randNum2;
            descPtr-> allocatedVector[i] = descPtr-> totalResourcesVector[i];
            descPtr-> resSharedVector[i] = 0;
        }
        else
        {
            descPtr-> totalResourcesVector[i] = randNum;
            descPtr-> allocatedVector[i] = descPtr-> totalResourcesVector[i];
            descPtr-> resSharedVector[i] = 1;
        }       
    }
    return;
}

int deadlock(resDesc *resDescPtr, int nProcs, clksim *clockPtr, int *pidArr, int *procCounter, int *outputLines);
int req_lt_avail(int req[], int avail[], int shared[], int held[]);
void release(resDesc *resDescPtr, int procPid, int *pidArr, int *procCounter);

#endif
