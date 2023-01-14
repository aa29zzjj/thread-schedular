/**
 *
 * File             : scheduler.c
 * Description      : This is a stub to implement all your scheduling schemes
 *
 * Author(s)        : Tzu Chieh Huang , Jill Hughes
 * Last Modified    : @date
*/

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
#include <semaphore.h>
#include <math.h>
#include <pthread.h>

void init_scheduler( int sched_type );
int schedule_me( float currentTime, int tid, int remainingTime, int tprio );
int P( float currentTime, int tid, int sem_id);
int V( float currentTime, int tid, int sem_id);

#define FCFS    0
#define SRTF    1
#define PBS     2
#define MLFQ    3
#define SEM_ID_MAX 50

/** func and variable state**/
/* ********************* define task, queue with the ops *************************** */
typedef struct ThreadTask{
	int id;
	float arrival_time;
	int remaining_time;
	int priority;
}Task;

Task CreateTask(int id, float arrival_time, int remaining_time, int priority){
    Task task;
    task.id = id;
    task.arrival_time = arrival_time;
    task.remaining_time = remaining_time;
    task.priority = priority;
    return task;
}

typedef struct TaskListNode{
    Task task;
    struct TaskListNode* next;
}TaskList, TaskListNode;

typedef struct TaskQueue{
    TaskList* taskList;
    int total;
}TaskReadyQueue, TaskQueue;

TaskListNode* LastP(TaskQueue* queue){
    TaskListNode* p = queue->taskList;
    while(p->next != NULL)
        p = p->next;
    return p;
}

void Push(TaskQueue* queue, Task task){
    if(queue->total == SEM_ID_MAX){
        printf("no space");
        return;
    }
    TaskListNode* p = LastP(queue);
    p->next = malloc(sizeof(TaskListNode)); //add the memory to the link list 
    p->next->task = task;  
    p->next->next = NULL;
    ++queue->total;
}

Task* Front(TaskQueue* queue){
    return &queue->taskList->next->task; //taskList->next is the start of the queue
}

void Pop(TaskQueue* queue){
    TaskListNode * temp = queue->taskList->next;
    queue->taskList->next = temp->next;
    free(temp);
    --queue->total;
}

Task GetAndDel(TaskQueue* queue, int id){
    TaskListNode* p = queue->taskList;
    while(p->next->task.id != id)	//use the thread id to find the specific thread
        p = p->next;
    Task task = p->next->task;
    TaskListNode * temp = p->next;
    p->next = p->next->next;
    free(temp);			// dequeue the specific task
    --queue->total;
    return task;
}



int fcfs(float currentTime, int tid, int remainingTime, int tprio);		    /* FCFS */
int srtf(float currentTime, int tid, int remainingTime, int tprio);		    /* SRTF */
int pbs(float currentTime, int tid, int remainingTime, int tprio);		    /* PBS */
int mlfq(float currentTime, int tid, int remaining_time, int tprio);		/* MLFQ */

pthread_mutex_t readyqueue_lock;
pthread_mutex_t theard_lock;
pthread_mutex_t temp_lock;
pthread_cond_t condition_var1[SEM_ID_MAX];
sem_t sem_var[SEM_ID_MAX];

int return_time;
int scheduler_type;
TaskReadyQueue * queue;
int spend[1024] = {};

/* ***************************************** tools ********************************************* */
/* check if thread is included in ready queue */
int search_id(TaskQueue* queue, int id) {
    for (TaskListNode* p = queue->taskList; p != NULL; p = p->next) {
		if (id == p->task.id) {
			return 1;
		}
	}
	return 0;
}

void SortByRemainingTime(TaskQueue* queue){
	//if there is not only a thread in thread, sort the remaining time 
    if(queue->total != 1){
		//use bubble sort to get the correct arrangement of remaining time. 
		//short time go first, then get greater 
        for(int i = 0; i < queue->total; i++){
            TaskListNode* head = queue->taskList;
            for(int j = 0; j != i; ++j)
                 head = head->next;

            int maxNextValue = head->next->task.remaining_time;
            int minNextID = head->next->task.id;
            TaskListNode* maxNext = head;
			//find the greatest remaining time 
            for(TaskListNode* p = head->next; p->next != NULL; p = p->next){
                if(maxNextValue < p->next->task.remaining_time){
                    maxNextValue = p->next->task.remaining_time;
                    maxNext = p;
                }
            }
			//then swap the queue by bubble sort, remaking the queue list 
            TaskListNode* temp = maxNext->next;
            maxNext->next = temp->next;
            temp->next = queue->taskList->next;
            queue->taskList->next = temp;
        }
    }
}
//same as the SortByRemainingtime, but this time arrange by priority
void SortByPriority(TaskQueue* queue){
    if(queue->total != 1){
        for(int i = 0; i < queue->total; i++){
            TaskListNode* head = queue->taskList;
            for(int j = 0; j != i; ++j)
                 head = head->next;

            int maxNextValue = head->next->task.priority;
            int minNextId = head->next->task.id;
            TaskListNode* maxNext = head;
            for(TaskListNode* p = head->next; p->next != NULL; p = p->next){
                if(maxNextValue < p->next->task.priority){
                    maxNextValue = p->next->task.priority;
                    maxNext = p;
                    minNextId = p->next->task.id;
                }
                if(maxNextValue == p->next->task.priority && minNextId < p->next->task.id){
                    maxNextValue = p->next->task.priority;
                    maxNext = p;
                    minNextId = p->next->task.id;
                }
            }      
            TaskListNode* temp = maxNext->next;
            maxNext->next = temp->next;
            temp->next = queue->taskList->next;
            queue->taskList->next = temp;
        }
    }
}
/* ***************************************** tools ********************************************* */

