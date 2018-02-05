#ifndef __GT_UTHREAD_H
#define __GT_UTHREAD_H

/* User-level thread implementation (using alternate signal stacks) */

typedef unsigned int uthread_t;
typedef unsigned int uthread_group_t;

/* uthread states */
#define UTHREAD_INIT 0x01
#define UTHREAD_RUNNABLE 0x02
#define UTHREAD_RUNNING 0x04
#define UTHREAD_CANCELLED 0x08
#define UTHREAD_DONE 0x10

#define NUM_THREADS 128
#define NUM_UTHREADS 128


/* uthread struct : has all the uthread context info */
typedef struct uthread_struct
{
	
	/*Introduced vars*/
	int yield_surplus;//number of times this thread needs to yield before it can actually run
	int weight; // 25 = 30ms 50 = 60ms 75 = 90ms 100 = 120ms
	int credit; // amount of remaining credit is calculated: credit  = credit - 25*(per_round_time/10000)
	struct timeval created_at; // Time in which thread is created
	struct timeval finalized_at; // Time in which the thread is finalized  
	struct timeval total_time; //finalized_at - created_at
		
	struct timeval scheduled_at; // Time in which thread got scheduled
	struct timeval run_time; // total_run_time += preempted_at - scheduled_at;


	/*End of int vars*/	
	
	int uthread_state; /* UTHREAD_INIT, UTHREAD_RUNNABLE, UTHREAD_RUNNING, UTHREAD_CANCELLED, UTHREAD_DONE */
	int uthread_priority; /* uthread running priority */
	int cpu_id; /* cpu it is currently executing on */
	int last_cpu_id; /* last cpu it was executing on */
	
	uthread_t uthread_tid; /* thread id */
	uthread_group_t uthread_gid; /* thread group id  */
	int (*uthread_func)(void*);
	void *uthread_arg;

	void *exit_status; /* exit status */
	int reserved1;
	int reserved2;
	int reserved3;
	
	sigjmp_buf uthread_env; /* 156 bytes : save user-level thread context*/
	stack_t uthread_stack; /* 12 bytes : user-level thread stack */
	TAILQ_ENTRY(uthread_struct) uthread_runq;
} uthread_struct_t;

//To save all uthreads to be used later for calculations
extern uthread_struct_t *uthread_arr[];

extern void yield_me();//When called, current uthread yields kthread


struct __kthread_runqueue;
extern void uthread_schedule(uthread_struct_t * (*kthread_best_sched_uthread)(struct __kthread_runqueue *));
#endif
