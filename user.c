/*  Author: Chase Richards
    Project: Homework 5 CS4760
    Date April 4, 2020
    Filename: user.c  */

#include "oss.h"

int main(int argc, char *argv[])
{
    msg message;
   
    clksim *clockPtr;    
 
    /* Get and attach to the clock simulation shared memory */
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
    if(clockSegment < 0)
    {
        perror("user: Error: Failed to get clock segment (shmget)");
        exit(EXIT_FAILURE);
    }
    clockPtr = shmat(clockSegment, NULL, 0);
    if(clockPtr < 0)
    {
        perror("user: Error: Failed to attach clock (shmat)");
        exit(EXIT_FAILURE);
    }      
    
    //Semaphore declarations 
    sem_t* sem;
    char *semaphoreName = "semUser";

    clksim startClock;
    clksim totalClock;
    int rCounter;
    int replyToOss;
    int boundB;   
    int procPid, resource, process;
    msgqSegment = atoi(argv[1]);
    procPid = atoi(argv[2]);
    process = getpid();
    int procsResources[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};    

    int i; // loops
    srand(time(0) + (clockPtr-> nanosec / 500000) + 100000);
    //Starting clock value for the process is now
    startClock = *clockPtr;

    //Continuous loop until it's time to terminate
    while(1)
    {
        //Get the totaltime for the process and check to ensure it's run for at least a second
        totalClock = subTime((*clockPtr), startClock);
        boundB = (rand() % 100) + 1;
        if((clockPtr-> nanosec % 250000000 == 0) && (totalClock.sec >= 1))
        {
            //Determine if the process is to terminate
            boundB = (rand() % 100) + 1;
            //If it is time to terminate, then send the termination message
            if(boundB >= 40)
            {
                terminateToOss(process, procPid);
                return 0;               
            }
        }
        //Random chance of requesting a resource
        boundB = (rand() % 100) + 1;
        if(boundB >= 40)
        {
            //Get a random resource and request it from oss
            resource = rand() % 20;
            replyToOss = requestToOss(process, procPid, resource);
            if(replyToOss == 3)
            {
                //increment the process resources
                rCounter++;
                procsResources[resource]++;
            }
            //If the reuqest is denied wait to receive a message
            else if(replyToOss == 4)
            {
                int receivemessage;
                //Wait for the resource
                receivemessage = msgrcv(msgqSegment, &message, sizeof(msg), process, 0);
                if(receivemessage == -1)
                {
                    perror("user: Error: Failed to receive message (msgrcv)\n");
                    exit(EXIT_FAILURE);
                }
                //Increment the process resources
                rCounter++;
                procsResources[resource]++;
            }
        }
        /* Else if it has resources then release */
        else if(procsResources > 0)
        {
            for(i = 0; i < 20; i++)
            {
                if(procsResources[i] > 0)
                {
                    resource = i;
                    break;
                }
            }
            //Send the release message to oss and if granted then release the resource
            replyToOss = releaseToOss(process, procPid, resource);
            if(replyToOss == 3)
            {
                rCounter--;
                procsResources[resource]--;
            }
        }
 
    
        //Open the semaphore and increment the shared memory clock
        sem = sem_open(semaphoreName, O_CREAT, 0700, 1);    
        if(sem == SEM_FAILED)
        {
            perror("user: Error: Failed to open semaphore\n");
            exit(EXIT_FAILURE);
        }
        //Increment clock
        clockIncrementor(clockPtr, 1000000);
        //Signal semaphore
        sem_unlink(semaphoreName);
    }
    return 0;
}

/* Message being sent to oss requesting resources */
int requestToOss(int process, int procPid, int resource)
{
    int sendmessage, receivemessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 1, .msgDetails = 0, .resource = resource, .process = procPid, .processesPid = process};
    
    /* Send the message and check for failure */
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("user: Error: Failed to send message (msgsnd)\n");
        exit(EXIT_FAILURE);
    }
    /* Receive the message from oss */
    receivemessage = msgrcv(msgqSegment, &message, sizeof(msg), process, 0);
    if(receivemessage == -1)
    {
        perror("user: Error: Failed to receive message (msgrcv)\n");
        exit(EXIT_FAILURE);
    }
    
    return message.msgDetails;
}

/* Message being sent to oss releasing resources */
int releaseToOss(int process, int procPid, int resource)
{
    int sendmessage, receivemessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 1, .msgDetails = 1, .resource = resource, .process = procPid, .processesPid = process};
    
    /* Send the message and check for failure */
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("user: Error: Failed to send message (msgsnd)\n");
        exit(EXIT_FAILURE);
    }
    /* Receive the message from oss */
    receivemessage = msgrcv(msgqSegment, &message, sizeof(msg), process, 0);
    if(receivemessage == -1)
    {
        perror("user: Error: Failed to receive message (msgrcv)\n");
        exit(EXIT_FAILURE);
    }
    
    return message.msgDetails;
}

/* Message being sent to oss terminating process */
void terminateToOss(int process, int procPid)
{
    int sendmessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 1, .msgDetails = 2, .process = procPid, .processesPid = process};
    
    /* Send the message and check for failure */
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("user: Error: Failed to send message (msgsnd)\n");
        exit(EXIT_FAILURE);
    }
        
    return;
}

