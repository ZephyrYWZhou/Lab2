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

/* End of the declaration region */

/*----------------------Kernel Start Routings------------------------*/
void KernelStart(ExceptionInfo *info, unsigned int pmem_size, 
	void *orig_brk, char **cmd_args)
{
	// some book-keeping of global variables
	end_of_kernel_heap = orig_brk;
	exception_info = info;
	TracePrintf(10, "before IVTvm is\n");

	//Initialize the interrupt vector table entries and set its address to the privileged register
	InitInterruptVectorTable();
	TracePrintf(10, "done with init IVT before free list\n");

	// make a list to keep track of free physical pages
	initFreePhysicalPages(pmem_size);

	TracePrintf(10, "done with free list before page table\n");

	terminal[0] = (Terminal *) Malloc(sizeof(Terminal));
	terminal[1] = (Terminal *) Malloc(sizeof(Terminal));
	terminal[2] = (Terminal *) Malloc(sizeof(Terminal));
	terminal[3] = (Terminal *) Malloc(sizeof(Terminal));
 	//Tty Initialzation
	terminal[0]->num_char = 0;
 	terminal[0]->readterm_lineList_head = NULL;
 	terminal[1]->num_char = 0;
 	terminal[1]->readterm_lineList_head = NULL;
 	terminal[2]->num_char = 0;
 	terminal[2]->readterm_lineList_head = NULL;
 	terminal[3]->num_char = 0;
 	terminal[3]->readterm_lineList_head = NULL;

	// initialize region 1 & region 0 page table, initialize REG_PTR0 and REG_PTR1
	initPageTable();

	TracePrintf(10, "done with page table before vm\n");

	// enable VM
	enable_virtual_memory();

	
	// initialize init process
 	init_process(cmd_args, exception_info);
 	TracePrintf(10, "done with init proc\n");

	// initialize idle process
 	idle_process();
 

 	if (loaded == 0) {
 		loaded = 1;
		int load_status = LoadProgram(cmd_args[0], cmd_args, exception_info);
		
		TracePrintf(9, "Status of init loaded is %d\n", load_status); 		
 	} else {
 		int idle_status = LoadProgram("idle", cmd_args, exception_info);
 		
 		TracePrintf(9, "Status of idle loaded is %d\n", idle_status); 
 	}

 	TracePrintf(9, "[KernelStart] DONE \n"); 
}

int SetKernelBrk(void *addr) {
	if (vm_enabled == 0) {
		end_of_kernel_heap = addr;
		return 0;
	} else {
		TracePrintf(0, "Addr of SetKernelBrk is %p\n", addr);
		TracePrintf(0, "pid %d\n SetKernelBrk", current_pcb->pid);
		int set_brk = (UP_TO_PAGE((int)addr) >> PAGESHIFT) - PAGE_TABLE_LEN;

		if (end_of_kernel_heap > VMEM_1_LIMIT - 2) {
			TracePrintf(0, "end_of_kernel_heap > VMEM_1_LIMIT - 2");
			return ERROR;
		}

		if (end_of_kernel_heap < VMEM_1_BASE) {
			TracePrintf(0, "end_of_kernel_heap < VMEM_1_BASE");
			return ERROR;
		}

		// case1: set_brk bigger than brk_pos, move up 
		int current_pcb_vpn = (UP_TO_PAGE(end_of_kernel_heap) >> PAGESHIFT) - PAGE_TABLE_LEN;
		TracePrintf(0, "Current pcb vpn is %d\n", current_pcb_vpn);
		TracePrintf(0, "Set heap to %d\n",set_brk);
		int num_pages_up = (set_brk - current_pcb_vpn);
		int i;
		for (i = 0; i < num_pages_up; i++) {
			if(current_pcb_vpn + i > PAGE_TABLE_LEN-3) {
				TracePrintf(9, "[SetKernelBrk] Error: Not enough memory \n");
				return -1;
			}
			//enqueue a page
			page_table_region_1[current_pcb_vpn + i].pfn = allocate_new_pfn();
			page_table_region_1[current_pcb_vpn+ i].valid = 1;
			page_table_region_1[current_pcb_vpn + i].uprot = PROT_NONE;
			page_table_region_1[current_pcb_vpn + i].kprot = (PROT_READ | PROT_WRITE);			
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

		}
		end_of_kernel_heap = addr;
	}
	TracePrintf(9, "[SetKernelBrk] DONE \n"); 
	return 0;
}

