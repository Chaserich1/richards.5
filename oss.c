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
    int verbose = 0;
    srand(time(0));
    while((c = getopt(argc, argv, "hn:v")) != -1)
    {
        switch(c)
        {
            case 'h':
                displayHelpMessage();
                return (EXIT_SUCCESS);
            case 'n':
                n = atoi(optarg);
                break;
            case 'v':
                verbose = 1;
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
    
    manager(n, verbose); //Call scheduler function
    removeAllMem(); //Remove all shared memory, message queue, kill children, close file
    return 0;
}
//Stats
int granted = 0;
int normalTerminations = 0;
int deadlockTerminations = 0;
int deadlockAlgRuns = 0;

/* Does the fork, exec and handles the messaging to and from user */
void manager(int maxProcsInSys, int verbose)
{
    filePtr = openLogFile(outputLog); //open the output file
    
    int receivedMsg; //Recieved from child telling scheduler what to do
    
    /* Create resource descriptor shared memory */ 
    resDesc *resDescPtr;
    resDescSegment = shmget(resDescKey, sizeof(int) * 18, IPC_CREAT | 0666);
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

    sem_t *sem;
    char *semaphoreName = "semOss";

    int outputLines = 0; //Counts the lines written to file to make sure we don't have an infinite loop
    int procCounter = 0; //Counts the processes

    //Statistics
    int granted1;
    int totalProcs = 0;

    int i, j; //For loops
    int processExec; //exec  nd check for failurei
    int deadlockDetector = 0; //deadlock flag
    int procPid; //generated pid
    int pid; //actual pid
    char msgqSegmentStr[10]; //for execing to the child
    char procPidStr[3]; //for execing the generated pid to child
    sprintf(msgqSegmentStr, "%d", msgqSegment);
    //Array of the pids in the generated pids index, initialized to -1 for available
    int *pidArr;
    pidArr = (int *)malloc(sizeof(int) * maxProcsInSys);
    for(i = 0; i < maxProcsInSys; i++)
        pidArr[i] = -1;

    printMatrix((*resDescPtr), maxProcsInSys, 20);
    outputLines += 19;

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
            totalProcs++;
            procCounter++;
            //Put the pid in the pid array in the spot based on the generated pid
            pidArr[procPid] = pid;
     
            //Get the time for the next process to run
            spawnNextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockPtr));
   
            if(outputLines < 100000 && verbose == 1)
            {
                fprintf(filePtr, "OSS has spawned process P%d at time %d:%d\n", procPid, clockPtr-> sec, clockPtr-> nanosec);
                outputLines++;
            }
        }
        else if((msgrcv(msgqSegment, &message, sizeof(msg), 1, IPC_NOWAIT)) > 0) 
        {
            //printf("Message Handling");
            if(message.msgDetails == 0)
            {
                //Write the request to the log file for verbose
                if(outputLines < 100000 && verbose == 1)
                {
                    fprintf(filePtr, "Oss has detected P%d requesting R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                    outputLines++;
                }
                //Requesting the shared resource
                if(resDescPtr-> resSharedVector[message.resource] == 1)
                {
                    //Make sure it doesn't have more than available of the shared resource
                    if(resDescPtr-> allocatedMatrix[message.process][message.resource] < 7)
                    {
                        //Grant the request for resource and send message to process that it was granted
                        resDescPtr-> allocatedMatrix[message.process][message.resource] += 1;
                        granted++;
                        messageToProcess(message.processesPid, 3);
                        if(outputLines < 100000 && verbose == 1)
                        {
                            fprintf(filePtr, "Oss granting P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                            outputLines++;
                        }                
                    }
                    //Otherwise deny the request and send denied message to process
                    else
                    {
                        resDescPtr-> requestingMatrix[message.process][message.resource] += 1;
                        messageToProcess(message.processesPid, 4);
                        if(outputLines < 100000 && verbose == 1)
                        {
                            fprintf(filePtr, "Oss denying P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                            outputLines++;
                        }   
                    }
                }
                /* Requesting a nonshareable resource, check if there are any left */
                else if(resDescPtr-> allocatedVector[message.resource] > 0)
                {
                    //Remove it from the allocated vector and add it to the matrix for the process
                    resDescPtr-> allocatedVector[message.resource] -= 1;
                    resDescPtr-> allocatedMatrix[message.process][message.resource] += 1;
                    //Let child know the request was granted
                    granted++;
                    messageToProcess(message.processesPid, 3);
                    if(outputLines < 100000 && verbose == 1)
                    {
                        fprintf(filePtr, "Oss granting P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                        outputLines++;
                    }
                }
                /* Otherwise there are no instances of the nonshareable resource available */
                else
                {
                    //Add one to the request for the process and send deny message to child
                    resDescPtr-> requestingMatrix[message.process][message.resource] += 1;
                    messageToProcess(message.processesPid, 4);
                    if(outputLines < 100000 && verbose == 1)
                    {
                        fprintf(filePtr, "Oss denying P%d request for R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                        outputLines++;
                    }
                }     
   
            }
            /* If the message is for releasing resources */
            else if(message.msgDetails == 1)
            {
                //If there are resources allocated to the process
                if(resDescPtr-> allocatedMatrix[message.process][message.resource] > 0)
                {
                    //If it's not shared, release, remove from allocated matrix and let the process know
                    if(resDescPtr-> resSharedVector[message.resource] == 0)
                        resDescPtr-> allocatedVector[message.resource] += 1;
                    resDescPtr-> allocatedMatrix[message.process][message.resource] -= 1;
                    messageToProcess(message.processesPid, 3);
                    if(outputLines < 100000 && verbose == 1)
                    {
                        fprintf(filePtr, "Oss has acknowledged P%d releasing R%d at time %d:%d\n", message.process, message.resource, clockPtr-> sec, clockPtr-> nanosec);
                        outputLines++;
                    }
                }
            }
            /* If it is time to terminate the process */
            else if(message.msgDetails == 2)
            {
                int pid;
                //Resources are available once again
                for(i = 0; i < 20; i++)
                {
                    //Removce all of the requests and allocated (release all resources)
                    if(resDescPtr-> resSharedVector[i] == 0)
                        resDescPtr-> allocatedVector[i] += resDescPtr-> allocatedMatrix[message.process][i];
                    resDescPtr-> allocatedMatrix[message.process][i] = 0;
                    resDescPtr-> requestingMatrix[message.process][i] = 0;
                }
                //The simulated pid is available now, wait for process completion
                pidArr[message.process] = -1;
                procCounter -= 1;
                normalTerminations++;
                pid = waitpid(message.processesPid, NULL, 0);
                if(outputLines < 100000 && verbose == 1)
                {
                    fprintf(filePtr, "Oss has acknowledged P%d terminating normally at time %d:%d\n", message.process, clockPtr-> sec, clockPtr-> nanosec);
                    outputLines++;
                }
            }
        }
       
        /* Every 20 granted requests, print the allocation matrix */ 
        if(granted % 20 == 0 && granted != 0 && granted != granted1 && verbose == 1)
        {
            granted1 = granted;
            printMatrix((*resDescPtr), maxProcsInSys, 20);
            outputLines += 19;
        }
        /*
        for(i = 0; i < 18; i++)
        {
            for(j = 0; j < 20; j++)
            {
                granted += resDescPtr-> allocatedMatrix[i][j];
                if(granted >= 20)
                {
                    printMatrix((*resDescPtr), maxProcsInSys, 20);
                    outputLines += 19;
                    break;   
                }
            }
        }*/        

        //printf("%d\n", clockPtr-> nanosec);
        //Check for a deadlock every second
        if(clockPtr-> nanosec == 0)
        {
            //printf("Check for deadlock");
            deadlockDetector = deadlock(resDescPtr, maxProcsInSys, clockPtr, pidArr, &procCounter, &outputLines);
         
            if(deadlockAlgRuns <= clockPtr-> sec)
            {
                deadlockAlgRuns++;
            }    
          
            while(deadlockDetector == 1 && outputLines < 100000)
            { 
                fprintf(filePtr, "   Oss running deadlock detector after process kill\n");
                outputLines++;
                deadlockDetector = deadlock(resDescPtr, maxProcsInSys, clockPtr, pidArr, &procCounter, &outputLines);
                if(deadlockDetector == 0)
                {
                    fprintf(filePtr, "   System is no longer in deadlock\n");   
                    outputLines++;
                }
            }
            
        }        
        
        //Open the semaphore for writing to shared memory clock
        sem = sem_open(semaphoreName, O_CREAT, 0644, 1);
        if(sem == SEM_FAILED)
        {
            perror("oss: Error: Failed to open semaphore (sem_open)\n");
            exit(EXIT_FAILURE);
        }
        //Increment the clock
        clockIncrementor(clockPtr, 1000000);
        //Signal the semaphore we are finished
        sem_post(sem);
    }    
    
    //printf("Total Granted Requests: %d\n", granted);
    //printf("Total Normal Terminations: %d\n", normalTerminations);
    //printf("Total Deadlock Algorithm Runs: %d\n", deadlockAlgRuns);
    //printf("Total Deadlock Terminations: %d\n", deadlockTerminations);
    //printf("Pecentage of processes in deadlock that had to terminate on avg: \n");
    
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
    msg message = {.typeofMsg = receiver, .msgDetails = response, .process = -1, .resource = -1, .processesPid = 1};
    
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
    int releasedValues[20];

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
                fprintf(filePtr, "   Oss detected that P%d is deadlocked\n", deadlockedProcs[i]);
                fprintf(filePtr, "   Attempting to resolve deadlock\n");
                (*outputLines) += 2;
            }
            
            int j; 
            //Terminate by killing the process
            kill(pidArr[deadlockedProcs[i]], SIGKILL);
            waitpid(pidArr[deadlockedProcs[i]], NULL, 0);
            deadlockTerminations++;
            //Release the resources
            for(j = 0; j < 20; j++)
            {
                if(resDescPtr-> resSharedVector[j] == 0)
                {
                    resDescPtr-> allocatedVector[j] += resDescPtr-> allocatedMatrix[deadlockedProcs[i]][j];
                    releasedValues[j] += resDescPtr-> allocatedMatrix[deadlockedProcs[i]][j];
                }
                resDescPtr-> allocatedMatrix[deadlockedProcs[i]][j] = 0;
                resDescPtr-> requestingMatrix[deadlockedProcs[i]][j] = 0;
                //if(releasedValues[j] <= 7 && releasedValues[j] > 0)
                //    fprintf(filePtr, "      Resources released are: R%d:%d\n", j, releasedValues[j]);
            }

            //Add the pid back to the available pids
            pidArr[deadlockedProcs[i]] = -1;
            (*procCounter)--;
            
            if((*outputLines) < 100000)
            {
                fprintf(filePtr, "   Oss killing process P%d\n", deadlockedProcs[i]);
                (*outputLines)++;
            }
            return 1;
        }
        return 1;
    }
    return 0;   
}

