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

//Shared memory keys and shared memory segment ids
const key_t pcbtKey = 122032;
const key_t clockKey = 202123;
const key_t messageKey = 493343;
int pcbtSegment, clockSegment, msgqSegment;

typedef struct
{
    long typeofMsg;
    int valueofMsg;
} msg;

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
clksim addTime(clksim a, clksim b)
{
    clksim sum = {.sec = a.sec + b.sec, .nanosec = a.nanosec + b.nanosec};
    if(sum.nanosec >= 1000000000)
    {
        sum.nanosec -= 1000000000;
        sum.sec += 1;
    }
    return sum;
}

clksim subTime(clksim a, clksim b)
{
    clksim sub = {.sec = a.sec - b.sec, .nanosec = a.nanosec - b.nanosec};
    if(sub.nanosec < 0)
    {
        sub.nanosec += 1000000000;
        sub.sec -= 1;
    }
    return sub;
}


#endif