void init_process(char **cmd_args, ExceptionInfo *info) {
	// step1: create new PCB
	pcb *init_pcb = (pcb *) Malloc(sizeof(pcb));
	init_pcb -> pid = 1;
	TracePrintf(9, "init process before initializing \n");
	// initialize the pt0 field
	init_pcb -> status = RUNNING;
	init_pcb -> pfn_pt0 = PAGE_TABLE_LEN * 2 - 2;
	init_pcb -> nextProc = NULL;
	init_pcb -> switchTime = total_time + 2;
    init_pcb -> brk_pos = region0_brk;
    init_pcb -> user_stack_low_vpn = PAGE_TABLE_LEN - KERNEL_STACK_PAGES - 2;
    init_pcb -> child = NULL;
    init_pcb -> next_child = NULL;
 	init_pcb->sibling_q_head = NULL;
	init_pcb->sibling_q_tail = NULL;
	init_pcb -> exited_childq_head = NULL;
	init_pcb -> exited_childq_tail = NULL; 
	init_pcb->parent = NULL;   

	// step 2: set current process to init
	current_pcb = init_pcb;
	TracePrintf(9, "init process before loading \n");
	TracePrintf(9, "[init_process] DONE \n"); 
}


void idle_process(char **cmd_args) {
	idle_pcb = make_pcb(allocate_new_pfn(), 0);
	idle_pcb -> pid = 0;
	TracePrintf(9, "[idle_process] DONE \n"); 

}

/* START of Kernel Start Sub-Routines */
void InitInterruptVectorTable() {
	int j;
	// Interrupt vector Table: An array of pointers to functions whose input is *ExceptionInfo.
	// Make it locate at the 510th (3rd to last) page of region 1 memory!
	trap_funcptr *InterruptVectorTable = Malloc(TRAP_VECTOR_SIZE * sizeof(trap_funcptr));
	
	// Define each element in the Table.
	InterruptVectorTable[TRAP_KERNEL] = trap_kernel;
	InterruptVectorTable[TRAP_CLOCK] = trap_clock;
	InterruptVectorTable[TRAP_ILLEGAL] = trap_illegal;
	InterruptVectorTable[TRAP_MEMORY] = trap_memory;
	InterruptVectorTable[TRAP_MATH] = trap_math;
	InterruptVectorTable[TRAP_TTY_RECEIVE] = trap_tty_receive;
	InterruptVectorTable[TRAP_TTY_TRANSMIT] = trap_tty_transmit;
	// initialize unused entries to NULL
	for (j = 7; j < 16; j++) {
		InterruptVectorTable[j] = NULL;
	}

	// Initialize the REG_VECTOR_BASE privileged machine register
	WriteRegister(REG_VECTOR_BASE, (RCS421RegVal) InterruptVectorTable);
	TracePrintf(9, "[InitInterruptVectorTable] DONE \n"); 
}

// some more work needs to be done
void initFreePhysicalPages(unsigned int pmem_size) {
	total_pages = DOWN_TO_PAGE(pmem_size) >> PAGESHIFT;
	
	//move the list to the semi-top of region 1!
	free_physical_pages = Malloc(total_pages * sizeof(int));

	// some stuff with invalid memory
	int page_iter;
	for (page_iter = 0; page_iter < PAGE_TABLE_LEN - KERNEL_STACK_PAGES; page_iter++) {
		free_physical_pages[page_iter] = 1;
		free_pages_counter++;
	}
	int heap_pages = UP_TO_PAGE(end_of_kernel_heap - VMEM_1_BASE) >> PAGESHIFT;
	for (page_iter = PAGE_TABLE_LEN - KERNEL_STACK_PAGES; page_iter < PAGE_TABLE_LEN - KERNEL_STACK_PAGES + heap_pages; 
		page_iter++) {
		free_physical_pages[page_iter] = 0;
	}
	for (page_iter = PAGE_TABLE_LEN - KERNEL_STACK_PAGES + heap_pages; page_iter < PAGE_TABLE_LEN * 2 - 2; page_iter++) {
		free_physical_pages[page_iter] = 1;
		free_pages_counter++;
	}
	for (page_iter = PAGE_TABLE_LEN * 2 - 2; page_iter < PAGE_TABLE_LEN * 2; page_iter++) {
		free_physical_pages[page_iter] = 0;
	}
	for (page_iter = PAGE_TABLE_LEN * 2; page_iter < total_pages; page_iter++) {
		free_physical_pages[page_iter] = 1;
		free_pages_counter++;
	}
	free_pages_pointer = 0;
	TracePrintf(9, "[initFreePhysicalPages] DONE \n"); 
}

