#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <signal.h>

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_MSEC (1000000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (4)
#define TRUE (1)
#define FALSE (0)

#define NUM_THREADS (3)

#define MY_CLOCK_TYPE CLOCK_MONOTONIC_RAW


int abortTest=FALSE;
int abortS1=FALSE, abortS2=FALSE, abortS3=FALSE;
sem_t semS1, semS2, semS3;
struct timespec start_time_val;
double start_realtime;
unsigned long long sequencePeriods;

static timer_t timer_1;
static struct itimerspec itime = {{1,0}, {1,0}};
static struct timespec sleep_time = {0, 0};
static struct timespec sleep_requested = {0, 0};
static struct timespec remaining_time = {0, 0};
static struct itimerspec last_itime;
static unsigned int sleep_count = 0;
static unsigned long long seqCnt=0;

typedef struct
{
    int threadIdx;
} threadParams_t;


void Sequencer(int id);

void *Service_1(void *threadp);
void *Service_2(void *threadp);
void *Service_3(void *threadp);

double realtime(struct timespec *tsptr);
void print_scheduler(void);

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


int main(void)
{
    struct timespec current_time_val, current_time_res;
    double current_realtime, current_realtime_res;

    int i, rc, flags=0;

    cpu_set_t threadcpu;
    cpu_set_t allcpuset;

    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio, cpuidx;

    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;


    pid_t mainpid;
    init_log();

    printf("Starting High Rate Sequencer Demo\n");
    clock_gettime(MY_CLOCK_TYPE, &start_time_val); start_realtime=realtime(&start_time_val);
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    clock_getres(MY_CLOCK_TYPE, &current_time_res); current_realtime_res=realtime(&current_time_res);
    printf("START High Rate Sequencer @ sec=%6.9lf with resolution %6.9lf\n", (current_realtime - start_realtime), current_realtime_res);
    syslog(LOG_CRIT, "START High Rate Sequencer @ sec=%6.9lf with resolution %6.9lf\n", (current_realtime - start_realtime), current_realtime_res);

   printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

   CPU_ZERO(&allcpuset);

   for(i=0; i < NUM_CPU_CORES; i++)
       CPU_SET(i, &allcpuset);

   printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));


    // initialize the sequencer semaphores
    //
    if (sem_init (&semS1, 0, 0)) { printf ("Failed to initialize S1 semaphore\n"); exit (-1); }
    if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize S2 semaphore\n"); exit (-1); }
    if (sem_init (&semS3, 0, 0)) { printf ("Failed to initialize S3 semaphore\n"); exit (-1); }
    
    mainpid=getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if(rc < 0) perror("main_param");
    print_scheduler();


    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);


    for(i=0; i < NUM_THREADS; i++)
    {
      {
          CPU_ZERO(&threadcpu);
          cpuidx=(3);
          CPU_SET(cpuidx, &threadcpu);
      }

      rc=pthread_attr_init(&rt_sched_attr[i]);
      rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
      rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
      rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

      rt_param[i].sched_priority=rt_max_prio-i;
      pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

      threadParams[i].threadIdx=i;
    }
   
    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Servcie_1 
    //
    printf("PRIORIDADE SERVICE 1 %d \n",rt_max_prio-1);
    rt_param[0].sched_priority=rt_max_prio-1;
    pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);
    rc=pthread_create(&threads[0],               // pointer to thread descriptor
                      &rt_sched_attr[0],         // use specific attributes
                      //(void *)0,               // default attributes
                      Service_1,                 // thread function entry point
                      (void *)&(threadParams[0]) // parameters to pass in
                     );
    if(rc < 0)
        perror("pthread_create for service 1");
    else
        printf("pthread_create successful for service 1\n");


    // Service_2 
    //
    printf("PRIORIDADE SERVICE 2 %d\n",rt_max_prio-2);
    rt_param[1].sched_priority=rt_max_prio-2;
    pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]);
    rc=pthread_create(&threads[1], &rt_sched_attr[1], Service_2, (void *)&(threadParams[1]));
    if(rc < 0)
        perror("pthread_create for service 2");
    else
        printf("pthread_create successful for service 2\n");


    // Service_3
    //
    printf("PRIORIDADE SERVICE 3 %d\n",rt_max_prio-3);
    rt_param[2].sched_priority=rt_max_prio-3;
    pthread_attr_setschedparam(&rt_sched_attr[2], &rt_param[2]);
    rc=pthread_create(&threads[2], &rt_sched_attr[2], Service_3, (void *)&(threadParams[2]));
    if(rc < 0)
        perror("pthread_create for service 3");
    else
        printf("pthread_create successful for service 3\n");


 
    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    sequencePeriods=2000;

    //
    /* set up to signal SIGALRM if timer expires */
    timer_create(CLOCK_REALTIME, NULL, &timer_1);

    signal(SIGALRM, (void(*)()) Sequencer);


    /* arm the interval timer */
    itime.it_interval.tv_sec = 0;
    itime.it_interval.tv_nsec = 10000000;
    itime.it_value.tv_sec = 0;
    itime.it_value.tv_nsec = 10000000;
    //itime.it_interval.tv_sec = 1;
    //itime.it_interval.tv_nsec = 0;
    //itime.it_value.tv_sec = 1;
    //itime.it_value.tv_nsec = 0;

    timer_settime(timer_1, flags, &itime, &last_itime);


    for(i=0;i<NUM_THREADS;i++)
    {
        if((rc=pthread_join(threads[i], NULL)) < 0)
		perror("main pthread_join");
	else
		printf("joined thread %d\n", i);
    }

   printf("\nTEST COMPLETE\n");

   return 0;
}


