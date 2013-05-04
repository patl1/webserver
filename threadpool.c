#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "list.h"
#include "threadpool.h"
#define N 10

typedef struct thread_pool
{
	pthread_t *threads;	    //will be used as an array of all pthreads in this pool
	struct list workers;	//FIFO queue for futures ready to start
	pthread_mutex_t lock;	//mutex for queues to wait for signals
	pthread_cond_t condition_var; //condition variable because it said so in the specs
	int shutdown;           //nonzero if shutdown is required
	int threadCount;		//number of threads this pool was told to create and manage
} * thread_pool;

typedef struct future
{
	sem_t semaTizeMe;       //used to indicate if a future's result is available
	struct list_elem elem;  //to put in a list of futures
	thread_pool_callable_func_t function;   //a function to be called
	void * data;            //the arguments passed to the function
	int getCalled;          //flag to see if future_get() was called before future_free()
	void * result;          //whatever the return value/data from the function was
} * future;

/*
 * Just a routine to intialize a thread.  It doesn't need to do anything, just
 * be there, so it should probably give up control right away.
 */
static
void * thread_init_routine(void* the_threadpool)
{
    assert(the_threadpool != NULL);
	struct thread_pool * pool = (struct thread_pool *) the_threadpool;
	while(1)   //why does the list need to be empty?
	{
		pthread_mutex_lock(&pool->lock);
        while (list_empty(&pool->workers)){
		    if (pool->shutdown)
	    	{
			    pthread_mutex_unlock(&pool->lock);
			    pthread_exit(NULL);
		    }
            pthread_cond_wait(&pool->condition_var, &pool->lock);
            if (pool->shutdown)
	    	{
			    pthread_mutex_unlock(&pool->lock);
			    pthread_exit(NULL);
		    }
        }
        if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->lock);
			pthread_exit(NULL);
		}
		else
		{
			struct list_elem * e= list_pop_front(&pool->workers);
			future execute = list_entry(e, struct future, elem);
			pthread_mutex_unlock(&pool->lock);
			void * result = (execute->function)(execute->data);
			execute->result = result;
			sem_post(&execute->semaTizeMe);
		}
	}
	return NULL;
}


struct thread_pool * thread_pool_new(int nthreads)
{
	struct thread_pool * pool = (struct thread_pool *) malloc(sizeof (struct thread_pool));
    if(pool == NULL){
        printf("Error allocating memory for thread pool.\n");
        exit(1);
    }
	//intialize the thread pool and all its fields
	list_init(&pool->workers);
	pthread_mutex_init(&pool->lock, NULL);
	pool->shutdown = 0;
	pool->threadCount = nthreads;
    pool->threads = (pthread_t *) malloc(nthreads * sizeof(pthread_t)); //have to allocate space for all the threads
    if(pool->threads == NULL){
        printf("Error allocating memory for thread pool.\n");
        exit(1);
    }
	pthread_cond_init(&pool->condition_var, NULL);
	//create all its threads
	int i;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nthreads; i++)
	{
		pthread_create(&pool->threads[i], &attr, thread_init_routine, (void *) pool);
	}
	return pool;
}

struct future * thread_pool_submit(struct thread_pool * pool, thread_pool_callable_func_t funct, void * callable_data)
{
	//intialize the future
	struct future * f = (struct future *) malloc(sizeof(struct future));
	sem_init(&f->semaTizeMe, 0, 0);
	f->function = funct;
	f->data = callable_data;
	f->getCalled = 0;
	pthread_mutex_lock(&pool->lock);
	//push future onto workers
	list_push_back(&pool->workers, &f->elem);
	//signal a future was added and let the threads take care of everything else
	pthread_cond_signal(&pool->condition_var);
	pthread_mutex_unlock(&pool->lock);
   	//pretty much done now
	return f;
}
void * future_get(struct future * f)
{
    assert(f != NULL);
	sem_wait(&f->semaTizeMe);
	f->getCalled = 1;
	return f->result;
}

void thread_pool_shutdown(struct thread_pool * pool)
{
    assert(pool != NULL);
    void *status;
	//notify all threads to wake the hell up
    pthread_mutex_lock(&pool->lock);
	pool->shutdown = 1;
    pthread_cond_broadcast(&pool->condition_var);
    pthread_mutex_unlock(&pool->lock);
	//remove any waiting threads and futures
	int i;
	for (i = 0; i < pool->threadCount; i++)
	{
		pthread_join(pool->threads[i], &status);
	}
	pthread_mutex_destroy(&pool->lock);
	pthread_cond_destroy(&pool->condition_var);
	//free
    free(pool->threads);
	free(pool);
}

void future_free(struct future * f)
{
	if(f != NULL)
	{
	    assert (f->getCalled == 1);
		free(f);
	}   
}