void init_scheduler( int sched_type ) {

    /** Fill your code here **/
    return_time = 0;
	scheduler_type = sched_type;

	/* initialize the mutex */
	pthread_mutex_init(&readyqueue_lock, NULL);
	pthread_mutex_init(&theard_lock, NULL);
	pthread_mutex_init(&temp_lock, NULL);
	for(int i = 0; i < SEM_ID_MAX; ++i){
		pthread_cond_init(&condition_var1[i], NULL);
		sem_init(&sem_var[i], 0, 0);
	}

    /* initialize the mutex */
    queue = malloc(sizeof(TaskQueue));
    queue->taskList = malloc(sizeof(TaskListNode));
    queue->taskList->next = NULL;
    queue->total = 0;
}

int schedule_me( float currentTime, int tid, int remainingTime, int tprio ) {
    /** Fill your code here **/
    return_time = currentTime;

	switch(scheduler_type) {
		case FCFS:
			return_time = fcfs(currentTime, tid, remainingTime, tprio);
			break;
		case SRTF:
			return_time = srtf(currentTime, tid, remainingTime, tprio);
			break;
		case PBS:
			return_time = pbs(currentTime, tid, remainingTime, tprio);
			break;
		case MLFQ:
			return_time = mlfq(currentTime, tid, remainingTime, tprio);
			break;
		default:
			break;
	}
	return return_time;
}

int P( float currentTime, int tid, int sem_id) { // returns current global time
/**
	Fill your code here
*/
//semaphore value = 0
    int sval = 0;
//get the semaphore value
    sem_getvalue(&sem_var[sem_id], &sval);
//if the semaphore value is enough, use sem_wait to make it into 1, else, block the thread
    if(sval > 0)
        sem_wait(&sem_var[sem_id]);
    else{
        Task task;
        if(Front(queue)->id == tid){
            task = GetAndDel(queue, tid);
            
            if(scheduler_type == SRTF ){
                SortByRemainingTime(queue);
            }else if(scheduler_type == PBS ){
                SortByPriority(queue);
            }
			//wake, reach the next thread 
            pthread_cond_signal(&condition_var1[Front(queue)->id]);
        }
        else{
			//since the head is not the specific thread, keep waiting.
            task = GetAndDel(queue, tid);
            if(scheduler_type == SRTF ){
                SortByRemainingTime(queue);
            }else if(scheduler_type == PBS){
                SortByPriority(queue);
            }
        }
		//wait the semaphore val becomes to 1
        sem_wait(&sem_var[sem_id]);
        Push(queue, task);
		//resort again since the thread is pushed into queue
        if(scheduler_type == SRTF ){
            SortByRemainingTime(queue);
            pthread_cond_signal(&condition_var1[Front(queue)->id]);
        }else if(scheduler_type == PBS ){
            SortByPriority(queue);
            pthread_cond_signal(&condition_var1[Front(queue)->id]);
        }
    }
	//send the sem_value to the sval, which means the proccessing
    sem_getvalue(&sem_var[sem_id],&sval);


    return return_time;
}

int V( float currentTime, int tid, int sem_id){ // returns current global time
/**
	Fill your code here
*/
    int sval = 0;
    sem_getvalue(&sem_var[sem_id],&sval);
    sem_post(&sem_var[sem_id]);
    return return_time;
}


/* ***************************************** 4 scheduling algorithm ********************************************* */
/* First Come First Serve */
int fcfs(float currentTime, int tid, int remainingTime, int tprio) {

	pthread_mutex_lock(&readyqueue_lock);
	/* case 1: if the input thread is not in the ready_queue, add it to the ready_queue */
	if (!search_id(queue, tid)) {
        Task task = CreateTask(tid, currentTime, remainingTime, tprio);
		pthread_cond_init(&condition_var1[tid], NULL);
		Push(queue, task);
	}
	/* case 2: if the thread is not at the head of ready_queue, block it, wait until it go to the head */
	
	if ( search_id(queue, tid) && (Front(queue)->id != tid)) {
		pthread_cond_wait(&condition_var1[tid], &readyqueue_lock);
	}

	/* update the time */
	Front(queue)->remaining_time = remainingTime;

	if (remainingTime <= 0) {
		Pop(queue);
		if (queue->total > 0) {
			pthread_cond_signal(&condition_var1[Front(queue)->id]);
		}
	}

	/* then, find the correct return time */
	if (ceil(currentTime) > (int)return_time)
		return_time = ceil(currentTime);

	pthread_mutex_unlock(&readyqueue_lock);
	return return_time;
}


