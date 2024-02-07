#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>

typedef struct
{
    int threadIdx;
} threadParams_t;


pthread_t thread;
threadParams_t threadParams;

void *printThread(void *threadp)
{
    syslog(LOG_INFO, "Hello World from Thread!\n");
    return EXIT_SUCCESS;
}

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

int main (int argc, char *argv[]){

    int result = system("echo > /dev/null | sudo tee /var/log/syslog"); // clear syslog
     if (result < 0){
      printf("Error while executing system command");
    }
    openlog("pthread: [COURSE:1][ASSIGNMENT:1] Weslley Araujo", LOG_NDELAY, LOG_DAEMON);

     log_uname();

    threadParams.threadIdx = 1;

    pthread_create(&thread,
                    (void *)0,
                    printThread,
                    (void *)&(threadParams)
                    );

    syslog(LOG_INFO, "Hello World from Main!");

    pthread_join(thread, NULL);

    printf("TEST COMPLETE");
    closelog();

  return EXIT_SUCCESS;
}