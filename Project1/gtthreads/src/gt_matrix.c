#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include "gt_include.h"


#define ROWS 256
#define COLS ROWS
#define SIZE COLS

#define NUM_CPUS 4
#define NUM_GROUPS NUM_CPUS
#define PER_GROUP_COLS (SIZE/NUM_GROUPS)

#define PER_THREAD_ROWS (SIZE/NUM_THREADS)


/* A[SIZE][SIZE] X B[SIZE][SIZE] = C[SIZE][SIZE]
 * Let T(g, t) be thread 't' in group 'g'. 
 * T(g, t) is responsible for multiplication : 
 * A(rows)[(t-1)*SIZE -> (t*SIZE - 1)] X B(cols)[(g-1)*SIZE -> (g*SIZE - 1)] */

typedef struct matrix
{
	int m[SIZE][SIZE];

	int rows;
	int cols;
	unsigned int reserved[2];
} matrix_t;


typedef struct __uthread_arg
{
	matrix_t *_A, *_B, *_C;
	unsigned int reserved0;
	int matrix_size;
	unsigned int tid;
	unsigned int gid;
	int start_row; /* start_row -> (start_row + PER_THREAD_ROWS) */
	int start_col; /* start_col -> (start_col + PER_GROUP_COLS) */
	
}uthread_arg_t;
	
struct timeval tv1;

static void generate_matrix(matrix_t *mat, int val)
{

	int i,j;
	mat->rows = SIZE;
	mat->cols = SIZE;
	for(i = 0; i < mat->rows;i++)
		for( j = 0; j < mat->cols; j++ )
		{
			mat->m[i][j] = val;
		}
	return;
}

static void print_matrix(matrix_t *mat)
{
	int i, j;

	for(i=0;i<SIZE;i++)
	{
		for(j=0;j<SIZE;j++)
			printf(" %d ",mat->m[i][j]);
		printf("\n");
	}

	return;
}

static void * uthread_mulmat(void *p)
{
	int i, j, k;
	int start_row, end_row;
	int start_col, end_col;
	unsigned int cpuid;
	struct timeval tv2;

#define ptr ((uthread_arg_t *)p)

	i=0; j= 0; k=0;

	start_row = 0;
	end_row = ptr->matrix_size;

	end_col = ptr->matrix_size;
	start_col = 0;

#ifdef GT_THREADS
	cpuid = kthread_cpu_map[kthread_apic_id()]->cpuid;
//	fprintf(stderr, "\nThread(id:%d, group:%d, cpu:%d) started",ptr->tid, ptr->gid, cpuid);
#else
//	fprintf(stderr, "\nThread(id:%d, group:%d) started",ptr->tid, ptr->gid);
#endif
	
	if(ptr->matrix_size == 256){
		yield_me();
	}
	for(i = start_row; i < end_row; i++){
		
		for(j = start_col; j < end_col; j++){
			for(k = 0; k < ptr->matrix_size; k++){
				ptr->_C->m[i][j] += ptr->_A->m[i][k] * ptr->_B->m[k][j];
				
					
			}
		}
	}
#ifdef GT_THREADS
//	fprintf(stderr, "\nThread(id:%d, group:%d, cpu:%d) finished (TIME : %lu s and %lu us)",
//			ptr->tid, ptr->gid, cpuid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
#else
	gettimeofday(&tv2,NULL);
//	fprintf(stderr, "\nThread(id:%d, group:%d) finished (TIME : %lu s and %lu us)",
//			ptr->tid, ptr->gid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
#endif

#undef ptr
	return 0;
}

matrix_t A, B, C;

static void init_matrices()
{
	generate_matrix(&A, 1);
	generate_matrix(&B, 1);
	generate_matrix(&C, 0);

	return;
}

void array_init(float *arr){
	for(int i = 0; i < 16; i++){
		arr[i] = 0;
	}
}

uthread_arg_t uargs[NUM_THREADS];
uthread_t utids[NUM_THREADS];