// Section 3.4.3
void initPageTable() {
	TracePrintf(10, "start of page table\n");
	//initialize region 1 & region 0 page table

	// make the initial pt0 and pt1 live on the top of region 1
	page_table_region_0 = (struct pte*) (VMEM_1_LIMIT - PAGESIZE * 2);
	page_table_region_1 = (struct pte*) (VMEM_1_LIMIT - PAGESIZE);
	
	// step2: fill in the entries with struct pte
	// first fill in the kernel stack for user process in region 0
	int pt_iter0;
	for (pt_iter0 = 0; pt_iter0 < KERNEL_STACK_PAGES; pt_iter0++) {
		int pt_index = PAGE_TABLE_LEN - pt_iter0 - 1; 
		page_table_region_0[pt_index].pfn = pt_index;
		page_table_region_0[pt_index].uprot = PROT_NONE;
		page_table_region_0[pt_index].kprot = (PROT_READ | PROT_WRITE);
		page_table_region_0[pt_index].valid = 1;
		//TracePrintf(10, "page table region 0 : %p\n", page_table_region_0);

	}

	// then fill in the kernel text, data, bss, and heap in region 1
	int pt_iter1;
	for (pt_iter1 = 0; pt_iter1 < (UP_TO_PAGE((end_of_kernel_heap - VMEM_1_BASE)) >> PAGESHIFT); pt_iter1++) {
		page_table_region_1[pt_iter1].pfn = PAGE_TABLE_LEN + pt_iter1;
		page_table_region_1[pt_iter1].uprot = PROT_NONE;
		page_table_region_1[pt_iter1].valid = 1;

		if (VMEM_1_BASE + pt_iter1 * PAGESIZE < &_etext) {
			// KERNEL text
			page_table_region_1[pt_iter1].kprot = (PROT_READ | PROT_EXEC);
		} else {
			// KERNEL data/bss/heap
			page_table_region_1[pt_iter1].kprot = (PROT_READ | PROT_WRITE);
		}
	}
	TracePrintf(10, "done with page table part 2\n");

	page_table_region_1[PAGE_TABLE_LEN-1].pfn = PAGE_TABLE_LEN + PAGE_TABLE_LEN-1;
	page_table_region_1[PAGE_TABLE_LEN-1].valid = 1;
	page_table_region_1[PAGE_TABLE_LEN-1].uprot = PROT_NONE;
	page_table_region_1[PAGE_TABLE_LEN-1].kprot = PROT_READ|PROT_WRITE;
	page_table_region_1[PAGE_TABLE_LEN-2].pfn = PAGE_TABLE_LEN + PAGE_TABLE_LEN-2;
	page_table_region_1[PAGE_TABLE_LEN-2].valid = 1;
	page_table_region_1[PAGE_TABLE_LEN-2].uprot = PROT_NONE;
	page_table_region_1[PAGE_TABLE_LEN-2].kprot = PROT_READ|PROT_WRITE;

	//initialize REG_PTR0 and REG_PTR1
	WriteRegister(REG_PTR0, (RCS421RegVal) page_table_region_0);
	WriteRegister(REG_PTR1, (RCS421RegVal) page_table_region_1);
	TracePrintf(9, "[initPageTable] DONE \n"); 
}

void enable_virtual_memory() {
	WriteRegister(REG_VM_ENABLE, 1);
	vm_enabled = 1;
}

/*----------------------Kernel Start Routings end------------------------*/

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
