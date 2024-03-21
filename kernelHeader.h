#ifndef yalnix_kernel_h
#define yalnix_kernel_h

#include <stdio.h>

#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* -------------Structure declaration------------- */
typedef struct pte pte;

/*the status queue for waiting*/
typedef struct status_fifo_q
{
    int pid;
    int status;
    struct status_fifo_q *next;
}StatusQueue;

/*PCB data structure */
typedef struct pcb {
    unsigned int pid;
    struct pte *pt_r0;
    SavedContext *ctx;
    int n_child;
    int delay_clock;
    unsigned long brk;
    StatusQueue *statusQ;
    struct pcb *parent;
    struct pcb *next;
}pcb;

typedef pcb ProcessControlBlock;

/*free page linked list data structure*/
typedef struct pf {
    unsigned int phys_frame_num;
    struct pf *next;
}phys_frame;

typedef struct terminal
{
    int n_buf_char;
    char read_buf[256];
    char *write_buf;
    pcb *readQ_head;
    pcb *readQ_end;
    pcb *writingProc;
    pcb *writeQ_head;
    pcb *writeQ_end;
}terminal;

/* -------------Global variables declaration------------- */
extern phys_frame *free_frames_head; /* head of free phys frames */
extern unsigned long next_PT_vaddr; /* data structure for marking allocated page table */
extern int half_full;
extern int free_frame_cnt;
typedef void (*interruptHandler)(ExceptionInfo *info);
extern interruptHandler interruptVector[TRAP_VECTOR_SIZE];
extern pte *pt_r1;
extern pte idle_pt_r0[PAGE_TABLE_LEN];
extern pcb *currentProc;
extern pcb *readyQ_head, *readyQ_end;
extern pcb *waitQ_head, *waitQ_end;
extern pcb *delayQ_head;
extern pcb *idleProc;
extern terminal yalnix_term[NUM_TERMINALS];
extern char kernel_stack_buff[PAGESIZE*KERNEL_STACK_PAGES];
extern unsigned int next_pid;
extern void *kernel_brk;
extern int vm_enabled;

/* -------------Kernel call functions declaration------------- */
extern int kernel_Fork(void);
extern int kernel_Exec(char *filename, char **argvec, ExceptionInfo *info);
extern void kernel_Exit(int status);
extern int kernel_Wait(int *status_ptr);
extern int kernel_Getpid(void);
extern int kernel_Brk(void *addr);
extern int kernel_Delay(int clock_ticks);
extern int kernel_Ttyread(int tty_id, void *buf, int len);
extern int kernel_Ttywrite(int tty_id, void *buf, int len);

/* -------------Trap handlers declaration------------- */
void trap_kernel_handler(ExceptionInfo *info);
void trap_clock_handler(ExceptionInfo *info);
void trap_illegal_handler(ExceptionInfo *info);
void trap_memory_handler(ExceptionInfo *info);
void trap_math_handler(ExceptionInfo *info);
void trap_tty_receive_handler(ExceptionInfo *info);
void trap_tty_transmit_handler(ExceptionInfo *info);


/* -------------Context switch functions declaration------------- */
SavedContext *switch_sf(SavedContext *ctpx, void *p1, void *p2);
SavedContext *init_sf(SavedContext *ctpx, void *p1, void *p2);
SavedContext *delay_sf(SavedContext *ctpx, void *p1, void *p2);
SavedContext *fork_sf(SavedContext *ctpx, void *p1, void *p2);
SavedContext *exit_sf(SavedContext *ctpx, void *p1, void *p2);
SavedContext *wait_sf(SavedContext *ctpx, void *p1, void *p2);
SavedContext *tty_sf(SavedContext *ctpx, void *p1, void *p2);

/* -------------Helper function declaration------------- */
int LoadProgram(char *name, char **args, ExceptionInfo *info);
int used_pgn_r0(void);
unsigned long get_free_page(void);
void remove_used_page(pte *p);
void allocate_pt(pcb* p);
unsigned long user_stack_bot(void);
void add_ready_queue(ProcessControlBlock *p);
void add_wait_queue(ProcessControlBlock *p);
void add_read_queue(int tty_id, ProcessControlBlock* p);
void add_write_queue(int tty_id, ProcessControlBlock* p);
ProcessControlBlock *next_ready_queue(void);
ProcessControlBlock *next_read_queue(int tty_id);
ProcessControlBlock *next_write_queue(int tty_id);
ProcessControlBlock *next_wait_queue(void);
void update_delay_queue(void);
RCS421RegVal vaddr2paddr(unsigned long vaddr);

#endif