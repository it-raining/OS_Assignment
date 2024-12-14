#include <stdio.h>
#include <stdlib.h>
#include "../include/queue.h"

// #define TEST_QUEUE
/* HOW TO TEST
Uncomment #define TEST_QUEUE before testing
cd to your Project
run gcc -Iinclude -o queue src/queue.c
*/

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL)
        {
                perror("Queue is NULL !\n");
                exit(1);
        }
        if (q->size == MAX_QUEUE_SIZE)
        {
                perror("Queue is full !\n");
                exit(1);
        }
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (empty(q) == 1)
        {
                return NULL;
        }
        struct pcb_t *temp = q->proc[0];
#ifdef MLQ_SCHED
        // Element in queue have the same prioprity
        // So that, just pop the first element in queue.
        int length = q->size - 1;
        for (int i = 0; i < length; ++i)
        {
                q->proc[i] = q->proc[i + 1];
        }
        q->proc[length] = NULL;
        q->size--;
        return temp;
#else
        // Compare priority and pop the element which has prioprity smallest
        int index = 0;
        int length = q->size;
        for (int i = 1; i < length; ++i)
        {
                if (temp->priority < q->proc[i]->priority)
                {
                        temp = q->proc[i];
                        index = i;
                }
        }
        q->proc[index] = q->proc[length - 1];
        q->proc[length - 1] = NULL;
        q->size--;
        return temp;
#endif
}

#ifdef TEST_QUEUE
void initProc(struct pcb_t *proc);
void printProc(struct pcb_t *proc);
int main()
{
        struct queue_t *q = NULL;
        printf("--------------------------- BEGIN TEST EMPTY QUEUE -----------------------------\n");
        printf("Queue is Empty: %s\n", empty(q) == 1 ? "True" : "False");

        printf("\n--------------------------- BEGIN TEST ENQUEUE AND DEQUEUE 1 PROC -----------------------------\n");
        struct pcb_t *proc = (struct pcb_t *)malloc(sizeof(struct pcb_t));
        if (proc == NULL)
                printf("Fail to allocate memory\n");
        initProc(proc);
        
        q = (struct queue_t *)malloc(sizeof(struct queue_t));
        if (q != NULL)
                printf("Allocate memory for Queue\n");
        q->size = 0;
        printf("Add Proc to Queue.\n");
        enqueue(q, proc);
        if (empty(q) == 0)
        {
                printf("Queue Size: %d\nDequeue:\n", q->size);
                printProc(dequeue(q));
                printf("Queue Size After Dequeue: %d\n", q->size);
        }
        free(proc);

        printf("\n--------------------------- BEGIN TEST ENQUEUE AND DEQUEUE 1 ARRAY PROC -----------------------------\n");
        struct pcb_t *arrayProc = (struct pcb_t *)malloc((MAX_QUEUE_SIZE + 1) * sizeof(struct pcb_t));
        for (int i = 0; i < MAX_QUEUE_SIZE + 1; i++)
                initProc(&arrayProc[i]);
        for (int i = 0; i < MAX_QUEUE_SIZE + 1; i++)
        {
                arrayProc[i].pid = i + 1;
                printf("Add Proc to Queue\n");
                enqueue(q, &arrayProc[i]);
                printf("Dequeue\n");
                printProc(dequeue(q));
        }

        printf("\n--------------------------- BEGIN TEST ENQUEUE UNTIL QUEUE IS FULL -----------------------------\n");
        for (int i = 0; i < MAX_QUEUE_SIZE + 1; i++)
        {
                arrayProc[i].pid = i + 1;
                printf("Add Proc to Queue\n");
                printProc(&arrayProc[i]);
                enqueue(q, &arrayProc[i]);
        }
        free(q);
        free(arrayProc);
}
void initProc(struct pcb_t *proc)
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
        proc->prio = 0;
        proc->priority = 0;
        for (int i = 0; i < 10; i++)
                proc->regs[i] = 0;
}
void printProc(struct pcb_t *proc)
{
        printf("Bp: %d Pc: %d Pid: %d Prio: %d Priority: %d\n", proc->bp, proc->pc, proc->pid, proc->prio, proc->priority);
}
#endif