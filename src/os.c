
#include "../include/cpu.h"
#include "../include/timer.h"
#include "../include/sched.h"
#include "../include/loader.h"
#include "../include/mm.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int time_slot;			// Thời gian của mỗi slot
static int num_cpus;			// Số lương  CPU 
static int done = 0;			// Cò báo hiệu trạng thái của hệ thống, 1: các tiến trình đã hoàn thành

#ifdef MM_PAGING
static int memramsz;
static int memswpsz[PAGING_MAX_MMSWP];
#ifdef MM_PAGING_HEAP_GODOWN
static int vmemsz;
#endif

struct mmpaging_ld_args {
	/* A dispatched argument struct to compact many-fields passing to loader */
	int vmemsz;
	struct memphy_struct *mram;
	struct memphy_struct **mswp;
	struct memphy_struct *active_mswp;
	struct timer_id_t  *timer_id;
};
#endif

static struct ld_args{
	char ** path;							// Đường dẫn
	unsigned long * start_time;				// Thời gian bắt đầu của tiến tình
#ifdef MLQ_SCHED
	unsigned long * prio;					// priority
#endif
} ld_processes;
int num_processes;							// Số lượng tiến trình sẽ được tải vào hệ thống

struct cpu_args {
	struct timer_id_t * timer_id;			// Định dạng của bộ định thời(điều phối thời gian)
	int id;									// ID của CPU
};

