/*  Author: Chase Richards
    Project: Homework 5 CS4760
    Date April 4, 2020
    Filename: user.c  */

#include "oss.h"

int main(int argc, char *argv[])
{
    msg message;
    int receiveMessage, sendMessage;
   
    int *resDescPtr;
    clksim *clockPtr;
        
    /* Get and attach to the process control block table shared memory */
    resDescSegment = shmget(resDescKey, sizeof(int), IPC_CREAT | 0666);
    if(resDescSegment < 0)
    {
        perror("user: Error: Failed to get resource descriptor segment (shmget)");
        exit(EXIT_FAILURE);
    }
    resDescPtr = shmat(resDescSegment, NULL, 0);
    if(resDescPtr < 0)
    {
        perror("user: Error: Failed to attach resource descriptor (shmat)");
        exit(EXIT_FAILURE);
    }
    
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

   
        
    /* Send the message back and check for failure/
    sendMessage = msgsnd(msgqSegment, &message, sizeof(message.valueofMsg), 0);
    if(sendMessage == -1)
    {
        perror("user: Error: Failed to send message (msgsnd)");
        exit(EXIT_FAILURE);
    } */

    printf("A process\n");

    return 0;
}

/* Message being sent to oss requesting resources */
int requestToOss(int process, int procPid, int resource)
{
    int sendmessage, receivemessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 1, .msgDetails = requestResource, .resource = resource, .process = process, .sendingProcess = procPid};
    
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
