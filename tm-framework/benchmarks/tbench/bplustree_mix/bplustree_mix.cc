#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <pthread.h>
#include "bplustree.h"
#include "utility.h"

int n_threads = 1;
int opsize = 5000000;
long range = 1048576 << 1;
int isize = 1048575;

#define MAX_NUMBER 0xffffffff

typedef struct testpara{
    long id;
    bplus_tree *tree;
    long puts;
} data_t;


void* testMix(void* para){
  TM_THREAD_ENTER();
  struct testpara* data = (struct testpara*)para;

  unsigned short seed[3];
  unsigned int d = (unsigned int)data->id;
  seed[0] = (unsigned short)rand_r(&d);
  seed[1] = (unsigned short)rand_r(&d);
  seed[2] = (unsigned short)rand_r(&d);

#ifdef PRIVE_MEMORY_POOL
  initialize_private_node_pool(n_threads, opsize);
#endif
  bplus_tree *ptree = data->tree;

  int total_ops = opsize / n_threads;
  int i;
  for(i = 0; i < total_ops; i++){
    long d = (long)(erand48(seed) * MAX_NUMBER) % range;
    if (i % 10 < 6) { // 60% lookup
      TM_BEGIN();
        bplus_tree_get(ptree, d);
      TM_END();
    } else if (i % 10 < 8){ // 20% range query
      TM_BEGIN();
        bplus_tree_get_range(ptree, d, d + (i % 256));
      TM_END();
    } else if (i % 10 < 9){ // 10% insert 
      TM_BEGIN();
        bplus_tree_put(ptree, d, d);
      TM_END();
    } else { // 10% remove
      TM_BEGIN();
        bplus_tree_delete(ptree, d);
      TM_END();
    }
    data->puts++;
  }
  TM_THREAD_EXIT();
  return NULL;
}



void* testPushOnly(void* para){
  TM_THREAD_ENTER();
  struct testpara* data = (struct testpara*)para;
  
  unsigned short seed[3];
  unsigned int d = (unsigned int)data->id;
  seed[0] = (unsigned short)rand_r(&d);
  seed[1] = (unsigned short)rand_r(&d);
  seed[2] = (unsigned short)rand_r(&d);

#ifdef PRIVE_MEMORY_POOL
  initialize_private_node_pool(n_threads, opsize);
#endif
  
  bplus_tree *ptree = data->tree;
  int total_ops = opsize / n_threads;
  long total_range = range << 1;

  for(int i = 0; i < total_ops; i++){
    long d = (long)(erand48(seed) * MAX_NUMBER) % total_range;
    TM_BEGIN();
      bplus_tree_put(ptree, d, d);
    TM_END();
    data->puts++;
  }
  TM_THREAD_EXIT();
  return NULL;
}


int main (int argc, char **argv)
{
  char ch;
  int i;
  while ((ch = getopt(argc, argv, "t:n:"))!= -1) {
    switch (ch) {
      case 't':
        n_threads = atoi(optarg);
        break;
      case 'n':
        opsize = atoi(optarg);
        break;
      case 'r':
        range = atoi(optarg);
        break;
      case 'i':
        isize = atoi(optarg);
        break;
      default:
        exit(0);
        break;
    }
  }

  pthread_t * pid = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);
  data_t* data = (data_t*)aligned_alloc(32, sizeof(data_t)*n_threads);
  bplus_tree *tree = bplus_tree_init(MAX_LEVEL, MAX_ORDER, MAX_ENTRIES);

  ////--- Initialize the tree with half of its key range
/*  long gap = range / (isize + 1);
  int counter = 0;
#ifdef PRIVE_MEMORY_POOL
   initialize_private_node_pool(1, isize + 1);
#endif
  for (long i = 0; i < range; i += gap) {
    bplus_tree_put(tree, i, i);
    counter++;
  }

  printf("B+Tree range: %ld, intial size: %d\n", range, counter);
  // bplus_tree_dump(tree);
  // --- End the initialization
  */

  TM_STARTUP(n_threads);
  P_MEMORY_STARTUP(n_threads);
  thread_startup(n_threads);
  SIM_GET_NUM_CPU(n_threads);
  GOTO_SIM();

  double start = get_wall_time();

  for(i=0; i<n_threads; i++){
      data[i].tree = tree;
      data[i].id = i+1;
      data[i].puts = 0;
      pthread_create(&pid[i], NULL, testMix, &data[i]);
  }

  for(i=0; i<n_threads; i++){
      pthread_join(pid[i], NULL);
  }
  double end = get_wall_time();
  //printf("stopping..\n");

  GOTO_REAL();

  TM_SHUTDOWN();
  P_MEMORY_SHUTDOWN();

  long puts = 0;
  for(i = 0; i < n_threads; i++){
      puts += data[i].puts;
  }
  double d = end - start;

  printf("%lf\n", puts/d/1000.0);
  //printf("txs: %ld, throughtput: %lf Ktps duration %lf, \n", puts, puts/d/1000.0, d);
}