/* Shortest Remaining Time First */
int srtf(float currentTime, int tid, int remainingTime, int tprio)
{
	pthread_mutex_lock(&readyqueue_lock);
	
	/* case 1: if the input thread is not in the ready_queue, add it to the ready_queue */
	if (!search_id(queue, tid)) {
		Task task = CreateTask(tid, currentTime, remainingTime, tprio);
		pthread_cond_init(&condition_var1[tid], NULL);
		Push(queue, task);
	}
    SortByRemainingTime(queue);
    pthread_cond_signal(&condition_var1[Front(queue)->id]);
	/* case 2: if the thread is not at the head of ready_queue, block it, wait until it go to the head */
	if ( search_id(queue, tid) && (Front(queue)->id != tid)) {
		pthread_cond_wait(&condition_var1[tid], &readyqueue_lock);
	}
	pthread_mutex_unlock(&readyqueue_lock);

	/* update the time */
	Front(queue)->remaining_time = remainingTime;

	
	if (remainingTime <= 0) {
	    pthread_mutex_lock(&readyqueue_lock);
		Pop(queue);
		if (queue->total > 0) {
		    SortByRemainingTime(queue);
			pthread_cond_signal(&condition_var1[Front(queue)->id]);
		}
		pthread_mutex_unlock(&readyqueue_lock);
	}

	/* then, find the correct return time */
	if (ceil(currentTime) > (int)return_time)
		return_time = ceil(currentTime);

	return return_time;
}


/* Priority Based Scheduling */
int pbs(float currentTime, int tid, int remainingTime, int tprio)
{
	pthread_mutex_lock(&readyqueue_lock);
	SortByPriority(queue);
	/* case 1: if the input thread is not in the ready_queue, add it to the ready_queue */
	if (!search_id(queue, tid)) {
		Task task = CreateTask(tid, currentTime, remainingTime, tprio);
		pthread_cond_init(&condition_var1[tid], NULL);
		Push(queue, task);
		SortByPriority(queue);
	}

	/* case 2: if the thread is not at the head of ready_queue, block it, wait until it go to the head */
	if ( search_id(queue, tid) && (Front(queue)->id != tid)) {
		pthread_cond_wait(&condition_var1[tid], &readyqueue_lock);
	}
	
	/* update the time */
	Front(queue)->remaining_time = remainingTime;

	if (remainingTime <= 0) {
		Pop(queue);
		if (queue->total > 0) {
		    SortByPriority(queue);
			pthread_cond_signal(&condition_var1[Front(queue)->id]);
		}
	}

	/* then, find the correct return time */
	if (ceil(currentTime) > (int)return_time)
		return_time = ceil(currentTime);
		
    pthread_mutex_unlock(&readyqueue_lock);
	return return_time;
}

/* MLFQ*/
int mlfq(float currentTime, int tid, int remainingTime, int tprio)
{
	//spend[tid] = the time using in for this thread
	pthread_mutex_lock(&readyqueue_lock);
	SortByPriority(queue);
	/* case 1: if the input thread is not in the ready_queue, add it to the ready_queue */
	if (!search_id(queue, tid)) {
	    spend[tid] = 0;
		Task task = CreateTask(tid, currentTime, remainingTime, 1);
		pthread_cond_init(&condition_var1[tid], NULL);
		Push(queue, task);
	}
	else{

	    Task old = GetAndDel(queue, tid);
	    Task task;
		//divide into 5 priority, if the spending time is greater, the priority is lower 
	    spend[tid]++;
	    if(spend[tid] <= 8)
	        task = CreateTask(tid, currentTime, old.remaining_time, 1);
	    else if(spend[tid] <= 16)
	        task = CreateTask(tid, currentTime, old.remaining_time, 2);
	    else if(spend[tid] <= 31)
	        task = CreateTask(tid, currentTime, old.remaining_time, 3);
	    else if(spend[tid] <= 51)
	        task = CreateTask(tid, currentTime, old.remaining_time, 4);
	    else
	        task = CreateTask(tid, currentTime, old.remaining_time, 5);
	    Push(queue, task);
	}
	
	
    SortByPriority(queue);
    // printf("first is %d\n", Front(queue)->id);
    pthread_cond_signal(&condition_var1[Front(queue)->id]);
	/* case 2: if the thread is not at the head of ready_queue, block it, wait until it go to the head */
	if ( search_id(queue, tid) && (Front(queue)->id != tid)) {
		pthread_cond_wait(&condition_var1[tid], &readyqueue_lock);
	}
	
	/* update the time */
	Front(queue)->remaining_time = remainingTime;

	if (remainingTime <= 0) {
		Pop(queue);
		if (queue->total > 0) {
		    SortByPriority(queue);
			pthread_cond_signal(&condition_var1[Front(queue)->id]);
		}
	}

	/* then, find the correct return time */
	if (ceil(currentTime) > (int)return_time)
		return_time = ceil(currentTime);
		
    pthread_mutex_unlock(&readyqueue_lock);
    ++spend[tid];
	return return_time;
}
