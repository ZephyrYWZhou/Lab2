/*-----------Headers-----------*/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <comp421/loadinfo.h>
#include <comp421/yalnix.h>
#include <comp421/hardware.h>

#include "kernelcall.h"
#include "handles.h"

/*----------Constant Definations---------*/
#define TERMINATED -1
#define RUNNING 0
#define READY 1
#define WAITING 2

#define DelayQueue 0
#define ReadyQueue 1

#define BlockQ_0 3
#define BlockQ_1 4
#define BlockQ_2 5
#define BlockQ_3 6

#define Write_BlockQ_0 8
#define Write_BlockQ_1 9
#define Write_BlockQ_2 10
#define Write_BlockQ_3 11

/* Data Structure Definitions */
// define a trap function pointer type
typedef int (*trap_funcptr)(ExceptionInfo *);

typedef struct exit_child_block {
    int pid;
    int status;
    struct exit_child_block *next;
} ecb;

typedef struct pcb {
	int pid;
	int status;
	SavedContext ctx;
	int pfn_pt0; // page table 0
	struct pcb *nextProc;
	long switchTime;
	int brk_pos;
	void *user_stack_low_vpn;
	struct pcb *child;
	struct pcb *next_child; 
	struct pcb *sibling_q_head;
	struct pcb *sibling_q_tail;
	ecb *exited_childq_head; 
	ecb *exited_childq_tail; 
	struct pcb *parent;

	/* TTY stuff */
	struct pcb *Read_BlockQ_head;

} pcb;

typedef struct Line
{
    char *ReadBuf;
    int length;
    struct Line *nextLine;   
} Line;

typedef struct Terminal
{
    int num_char;
    Line *readterm_lineList_head;
    Line *readterm_lineList_tail;
    int term_writing; // 1 means a process is writing, 0 means no process is writing.
    Line *writeterm_lineList_head;
    Line *writeterm_lineList_tail;

} Terminal;

Terminal *terminal[4];

/* KernelStart Routine */
extern void KernelStart(ExceptionInfo *, unsigned int, void *, char **);
int LoadProgram(char *name, char **args, ExceptionInfo *info);

/* SetKernelBrk Routine */
extern int SetKernelBrk(void *);

/* Context Switch */
SavedContext *MySwitchFunc(SavedContext *, void *, void *);

/* Helper Routines */
pcb *make_pcb(int pfn, int pid);
void *Malloc(size_t size);

/* Global Variables */

//A list to keep track of all free physical pages' indices
int *free_physical_pages; // 1 if free, 0 if not free
struct pte *page_table_region_0; // temporary region 0 pt for the first process
struct pte *page_table_region_1; // region 1 pt, shared by all processes
void *end_of_kernel_heap;
//int region0_brk_pn; // the page number region brk is in 
int region0_brk = MEM_INVALID_PAGES;
int free_pages_counter; // number of free physical pages
int free_pages_pointer; // pointer to the physical pages index that was used last time
ExceptionInfo *exception_info;
int process_id = 2;
int vm_enabled;
int total_pages;
int loaded = 0;
int idle_loaded = 0;

// kernel call necessary global variables
pcb *current_pcb;
pcb *idle_pcb;
struct pte *pt0 = (struct pte*) ((VMEM_1_LIMIT) - (2 * (PAGESIZE)));
unsigned long total_time = 0;

/* Ready and Delay queue vars */
pcb *ready_q_head = NULL, *ready_q_tail = NULL;
pcb *delay_q_head = NULL, *delay_q_tail = NULL;

/* TTY Queue Vars */

pcb *Read_BlockQ_0_head = NULL;
pcb *Read_BlockQ_0_tail = NULL;
pcb *Read_BlockQ_1_head = NULL;
pcb *Read_BlockQ_1_tail = NULL;
pcb *Read_BlockQ_2_head = NULL;
pcb *Read_BlockQ_2_tail = NULL;
pcb *Read_BlockQ_3_head = NULL;
pcb *Read_BlockQ_3_tail = NULL;


pcb *Write_BlockQ_0_head = NULL;
pcb *Write_BlockQ_0_tail = NULL;
pcb *Write_BlockQ_1_head = NULL;
pcb *Write_BlockQ_1_tail = NULL;
pcb *Write_BlockQ_2_head = NULL;
pcb *Write_BlockQ_2_tail = NULL;
pcb *Write_BlockQ_3_head = NULL;
pcb *Write_BlockQ_3_tail = NULL;

/*----------Structure Definations---------*/
// Define the array of function pointers
void (*interrupt_vector_table[TRAP_VECTOR_SIZE])(ExceptionInfo *);

// Assign function pointers to array elements
interrupt_vector_table[TRAP_KERNEL] = &trap_kernel_handler;
interrupt_vector_table[TRAP_CLOCK] = &trap_clock_handler;
interrupt_vector_table[TRAP_ILLEGAL] = &trap_illegal_handler;
interrupt_vector_table[TRAP_MEMORY] = &trap_memory_handler;
interrupt_vector_table[TRAP_MATH] = &trap_math_handler;
interrupt_vector_table[TRAP_TTY_RECEIVE] = &trap_tty_receive_handler;
interrupt_vector_table[TRAP_TTY_TRANSMIT] = &trap_tty_transmit_handler;

extern void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args);

/*
 * Kernel Boot Entry Point
 */
void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args){
        // Create idle process with pid 0 and init process with pid 1

        // Have a corresponding initialized SavedContext to use in loading the CPU state of this process

        // perform any initialization necessary for your kernel or required by the hardware

        /* Initialization */

        // Initialize the interrupt vector table entries for each type of interrupt, exception, or trap, by making them point to the correct handler functions in your kernel
}



/*
 * Kernel Memory Management
 */
int SetKernelBrk(void *addr){

}
