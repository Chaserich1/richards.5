/*  Author: Chase Richards
    Project: Homework 5 CS4760
    Date April 4, 2020
    Filename: user.c  */

#include "oss.h"

int main(int argc, char *argv[])
{
    msg message;
    int receiveMessage, sendMessage;
   
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

    clksim startClock;
    clksim totalClock;
    const int chanceOfTermProcess = 50;
    const int chanceOfReqResources = 55;
    int rCounter;
    int replyToOss;
    int boundB;   
 
    int procPid, resource, process;
    msgqSegment = atoi(argv[1]);
    procPid = atoi(argv[2]);
    process = getpid();
    int procsResources[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};    

    srand(time(0) + (clockPtr-> nanosec / 100000) + process);
    int i; //loops
    //Starting clock value for the process is now
    startClock = *clockPtr;

    //Continuous loop
    while(1)
    {
        //Get the totaltime for the process and check
        totalClock = subTime((*clockPtr), startClock);
        if((totalClock.sec >= 1) && (clockPtr-> nanosec % 250000000 == 0))
        {
            //Determine if the process is to terminate
            boundB = (rand() % 100) + 1;
            //If it is time to terminate, then send the termination message
            if(boundB <= chanceOfTermProcess)
            {
                terminateToOss(process, procPid);
                return 0;               
            }
        }
        //Random chance of requesting a resource
        boundB = (rand() % 100) + 1;
        if(boundB <= chanceOfReqResources)
        {
            //Get a random resource and request it from oss
            resource = rand() % 20;
            replyToOss = requestToOss(process, procPid, resource);
            if(replyToOss == grantedRequest)
            {
                rCounter++;
                procsResources[resource]++;
            }
            //If the reuqest is denied wait
            else if(replyToOss == denyRequest)
            {
                waiting(process);
                rCounter++;
                procsResources[resource]++;
            }
        }
        /* Else if it has resources then release one */
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
            if(replyToOss == grantedRequest)
            {
                rCounter--;
                procsResources[resource]--;
            }
        }
 
    }

    return 0;
}

/* Message being sent to oss requesting resources */
int requestToOss(int process, int procPid, int resource)
{
    int sendmessage, receivemessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 1, .msgDetails = requestResource, .resource = resource, .process = procPid, .sendingProcess = process};
    
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
    msg message = {.typeofMsg = 1, .msgDetails = releaseResource, .resource = resource, .process = procPid, .sendingProcess = process};
    
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
    int sendmessage, receivemessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 1, .msgDetails = terminateProcess, .process = procPid, .sendingProcess = process};
    
    /* Send the message and check for failure */
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("user: Error: Failed to send message (msgsnd)\n");
        exit(EXIT_FAILURE);
    }
        
    return;
}

void waiting(int process)
{
    msg message;
    int receivemessage;
    receivemessage = msgrcv(msgqSegment, &message, sizeof(msg), process, 0);
    if(receivemessage == -1)
    {
        perror("user: Error: Failed to receive message (msgrcv)\n");
        exit(EXIT_FAILURE);
    }
}