void Sequencer(int id)
{
    int  flags=0;
       
    seqCnt++;

    // Release each service at a sub-rate of the generic sequencer rate

    // Servcie_1 = 20 msec
    if((seqCnt % 2) == 0) sem_post(&semS1);

    // Service_2 = 100 msec
    if((seqCnt % 10) == 0) sem_post(&semS2);

    // Service_3 = 150 msec
    if((seqCnt % 15) == 0) sem_post(&semS3);
    
    if(abortTest || (seqCnt >= sequencePeriods))
    {
        // disable interval timer
        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_nsec = 0;
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_nsec = 0;
        timer_settime(timer_1, flags, &itime, &last_itime);
	    printf("Disabling sequencer interval timer with abort=%d and %llu of %lld\n", abortTest, seqCnt, sequencePeriods);

	// shutdown all services
        sem_post(&semS1); sem_post(&semS2); sem_post(&semS3);
        
        abortS1=TRUE; abortS2=TRUE; abortS3=TRUE;
        
    }

}



void *Service_1(void *threadp)
{
    int rc;
    unsigned int max_sleep_calls=3;
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S1Cnt=0;
    
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S1 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S1 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS1) // check for synchronous abort request
    {
	// wait for service request from the sequencer, a signal handler or ISR in kernel
        sem_wait(&semS1);

        S1Cnt++;
        
	// DO WORK
    sleep_time.tv_sec=0;
    sleep_time.tv_nsec=10000000;
    sleep_requested.tv_sec=sleep_time.tv_sec;
    sleep_requested.tv_nsec=sleep_time.tv_nsec;
    do 
      {
          if((rc=nanosleep(&sleep_time, &remaining_time)) == 0) break;
         
          sleep_time.tv_sec = remaining_time.tv_sec;
          sleep_time.tv_nsec = remaining_time.tv_nsec;
          sleep_count++;
      } 
      while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0))
		      && (sleep_count < max_sleep_calls));

	    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S1 on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S1Cnt, current_realtime-start_realtime);
    }

    // Resource shutdown here
    //
    pthread_exit((void *)0);
}


void *Service_2(void *threadp)
{
    struct timespec current_time_val;
     int rc;
    unsigned int max_sleep_calls=3;
    double current_realtime;
    unsigned long long S2Cnt=0;
    
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S2 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S2 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS2)
    {
        sem_wait(&semS2);
        S2Cnt++;
      
        sleep_time.tv_sec=0;
        sleep_time.tv_nsec=10000000;
        sleep_requested.tv_sec=sleep_time.tv_sec;
        sleep_requested.tv_nsec=sleep_time.tv_nsec;
    do 
      {
          if((rc=nanosleep(&sleep_time, &remaining_time)) == 0) break;
         
          sleep_time.tv_sec = remaining_time.tv_sec;
          sleep_time.tv_nsec = remaining_time.tv_nsec;
          sleep_count++;
      } 
      while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0))
		      && (sleep_count < max_sleep_calls));

        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S2 on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S2Cnt, current_realtime-start_realtime);
    }

    pthread_exit((void *)0);
}


void *Service_3(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    int rc;
    unsigned int max_sleep_calls=3;
    unsigned long long S3Cnt=0;
    
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S3 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S3 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS3)
    {
        sem_wait(&semS3);
        S3Cnt++;
        sleep_time.tv_sec=0;
        sleep_time.tv_nsec=20000000;
        sleep_requested.tv_sec=sleep_time.tv_sec;
        sleep_requested.tv_nsec=sleep_time.tv_nsec;
        do 
        {
            if((rc=nanosleep(&sleep_time, &remaining_time)) == 0) break;
            
            sleep_time.tv_sec = remaining_time.tv_sec;
            sleep_time.tv_nsec = remaining_time.tv_nsec;
            sleep_count++;
        } 
        while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0))
                && (sleep_count < max_sleep_calls));
        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S3 on core %d forrelease %llu @ sec=%6.9lf\n", sched_getcpu(), S3Cnt, current_realtime-start_realtime);
    }

    pthread_exit((void *)0);
}


double realtime(struct timespec *tsptr)
{
    return ((double)(tsptr->tv_sec) + (((double)tsptr->tv_nsec)/1000000000.0));
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
       case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
       case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
         break;
       case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
       //case SCHED_DEADLINE:
       //    printf("Pthread Policy is SCHED_DEADLINE\n"); exit(-1);
       //    break;
       default:
           printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }
}

