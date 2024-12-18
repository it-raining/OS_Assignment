
#include "../include/queue.h"
#include "../include/sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

// #define TEST_SCHED
/* HOW TO TEST
Comment #define TEST_QUEUE and uncomment #define TEST_SCHED before testing
cd to your Project
run gcc -Iinclude -o sched src/queue.c src/sched.c -lpthread
*/

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void)
{
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if (!empty(&mlq_ready_queue[prio]))
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

/*Tạo cấu trúc dữ liệu cho bộ lập lịch*/
void init_scheduler(void)
{
#ifdef MLQ_SCHED
	int i;
	for (i = 0; i < MAX_PRIO; i++)
	{	
		mlq_ready_queue[i].size = 0;			// Mảng hàng đợi (Mức độ ưu tiên khác nhau) .size() = 0: ban đầu không có tiến trình nào trong hàng đợi 
		slot[i] = MAX_PRIO - i;					// Mảng gán giá trị mức độ ưu tiên.
	}
#endif
	ready_queue.size = 0;						// Hàng đợi chứa tiến trình sẵn sàng để thực thi
	run_queue.size = 0;							// hàng đợi chứa tiến trình hiện tại đang chạy
	pthread_mutex_init(&queue_lock, NULL);		// Khóa đồng bọ cho các hàng đợi
}

#ifdef MLQ_SCHED
/*
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t *get_mlq_proc(void)
{
	struct pcb_t *proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
	for (int i = 0; i < MAX_PRIO; ++i)
	{
		if (empty(&mlq_ready_queue[i]) || slot[i] == 0)
		{
			slot[i] = MAX_PRIO - i;
			continue;
		}
		proc = dequeue(&mlq_ready_queue[i]);
		slot[i]--;
		break;
	}
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_mlq_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

struct pcb_t *get_proc(void)
{
	return get_mlq_proc();
}

void put_proc(struct pcb_t *proc)
{
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc)
{
	return add_mlq_proc(proc);
}
#else
struct pcb_t *get_proc(void)
{
	struct pcb_t *proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
	if (empty(&ready_queue))
	{
		while (!empty(run_queue))
		{
			enqueue(&ready_queue, dequeue(&run_queue));
		}
	}
	proc = dequeue(&ready_queue);
	pthread_mutex_unlock(&queue_lock);

	return proc;
}

void put_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}
#endif

#ifdef TEST_SCHED
#include "common.h"
void initProc(struct pcb_t *proc, int prio);
void printProc(struct pcb_t *proc);
int main()
{
	printf("_______________________GET INIT FOR SCHEDULER_______________________\n");
	init_scheduler();
	printf("\n_______________________SET UP QUEUE_______________________\n");
	struct pcb_t *procArray = (struct pcb_t *)malloc(2 * sizeof(struct pcb_t));
	printf("___________CREATE 2 PROC WITH PRIO FROM 137 TO 139_______________________\n");
	initProc(&procArray[0], 137);
	initProc(&procArray[1], 139);
	printf("_______________________ADD 10 PROC WITH PRIO FROM 137 TO 139 TO QUEUE_______________________\n");
	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 10; j++)
			put_proc(&procArray[i]);
	}
	printf("\n_______________________BEGIN TEST FOR GET MQL PROC_______________________\n");
	printf("Print 3 Prio 137, After that print 1 Prio 139 (Slot of Prio 137 is 3)\n");
	for (int i = 0; i < 4; i++)
		printProc(get_proc());
	printf("Until slot of Prio 139 is 0, reset slot for Prio 139, so return NULL for Proc\n");
	for (int i = 0; i < 4; i++)
		printProc(get_proc());
	return 0;

}
void initProc(struct pcb_t *proc, int prio)
{
	proc->active_mswp = NULL;
	proc->bp = 10;
	proc->code = NULL;
	proc->mm = NULL;
	proc->mram = NULL;
	proc->mswp = NULL;
	proc->page_table = NULL;
	proc->pc = 0;
	proc->pid = 0;
	proc->prio = prio;
	proc->priority = 0;
	for (int i = 0; i < 10; i++)
		proc->regs[i] = 0;
}
void printProc(struct pcb_t *proc)
{
	if (proc == NULL)
	{
		printf("NULL\n");
		return;
	}
	printf("Bp: %d Pc: %d Pid: %d Prio: %d Priority: %d\n", proc->bp, proc->pc, proc->pid, proc->prio, proc->priority);
}
#endif