/*  Author: Chase Richards
    Project: Homework 5 CS4760
    Date April 4, 2020
    Filename: oss.c  */

#include "oss.h"

char *outputLog = "logOutput.dat";

int main(int argc, char* argv[])
{
    int c;
    int n = 18; //Max Children in system at once
    //char *outputLog = "logOutput.dat";
    srand(time(0));
    while((c = getopt(argc, argv, "hn:")) != -1)
    {
        switch(c)
        {
            case 'h':
                displayHelpMessage();
                return (EXIT_SUCCESS);
            case 'n':
                n = atoi(optarg);
                break;
            default:
                printf("Using default values");
                break;               
        }
    }
  
    /* Signal for terminating, freeing up shared mem, killing all children 
       if the program goes for more than two seconds of real clock */
    signal(SIGALRM, sigHandler);
    alarm(10);

    /* Signal for terminating, freeing up shared mem, killing all 
       children if the user enters ctrl-c */
    signal(SIGINT, sigHandler);  

    manager(n); //Call scheduler function
    removeAllMem(); //Remove all shared memory, message queue, kill children, close file
    return 0;
}

void manager(int maxProcsInSys)
{
    filePtr = openLogFile(outputLog); //open the output file
    
    int receivedMsg; //Recieved from child telling scheduler what to do
    
    /* Create resource descriptor shared memory */ 
    resDesc *resDescPtr;
    resDescSegment = shmget(resDescKey, sizeof(int) * maxProcsInSys, IPC_CREAT | 0666);
    if(resDescSegment < 0)
    {
        perror("oss: Error: Failed to get resource desriptor segment (shmget)");
        removeAllMem();
    }
    resDescPtr = shmat(resDescSegment, NULL, 0);
    if(resDescPtr < 0)
    {
        perror("oss: Error: Failed to attach to resource descriptor segment (shmat)");
        removeAllMem();
    }
    resDescConstruct(resDescPtr);
 
    /* Create the simulated clock in shared memory */
    clksim *clockPtr;
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
    if(clockSegment < 0)
    {
        perror("oss: Error: Failed to get clock segment (shmget)");
        removeAllMem();
    }
    clockPtr = shmat(clockSegment, NULL, 0);
    if(clockPtr < 0)
    {
        perror("oss: Error: Failed to attach clock segment (shmat)");
        removeAllMem();
    }
    clockPtr-> sec = 0;
    clockPtr-> nanosec = 0;   
 
    /* Constant for the time between each new process and the time for
       spawning the next process, initially spawning one process */   
    clksim maxTimeBetweenNewProcesses = {.sec = 0, .nanosec = 500000000};
    clksim spawnNextProc = {.sec = 0, .nanosec = 0};

    /* Create the message queue */
    msgqSegment = msgget(messageKey, IPC_CREAT | 0777);
    if(msgqSegment < 0)
    {
        perror("oss: Error: Failed to get message segment (msgget)");
        removeAllMem();
    }
    msg message;

    int outputLines = 0; //Counts the lines written to file to make sure we don't have an infinite loop
    int procCounter = 0; //Counts the processes
    int i = 0; //For loops
    int processExec; //exec  nd check for failurei
    int deadlockDetector = 0;
    int procPid;
    int pid;    
    char msgqSegmentStr[10];
    char procPidStr[3];
    sprintf(msgqSegmentStr, "%d", msgqSegment);    

    int *pidArr;
    pidArr = (int *)malloc(sizeof(int) * maxProcsInSys);
    for(i = 0; i < maxProcsInSys; i++)
        pidArr[i] = -1;


    //Loop runs constantly until it has to terminate
    while(1)
    {
        //Only 18 processes in the system at once and spawn random between 0 and 5000000000
        if((procCounter < maxProcsInSys) && ((clockPtr-> sec > spawnNextProc.sec) || (clockPtr-> sec == spawnNextProc.sec && clockPtr-> nanosec >= spawnNextProc.nanosec)))
        {
            procPid = generateProcPid(pidArr, maxProcsInSys);
            if(procPid < 0)
            {
                perror("oss: Error: Max number of pids in the system\n");
                removeAllMem();               
            }     
            /* Copy the generated pid into a string for exec, fork, check
               for failure, execl and check for failure */      
            sprintf(procPidStr, "%d", procPid);
            pid = fork();
            if(fork < 0)
            {
                perror("oss: Error: Failed to fork process\n");
                removeAllMem();
            }
            else if(pid == 0)
            {
                processExec = execl("./user", "user", msgqSegmentStr, procPidStr, (char *)NULL);
                if(processExec < 0)
                {
                    perror("oss: Error: Failed to execl\n");
                    removeAllMem();
                }
            }
            procCounter++;
            //Put the pid in the pid array in the spot based on the generated pid
            pidArr[procPid] = pid;
     
            //Get the time for the next process to run
            spawnNextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockPtr));
   
            if(outputLines < 100000)
            {
                fprintf(filePtr, "OSS has spawned process P%d at time %d:%d\n", procPid, clockPtr-> sec, clockPtr-> nanosec);
                outputLines++;
            }
        }
        else if((msgrcv(msgqSegment, &message, sizeof(msg), 1, IPC_NOWAIT)) > 0) 
        {
            //printf("Message Handling");
            if(message.msgDetails == requestResource)
            {
                //Write the request to the log file for verbose
                if(outputLines < 100000)
                {
                    fprintf(filePtr, "Oss has detected P%d requesting R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                    outputLines++;
                }
                //Requesting the shared resource
                if(resDescPtr-> resSharedVector[message.resource] == 1)
                {
                    //Make sure it doesn't have more than it should
                    if(resDescPtr-> allocatedMatrix[message.process][message.resource] < 5)
                    {
                        //Grant the request for resource
                        resDescPtr-> allocatedMatrix[message.process][message.resource] += 1;
                        messageToProcess(message.sendingProcess, grantedRequest);
                        if(outputLines < 100000)
                        {
                            fprintf(filePtr, "Oss granting P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                            outputLines++;
                        }                
                    }
                    //Otherwise deny the request and 
                    else
                    {
                        resDescPtr-> requestingMatrix[message.process][message.resource] += 1;
                        messageToProcess(message.sendingProcess, denyRequest);
                        if(outputLines < 100000)
                        {
                            fprintf(filePtr, "Oss denying P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                            outputLines++;
                        }   
                    }
                }
                /* Requesting a nonshareable resource */
                else if(resDescPtr-> allocatedVector[message.resource] > 0)
                {
                    resDescPtr-> allocatedVector[message.resource] -= 1;
                    resDescPtr-> allocatedMatrix[message.process][message.resource] += 1;
                    messageToProcess(message.sendingProcess, grantedRequest);
                    if(outputLines < 100000)
                    {
                        fprintf(filePtr, "Oss granting P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                        outputLines++;
                    }
                }
                /* Otherwise there are not enough of the nonshareable resource available */
                else
                {
                    resDescPtr-> requestingMatrix[message.process][message.resource] += 1;
                    messageToProcess(message.sendingProcess, denyRequest);
                    if(outputLines < 100000)
                    {
                        fprintf(filePtr, "Oss denying P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                        outputLines++;
                    }
                }     
   
            }
            /* If the message is for releasing resources */
            else if(message.msgDetails == releaseResource)
            {
                //If there is resources allocated
                if(resDescPtr-> allocatedMatrix[message.process][message.resource] > 0)
                {
                    //If it's not shared, release, and let the process know
                    if(resDescPtr-> resSharedVector[message.resource] == 0)
                        resDescPtr-> allocatedVector[message.resource] += 1;
                    resDescPtr-> allocatedMatrix[message.process][message.resource] -= 1;
                    messageToProcess(message.sendingProcess, grantedRequest);
                    if(outputLines < 100000)
                    {
                        fprintf(filePtr, "Oss has acknowledged P%d releasing R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                        outputLines++;
                    }
                }
            }
            /* If it is time to terminate the process */
            else if(message.msgDetails == terminateProcess)
            {
                int pid;
                //Resources are available once again
                for(i = 0; i < 20; i++)
                {
                    if(resDescPtr-> resSharedVector[i] == 0)
                        resDescPtr-> allocatedVector[i] += resDescPtr-> allocatedMatrix[message.process][i];
                    resDescPtr-> allocatedMatrix[message.process][i] = 0;
                    resDescPtr-> requestingMatrix[message.process][i] = 0;
                }
                //The simulated pid is available now
                pidArr[message.process] = -1;
                procCounter -= 1;
                pid = waitpid(message.sendingProcess, NULL, 0);
                if(outputLines < 100000)
                {
                    fprintf(filePtr, "Oss has acknowledged P%d terminating at time %d:%d\n", message.process, clockPtr-> sec, clockPtr-> nanosec);
                    outputLines++;
                }
            }
        }
        

        //Check for a deadlock every second
        if(clockPtr-> nanosec == 0)
        {
            //printf("Check for deadlock");
            deadlockDetector = deadlock(resDescPtr, maxProcsInSys, clockPtr, pidArr, &procCounter, &outputLines);
            if(deadlockDetector == 1 && outputLines < 100000)
            {
                printf("Not sure yet\n");
            }
        }        

        clockIncrementor(clockPtr, 1000000);
    }    
    
    return;
   
}

//Generate a simulated process pid between 0-18
int generateProcPid(int *pidArr, int totalPids)
{
    int i;
    for(i = 0; i < totalPids; i++)
    {
        if(pidArr[i] == -1)
            return i;
    }
    return -1;
}

/* Determines when to launch the next process based on a random value
 *    between 0 and maxTimeBetweenNewProcs and returns the time that it should launch */
clksim nextProcessStartTime(clksim maxTimeBetweenProcs, clksim curTime)
{
    clksim nextProcTime = {.sec = (rand() % (maxTimeBetweenProcs.sec + 1)) + curTime.sec, .nanosec = (rand() % (maxTimeBetweenProcs.nanosec + 1)) + curTime.nanosec};
    if(nextProcTime.nanosec >= 1000000000)
    {
        nextProcTime.sec += 1;
        nextProcTime.nanosec -= 1000000000;
    }
    return nextProcTime;
}

/* For sending the message to the process */
void messageToProcess(int receiver, int response)
{
    int sendmessage;
    //Process is -1 because we didn't generate one, no resource, oss is sending process of 1
    msg message = {.typeofMsg = receiver, .msgDetails = response, .process = -1, .resource = -1, .sendingProcess = 1};
    
    //Send the message and check for failure
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("oss: Error: Failed to send message (msgsnd)\n");
        removeAllMem();
    }
    return;
}

/* req_lt_avail from notes */
int req_lt_avail(int req[], int avail[], int shared[], int held[])
{
    int i;
    for(i = 0; i < 20; i++)
    {
        if(shared[i] == 1 && req[i] > 0 && held[i] == 5)
            break;
        else if(req[i] > avail[i])
            break;
    }

    if(i == 20)
        return 1;
    else
        return 0;
}

/* Deadlock algorithm from notes */
int deadlock(resDesc *resDescPtr, int nProcs, clksim *clockPtr, int *pidArr, int *procCounter, int *outputLines)
{
    int i, p;
    int work[20];
    int finish[nProcs];
    int deadlockedProcs[nProcs];
    int counter = 0;

    for(i = 0; i < 20; i++)
        work[i] = resDescPtr-> allocatedVector[i];
    for(i = 0; i < nProcs; i++)
        finish[i] = 0;

    for(p = 0; p < nProcs; p++)
    {
        if(finish[p])
            continue;
        if((req_lt_avail(resDescPtr-> requestingMatrix[p], resDescPtr-> allocatedVector, resDescPtr-> resSharedVector, resDescPtr-> allocatedMatrix[p])))
        {
            finish[p] = 1;
            for(i = 0; i < 20; i++)
                work[i] += resDescPtr-> allocatedMatrix[p][i];
            p = -1;
        }
    }

    for(p = 0; p < nProcs; p++)
    {
        if(!finish[p])
            deadlockedProcs[counter++] = p;
    }

    /* Deadlock resolution: Kill all deadlocked processes */
    if(counter > 0)
    {
        for(i = 0; i < counter; i++)
        {
            if((*outputLines) < 100000)
            {
                fprintf(filePtr, "Oss detected that P%d is deadlocked\n", deadlockedProcs[i]);
                (*outputLines)++;
            }
            
            int j; 
            //Terminate by killing the process
            kill(pidArr[deadlockedProcs[i]], SIGKILL);
            waitpid(pidArr[deadlockedProcs[i]], NULL, 0);
            //Release the resources
            for(j = 0; j < 20; j++)
            {
                if(resDescPtr-> resSharedVector[j] == 0)
                    resDescPtr-> allocatedVector[j] += resDescPtr-> allocatedMatrix[deadlockedProcs[i]][j];
                resDescPtr-> allocatedMatrix[deadlockedProcs[i]][j] = 0;
                resDescPtr-> requestingMatrix[deadlockedProcs[i]][j] = 0;
            }

            //Add the pid back to the available pids
            pidArr[deadlockedProcs[i]] = -1;
            (*procCounter)--;
            
            if((*outputLines) < 100000)
            {
                fprintf(filePtr, "Oss killing process P%d\n", deadlockedProcs[i]);
                (*outputLines)++;
            }
        }
        return 1;
    }
    return 0;   
}

/* Open the log file that contains the output and check for failure */
FILE *openLogFile(char *file)
{
    filePtr = fopen(file, "a");
    if(filePtr == NULL)
    {
        perror("oss: Error: Failed to open output log file");
        exit(EXIT_FAILURE);
    }
    return filePtr;
}

/* When there is a failure, call this to make sure all memory is removed */
void removeAllMem()
{
    shmctl(resDescSegment, IPC_RMID, NULL);   
    shmctl(clockSegment, IPC_RMID, NULL);
    msgctl(msgqSegment, IPC_RMID, NULL);
    kill(0, SIGTERM);
    fclose(filePtr);
    exit(EXIT_SUCCESS);
} 

/* Signal handler, that looks to see if the signal is for 2 seconds being up or ctrl-c being entered.
   In both cases, I connect to shared memory so that I can write the time that it is killed to the file
   and so that I can disconnect and remove the shared memory. */
void sigHandler(int sig)
{
    if(sig == SIGALRM)
    {
        printf("Timer is up.\n"); 
        printf("Killing children, removing shared memory and message queue.\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
    
    if(sig == SIGINT)
    {
        printf("Ctrl-c was entered\n");
        printf("Killing children, removing shared memory and message queue\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
}


/* For the -h option that can be entered */
void displayHelpMessage() 
{
    printf("\n---------------------------------------------------------\n");
    printf("See below for the options:\n\n");
    printf("-h    : Instructions for running the project and terminate.\n"); 
    printf("-n x  : Number of processes allowed in the system at once (Default: 18).\n");
    printf("-o filename  : Option to specify the output log file name (Default: ossOutputLog.dat).\n");
    printf("\n---------------------------------------------------------\n");
    printf("Examples of how to run program(default and with options):\n\n");
    printf("$ make\n");
    printf("$ ./oss\n");
    printf("$ ./oss -n 10 -o outputLog.dat\n");
    printf("$ make clean\n");
    printf("\n---------------------------------------------------------\n"); 
    exit(0);
}