int main(int argc, char *argv[])
{
	uthread_arg_t *uarg;
	int inx;
	
	int scheduler_type = atoi(argv[1]);
	
	gtthread_app_init(scheduler_type); // passing this information to k_shared_info. In many occasions we need to know the schedule type 

	init_matrices();

	gettimeofday(&tv1,NULL);
	
	int thread_id = 0;// range from 0 to 127
 	int group_id  = 0;// range from 0 to 15

	//printf("Scheuler type %d \n" , atoi(argv[1]));	

	//If 1 --> credit scheduler
	if(scheduler_type== 1){
			
		for(int weight = 25; weight <= 100; weight += 25){ // 25 50 75 100
			for(int size = 32; size <= 256; size *= 2){ //32 64 128 256		
				for(int i=0; i < 8; i++){ // 4*4*8 ---> all combos of weight and matrix size --> 128 thread ids
					uarg = &uargs[thread_id];
					uarg->_A = &A;
					uarg->_B = &B;
					uarg->_C = &C;
					uarg->matrix_size = size; //Since each thread will have a matrix of its own, might as well save it as an intrinsic attr
					uarg->tid = thread_id;

					uarg->gid = (thread_id % NUM_GROUPS);//group_id;		
						
					uthread_create(&utids[thread_id], uthread_mulmat, uarg, uarg->gid , weight);
					thread_id++;
				}

				group_id++;
			}
		}
		
		
	}else{	
		

		for(inx=0; inx<NUM_THREADS; inx++)
		{
			uarg = &uargs[inx];
			uarg->_A = &A;
			uarg->_B = &B;
			uarg->_C = &C;

			uarg->tid = inx;

			uarg->gid = (inx % NUM_GROUPS);

			uarg->start_row = (inx * PER_THREAD_ROWS);
#ifdef GT_GROUP_SPLIT
			/* Wanted to split the columns by groups !!! */
			uarg->start_col = (uarg->gid * PER_GROUP_COLS);
#endif

			uthread_create(&utids[inx], uthread_mulmat, uarg, uarg->gid , 0);
		}
	}
	gtthread_app_exit();

	/******* HARDCOREEEE STATISTICSSSSSS **************/	
	float total_time_stats[16];
	float run_time_stats[16];
	float total_time_stdv[16];
	float run_time_stdv[16];	
	
	
	array_init(total_time_stats);
	array_init(run_time_stats);
	array_init(total_time_stdv);
	array_init(run_time_stdv);
	
	int size = 32;
	int group_number = 0;
	for(int i = 0; i < 128 ; i+=8){
		for(int j = 0 ; j < 8; j++){
			total_time_stats[group_number] = total_time_stats[group_number] + (uthread_arr[i]->total_time.tv_sec*1000000  + uthread_arr[i]->total_time.tv_usec);	
			run_time_stats[group_number] = run_time_stats[group_number] + (uthread_arr[i]->run_time.tv_sec*1000000  + uthread_arr[i]->run_time.tv_usec);	
		}
		total_time_stats[group_number] = total_time_stats[group_number] / 8;
		run_time_stats[group_number]= run_time_stats[group_number] / 8;
		
		for(int i=0; i < 8; i++){
			total_time_stdv[group_number] = pow((uthread_arr[i]->total_time.tv_sec*1000000  + uthread_arr[i]->total_time.tv_usec) - total_time_stats[group_number] , 2);	
			run_time_stdv[group_number] = pow((uthread_arr[i]->run_time.tv_sec*1000000  + uthread_arr[i]->run_time.tv_usec) - run_time_stats[group_number] , 2);
		}		
		printf("%3d     %3d    %15.0f %15.5f %15.0f %15.5f \n" , uthread_arr[i]->weight, size, total_time_stats[group_number] , sqrt(total_time_stdv[group_number]) , run_time_stats[group_number] , sqrt(run_time_stdv[group_number]) );
		
		group_number++;
		if(size <256){
			size*=2;
		}else{
			size = 32;
		}		
		//printf("thread_id = %d  weight = %d  run time = %lu  total time = %lu\n" , uthread_arr[i]->uthread_tid , uthread_arr[i]->weight, (uthread_arr[i]->run_time.tv_sec*1000000  + uthread_arr[i]->run_time.tv_usec), (uthread_arr[i]->total_time.tv_sec*1000000  + uthread_arr[i]->total_time.tv_usec));
	}
	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