/*
	Hàm xử lý của từng CPU: kiểm tả ready queue để lấy tiến trình để xử lý
--> Nếu tiến trình kết thúc hoặc hết thời gian trong slot(trả lại hàng đợi)
--> Dừng CPU khi tất cả hoàn tất
*/
static void * cpu_routine(void * args) {
	struct timer_id_t * timer_id = ((struct cpu_args*)args)->timer_id;
	int id = ((struct cpu_args*)args)->id;
	/* Check for new process in ready queue */
	int time_left = 0;						// Thời gian còn lại của tiến trình hiện tại
	struct pcb_t * proc = NULL;				// Con trỏ đến tiến trình CPU đang xử lý
	while (1) {
		/* Check the status of current process */
		if (proc == NULL) {    				// Chưa có tiến trình đang chạy
			/* No process is running, the we load new process from
		 	* ready queue */
			proc = get_proc();				// Lấy một tiến trình đang đợi ở hàng đợi
			if (proc == NULL) {
                next_slot(timer_id);
                continue; 					/* First load failed. skip dummy load */
            }
		}else if (proc->pc == proc->code->size) { // Con trỏ chương trình bằng kích thước tổng số lệnh của tiến trình
			/* The porcess has finish it job */
			printf("\tCPU %d: Processed %2d has finished\n", id , proc->pid);
			free(proc);
			proc = get_proc();
			time_left = 0;						// Reset thời gian cho tiến trình mới
		}else if (time_left == 0) {				// Tiến trình hiện tại đã hết thời gian trong slot
			/* The process has done its job in current time slot */
			printf("\tCPU %d: Put process %2d to run queue\n", id, proc->pid);
			put_proc(proc);						// Đưa tiến trình trở lại ready queue
			proc = get_proc();
		}
		
		/* Recheck process status after loading new process */
		if (proc == NULL && done) {			// 
			/* No process to run, exit */
			printf("\tCPU %d stopped\n", id);
			break;
		}else if (proc == NULL) {			// Không có tiền trình trong hàng đợi nhưng hệ thống chưa hoàn thành
			/* There may be new processes to run in
			 * next time slots, just skip current slot */
			next_slot(timer_id);
			continue;
		}else if (time_left == 0) {
			printf("\tCPU %d: Dispatched process %2d\n", id, proc->pid);  // Đã nhận được tiến trình và băt sđầu thực hiện nó
			time_left = time_slot;
		}
		
		/* Run current process */
		run(proc);
		time_left--;
		next_slot(timer_id);
	}
	detach_event(timer_id);
	pthread_exit(NULL);
}
/*Tải các tiến trình vào hệ thống từ một tập tin*/
static void * ld_routine(void * args) {
#ifdef MM_PAGING
	struct memphy_struct* mram = ((struct mmpaging_ld_args *)args)->mram;
	struct memphy_struct** mswp = ((struct mmpaging_ld_args *)args)->mswp;
	struct memphy_struct* active_mswp = ((struct mmpaging_ld_args *)args)->active_mswp;
	struct timer_id_t * timer_id = ((struct mmpaging_ld_args *)args)->timer_id;
#else
	struct timer_id_t * timer_id = (struct timer_id_t*)args;
#endif
	int i = 0;
	printf("ld_routine\n"); // Bắt đầu thực hiện hàm ld_routine
	/*Tải tiến trình*/
	while (i < num_processes) {
		struct pcb_t * proc = load(ld_processes.path[i]); // Đọc tập tin tiến trình và trả về PCB 
#ifdef MLQ_SCHED
		proc->prio = ld_processes.prio[i];
#endif
		/* Chờ đến khi thời gian hiện tại bằng với thời gian bắt đầu của tiến trình*/
		while (current_time() < ld_processes.start_time[i]) {
			next_slot(timer_id);
		}
#ifdef MM_PAGING
		proc->mm = malloc(sizeof(struct mm_struct));   // Cấp phát bộ nhớ cho tiến trình để quản lý bộ nhớ
#ifdef MM_PAGING_HEAP_GODOWN
		proc->vmemsz = vmemsz;
#endif
		init_mm(proc->mm, proc);
		proc->mram = mram;
		proc->mswp = mswp;
		proc->active_mswp = active_mswp;
#endif
		printf("\tLoaded a process at %s, PID: %d PRIO: %ld\n",
			ld_processes.path[i], proc->pid, ld_processes.prio[i]);
		add_proc(proc);									// Thêm tiến trình vào hàng đợi sẵn sàng
		free(ld_processes.path[i]);					
		i++;
		next_slot(timer_id);
	}
	free(ld_processes.path);
	free(ld_processes.start_time);
	done = 1;											// Đánh dấu tất cả tiến tình đã được tải xong
	detach_event(timer_id);
	pthread_exit(NULL);									// Kết thúc luồng hiện tại
}
/*Đọc tệp cấu hình, lưu các thông tin: tiến trình, thời gian bắt đầu, ưu tiên, cấu hình liên quan đến bộ nhớ*/
static void read_config(const char * path) {
	FILE * file;
	if ((file = fopen(path, "r")) == NULL) {
		printf("Cannot find configure file at %s\n", path);
		exit(1);
	}
	fscanf(file, "%d %d %d\n", &time_slot, &num_cpus, &num_processes);
	/*Cấp phát 2 mảng lưu đường dẫn và start_time*/
	ld_processes.path = (char**)malloc(sizeof(char*) * num_processes);						  // Mảng con trỏ chuỗi lưu đường dẫn của từng tiến trình
	ld_processes.start_time = (unsigned long*) malloc(sizeof(unsigned long) * num_processes); // mảng lưu thời điểm bắt đầu của từng tiến trình
#ifdef MM_PAGING
	int sit;
#ifdef MM_FIXED_MEMSZ
	/* We provide here a back compatible with legacy OS simulatiom config file
         * In which, it have no addition config line for Mema, keep only one line
	 * for legacy info 
         *  [time slice] [N = Number of CPU] [M = Number of Processes to be run]
         */
        memramsz    =  0x100000;
        memswpsz[0] = 0x1000000;
	for(sit = 1; sit < PAGING_MAX_MMSWP; sit++)
		memswpsz[sit] = 0;
#ifdef MM_PAGING_HEAP_GODOWN
	vmemsz = 0x300000;
#endif
#else
	/* Read input config of memory size: MEMRAM and upto 4 MEMSWP (mem swap)
	 * Format: (size=0 result non-used memswap, must have RAM and at least 1 SWAP)
	 *        MEM_RAM_SZ MEM_SWP0_SZ MEM_SWP1_SZ MEM_SWP2_SZ MEM_SWP3_SZ
	*/
	/*Đọc tệp cấu hình các giá trị RAM, SWAP*/
	fscanf(file, "%d\n", &memramsz);
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
		fscanf(file, "%d", &(memswpsz[sit])); 
#ifdef MM_PAGING_HEAP_GODOWN
	fscanf(file, "%d\n", &vmemsz);
#endif

       fscanf(file, "\n"); /* Final character */
#endif
#endif

#ifdef MLQ_SCHED
	/*Khởi tạo mảng lưu độ ưu tiên của từng tiến trình*/
	ld_processes.prio = (unsigned long*)malloc(sizeof(unsigned long) * num_processes);
#endif
	int i;
	for (i = 0; i < num_processes; i++) {
		ld_processes.path[i] = (char*)malloc(sizeof(char) * 100);
		ld_processes.path[i][0] = '\0';
		strcat(ld_processes.path[i], "input/proc/");  // ghép chuỗi
		char proc[100];
#ifdef MLQ_SCHED
		fscanf(file, "%lu %s %lu\n", &ld_processes.start_time[i], proc, &ld_processes.prio[i]);
#else
		fscanf(file, "%lu %s\n", &ld_processes.start_time[i], proc);
#endif
		strcat(ld_processes.path[i], proc);
	}
}

