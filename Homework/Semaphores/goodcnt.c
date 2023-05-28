#include "csapp.h"

volatile int cnt = 0;
sem_t mutex;

void *thread(void *vargp);

int main(int argc, char **argv)
{
  Sem_init(&mutex, 0, 1);
  
  int niters = atoi(argv[1]);
  pthread_t tid1, tid2;

  struct timeval start_time, end_time;
  long elapsed_time;

  gettimeofday(&start_time, NULL);

  pthread_create(&tid1, NULL, thread, &niters);
  pthread_create(&tid2, NULL, thread, &niters);
  pthread_join(tid1, NULL);
  pthread_join(tid2, NULL);

  gettimeofday(&end_time, NULL);

  elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000L + (end_time.tv_usec - start_time.tv_usec);

  /* Check result */
  if (cnt != (2 * niters))
    printf("BOOM! cnt=%d\n", cnt);
  else
    printf("OK cnt=%d\n", cnt);

  printf("Elapsed time: %ld microseconds\n", elapsed_time);

  exit(0);
}

void *thread(void *vargp)
{
  int i, niters = *((int *)vargp);

  for (i = 0; i < niters; i++) {
    P(&mutex);
    cnt++;
    V(&mutex);
  }
  
  return NULL;
}