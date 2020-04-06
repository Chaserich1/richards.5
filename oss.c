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
    alarm(2);

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
    
    int maxProcs = 100; //Only 100 processes should be created
    int outputLines = 0; //Counts the lines written to file to make sure we don't have an infinite loop
    int completedProcs = 0; //Processes that have finished
    int procCounter = 0; //Counts the processes

    int i = 0; //For loops 
    int processExec; //exec and check for failure

    int receivedMsg; //Recieved from child telling scheduler what to do
    
    /* Create process control block shared memory */ 
    int *pcbtPtr;
    pcbtSegment = shmget(pcbtKey, sizeof(int) * maxProcsInSys, IPC_CREAT | 0666);
    if(pcbtSegment < 0)
    {
        perror("oss: Error: Failed to get process control table segment (shmget)");
        removeAllMem();
    }
    pcbtPtr = shmat(pcbtSegment, NULL, 0);
    if(pcbtPtr < 0)
    {
        perror("oss: Error: Failed to attach to control table segment (shmat)");
        removeAllMem();
    }
 
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

    /* Create the message queue */
    msgqSegment = msgget(messageKey, IPC_CREAT | 0777);
    if(msgqSegment < 0)
    {
        perror("oss: Error: Failed to get message segment (msgget)");
        removeAllMem();
    }

    
   
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
    shmctl(pcbtSegment, IPC_RMID, NULL);   
    shmctl(clockSegment, IPC_RMID, NULL);
    msgctl(msgqSegment, IPC_RMID, NULL);
    fclose(filePtr);
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