/* Print Allocation Matrix according to the assignment sheet */
void printAllocatedMatrix(int allocatedMatrix[18][20], int processes, int resources)
{
    int mRow, mColumn;
    fprintf(filePtr, "Allocated Matrix\n   ");
    for(mColumn = 0; mColumn < resources; mColumn++)
    {
        fprintf(filePtr, "R%-2d ", mColumn);
    }
    fprintf(filePtr, "\n");
    for(mRow = 0; mRow < processes; mRow++)
    {
        fprintf(filePtr, "P%-2d ", mRow);
        for(mColumn = 0; mColumn < resources; mColumn++)
        {
            if(allocatedMatrix[mRow][mColumn] == 0)
                fprintf(filePtr, "0   ");
            else
                fprintf(filePtr, "%-3d ", allocatedMatrix[mRow][mColumn]);           
        }
        fprintf(filePtr, "\n");
    }
    return;
}

void printMatrix(resDesc resDescPtr, int processes, int resources)
{
    printAllocatedMatrix(resDescPtr.allocatedMatrix, processes, resources);
    return;
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
    sem_unlink("semOss");
    sem_unlink("semUser");
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
        printf("Killing children, removing shared memory, semaphore and message queue.\n");
        printf("Total Granted Requests: %d\n", granted);                                                                        printf("Total Normal Terminations: %d\n", normalTerminations);                                                          printf("Total Deadlock Algorithm Runs: %d\n", deadlockAlgRuns);                                                         printf("Total Deadlock Terminations: %d\n", deadlockTerminations);                                                      printf("Pecentage of processes in deadlock that had to terminate on avg: \n"); 
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
    
    if(sig == SIGINT)
    {
        printf("Ctrl-c was entered\n");
        printf("Killing children, removing shared memory, semaphore and message queue\n");
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
    printf("-v    : Option to turn verbose on: indicates requests and releases of resources (Default: off).\n");
    printf("\n---------------------------------------------------------\n");
    printf("Examples of how to run program(default and with options):\n\n");
    printf("$ make\n");
    printf("$ ./oss\n");
    printf("$ ./oss -n 10 -v\n");
    printf("$ make clean\n");
    printf("\n---------------------------------------------------------\n"); 
    exit(0);
}
