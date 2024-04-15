#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sched.h>
#include <syslog.h>
#include <unistd.h>

#define NUM_THREADS 128

typedef struct
{
    int threadIdx;
} threadParams_t;


// POSIX thread declarations and scheduling attributes
//
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

pthread_t mainthread;
pthread_t startthread;


pthread_attr_t fifo_sched_attr;

struct sched_param fifo_param;

#define SCHED_POLICY SCHED_FIFO

void log_uname(void) {
  FILE *fp;
  const char* cmd = "uname -a";
  fp = popen(cmd, "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    exit(1);
  }
  char output[1024];
  while (fgets(output, sizeof(output), fp)) {                                                                                                                                               
    syslog(LOG_INFO, "%s", output);
  }
  pclose(fp);
}

void init_log()
{
    int result = system("echo > /dev/null | sudo tee /var/log/syslog"); // clear syslog
    if (result < 0){
      printf("Error while executing system command");
    }
    openlog("pthread: [COURSE:1][ASSIGNMENT:4] Weslley Araujo", LOG_NDELAY, LOG_DAEMON);
    log_uname();
}

void print_scheduler(void)
{
    int schedType = sched_getscheduler(getpid());

    switch(schedType)
    {
        case SCHED_FIFO:
            printf("Pthread policy is SCHED_FIFO\n");
            break;
        case SCHED_OTHER:
            printf("Pthread policy is SCHED_OTHER\n");
            break;
        case SCHED_RR:
            printf("Pthread policy is SCHED_RR\n");
            break;
        default:
            printf("Pthread policy is UNKNOWN\n");
    }
}


void set_scheduler(void)
{
    int max_prio, rc, cpuidx;
     cpu_set_t cpuset;
    
    pthread_attr_init(&fifo_sched_attr);
    pthread_attr_setinheritsched(&fifo_sched_attr, PTHREAD_EXPLICIT_SCHED); //Threads that are created using fifo_sched_attr 
                                                                            //take their scheduling attributes from the values 
                                                                            //specified by the attributes object

    pthread_attr_setschedpolicy(&fifo_sched_attr, SCHED_POLICY);
    
    CPU_ZERO(&cpuset);
    cpuidx=(3);
    CPU_SET(cpuidx, &cpuset);
    pthread_attr_setaffinity_np(&fifo_sched_attr, sizeof(cpu_set_t), &cpuset);

    max_prio=sched_get_priority_max(SCHED_POLICY);
    fifo_param.sched_priority=max_prio;    

    if((rc=sched_setscheduler(getpid(), SCHED_POLICY, &fifo_param)) < 0)
        perror("sched_setscheduler");

    pthread_attr_setschedparam(&fifo_sched_attr, &fifo_param);

    printf("ADJUSTED "); print_scheduler();
}

void destroy_attributes()
{
    pthread_attr_destroy(&fifo_sched_attr);
}

void *counterThread(void *threadp)
{
    int sum=0, i;
    threadParams_t *threadParams = (threadParams_t *)threadp;
    
    for(i=1; i < (threadParams->threadIdx)+1; i++)
    {
        sum+= i;
    }   
    syslog(LOG_INFO,"Thread idx=%d, sum[1...%d]=%d, Running on Core=%d", 
           threadParams->threadIdx,
           threadParams->threadIdx, sum, sched_getcpu());
    return EXIT_SUCCESS;
}


void *starterThread(void *threadp)
{
   int i;

   printf("starter thread running on CPU=%d\n", sched_getcpu());

   for(i=1; i < NUM_THREADS+1; i++)
   {
       threadParams[i-1].threadIdx = i; //set the index id

       pthread_create(&threads[i-1],   // pointer to thread descriptor
                      &fifo_sched_attr,     // use FIFO RT max priority attributes
                      counterThread, // thread function entry point
                      (void *)&(threadParams[i-1]) // parameters to pass in
                     );

   }

   for(i=1;i<NUM_THREADS+1;i++)
   {
       pthread_join(threads[i-1], NULL);
   }
    return EXIT_SUCCESS;
}


int main (int argc, char *argv[])
{
    init_log();
    printf("INIT "); print_scheduler();
   int rc;
   int j;
   cpu_set_t cpuset;

   set_scheduler();
    
   CPU_ZERO(&cpuset);
   
   
   // get affinity set for main thread
   mainthread = pthread_self();

   // Check the affinity mask assigned to the thread 
   rc = pthread_getaffinity_np(mainthread, sizeof(cpu_set_t), &cpuset);
   if (rc != 0)
       perror("pthread_getaffinity_np");
   else
   {
       printf("main thread running on CPU=%d, CPUs =", sched_getcpu());

       for (j = 0; j < CPU_SETSIZE; j++)
           if (CPU_ISSET(j, &cpuset))
               printf(" %d", j);

       printf("\n");
   }

    pthread_create(&startthread,   // pointer to thread descriptor
                  &fifo_sched_attr,     // use FIFO RT max priority attributes
                  starterThread, // thread function entry point
                  (void *)0 // parameters to pass in
                 );

    pthread_join(startthread, NULL);
    
   
    printf("\nTEST COMPLETE\n");
    destroy_attributes();
}
