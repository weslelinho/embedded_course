#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <syslog.h>

#define COUNT  128

typedef struct
{
    int threadIdx;
} threadParams_t;

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

// POSIX thread declarations and scheduling attributes
//
pthread_t threads[COUNT];
threadParams_t threadParams[COUNT];


// Unsafe global
int gsum=0;

void *incThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;
    
    gsum+=threadParams->threadIdx;
    syslog(LOG_INFO,"Thread idx=%d, sum[1...%d]=%d\n", threadParams->threadIdx,threadParams->threadIdx, gsum);
    
}

void initLog()
{
    system("echo > /dev/null | sudo tee /var/log/syslog"); // clear syslog
    openlog("pthread: [COURSE:1][ASSIGNMENT:2] Weslley Araujo", LOG_NDELAY, LOG_DAEMON);
    log_uname();
}

int main (int argc, char *argv[])
{
    int rc;
    int i=0;
    initLog();

    for (i=0;i<COUNT;i++)
    {
        threadParams[i].threadIdx = i;
        pthread_create(&threads[i],   // pointer to thread descriptor
                  (void *)0,     // use default attributes
                  incThread, // thread function entry point
                  (void *)&(threadParams[i]) // parameters to pass in
                 );
    }
   
   for(i=0; i<COUNT; i++)
   {
        pthread_join(threads[i], NULL);
   }
    

   printf("TEST COMPLETE\n");
}