/*
	argc: số lượng tham số truyền vào
	argv[]: Chưa các tham số truyền vào
*/
int main(int argc, char * argv[]) {
	/* Read config */
	if (argc != 2) {
		printf("Usage: os [path to configure file]\n"); 		// name / path
		return 1;
	}
	char path[100];
	path[0] = '\0';
	strcat(path, "input/");
	strcat(path, argv[1]);										// argv[1]: File input được đọc
	read_config(path);
	/*Tạo luồng cho CPU và bộ nạp*/
	pthread_t * cpu = (pthread_t*)malloc(num_cpus * sizeof(pthread_t));
	struct cpu_args * args =									// Mảng chứa các tham số (timer_id + id) cho CPU
		(struct cpu_args*)malloc(sizeof(struct cpu_args) * num_cpus);
	pthread_t ld;												// luồng cho bộ nạp loader
	
	/* Init timer cho mỗi CPU*/
	int i;
	for (i = 0; i < num_cpus; i++) {
		args[i].timer_id = attach_event();  					// M
		args[i].id = i;
	}
	struct timer_id_t * ld_event = attach_event();
	start_timer();

#ifdef MM_PAGING
	/* Init all MEMPHY include 1 MEMRAM and n of MEMSWP */
	int rdmflag = 1; /* By default memphy is RANDOM ACCESS MEMORY */

	struct memphy_struct mram;						// Chưa thông tin bộ nhớ vật lý
	struct memphy_struct mswp[PAGING_MAX_MMSWP];	// Mảng chứa cấu trúc bộ nhớ hoán đổi

	/* Create MEM RAM */
	init_memphy(&mram, memramsz, rdmflag);

     /* Create all MEM SWAP */ 
	int sit;
	for(sit = 0; sit < PAGING_MAX_MMSWP; sit++)
	    init_memphy(&mswp[sit], memswpsz[sit], rdmflag);

	/* In Paging mode, it needs passing the system mem to each PCB through loader*/
	// Chứa các tham số cho quá trình nạp chương trình
	struct mmpaging_ld_args *mm_ld_args = malloc(sizeof(struct mmpaging_ld_args));

	mm_ld_args->timer_id = ld_event;
	mm_ld_args->mram = (struct memphy_struct *) &mram;
	mm_ld_args->mswp = (struct memphy_struct**) &mswp;
#ifdef MM_PAGING_HEAP_GODOWN
	mm_ld_args->vmemsz = vmemsz;
#endif
	mm_ld_args->active_mswp = (struct memphy_struct *) &mswp[0];
#endif
	/* Init scheduler */
	init_scheduler();  // 2

	/* Run CPU and loader */
#ifdef MM_PAGING
	pthread_create(&ld, NULL, ld_routine, (void*)mm_ld_args);			// tạo một luồng mới
#else
	pthread_create(&ld, NULL, ld_routine, (void*)ld_event);
#endif
	for (i = 0; i < num_cpus; i++) {
		pthread_create(&cpu[i], NULL,
		cpu_routine, (void*)&args[i]);
	}

	/* Wait for CPU and loader finishing */
	for (i = 0; i < num_cpus; i++) {
		pthread_join(cpu[i], NULL);										// Chờ một luồng hoàn thành
	}
	pthread_join(ld, NULL);

	/* Stop timer */
	stop_timer();

	return 0;

}