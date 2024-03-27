#include "kernelHeader.h"

/*variables definitions*/
phys_frame *free_frames_head;   // Pointer to the head of the list of free physical frames
int num_free_frame = 0;         // Count of free frames

unsigned int next_pid = 0;      // Next process ID to be assigned
void *kernel_brk;               // Current kernel break pointer
int VM_flag = 0;                // Virtual memory flag

unsigned long next_PT_vaddr = VMEM_1_LIMIT - PAGESIZE; // Next page table virtual address
int half_full = 0;              // Flag indicating whether the kernel stack is half full

terminal yalnix_term[NUM_TERMINALS];    // Array of terminal structures
char kernel_stack_buff[PAGESIZE * KERNEL_STACK_PAGES]; // Kernel stack buffer

interruptHandler interruptVector[TRAP_VECTOR_SIZE];   // Interrupt vector table
struct pte *pt_r1;              // Page table register 1 (r1)
struct pte idle_pt_r0[PAGE_TABLE_LEN];   // Idle process page table (r0)

pcb *current_process;           // Pointer to the current running process
pcb *delay_queue_head;               // Head of the delay queue
pcb *idle_process;              // Pointer to the idle process
pcb *ready_queue_head = NULL, *ready_queue_end = NULL; // Head and end pointers of the ready queue
pcb *wait_queue_head = NULL, *wait_queue_end = NULL;   // Head and end pointers of the wait queue

void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char** cmd_args) {
    unsigned int i;
    unsigned long addr;

    pt_r1=(pte*)malloc(PAGE_TABLE_SIZE); /* Allocate memory for the level 1 page table */
    kernel_brk = orig_brk; /* Set the kernel break pointer to the original break */
    delay_queue_head=(pcb*)malloc(sizeof(pcb)); /* Allocate memory for the delay queue head */

    /* Initialize interrupt vector table */    
    interruptVector[TRAP_KERNEL] = &trap_kernel_handler;
    interruptVector[TRAP_CLOCK] = &trap_clock_handler;
    interruptVector[TRAP_ILLEGAL] = &trap_illegal_handler;
    interruptVector[TRAP_MEMORY] = &trap_memory_handler;
    interruptVector[TRAP_MATH] = &trap_math_handler;
    interruptVector[TRAP_TTY_RECEIVE] = &trap_tty_receive_handler;
    interruptVector[TRAP_TTY_TRANSMIT] = &trap_tty_transmit_handler;
    for (i = 7; i < TRAP_VECTOR_SIZE; i++) {
        interruptVector[i] = NULL;
    }

    /* Set the interrupt vector base register */
    WriteRegister(REG_VECTOR_BASE, (RCS421RegVal)(interruptVector));

    /* initialize the terminal */
    for(i = 0;i < NUM_TERMINALS;i++) {
        yalnix_term[i].n_buf_char = 0;
        yalnix_term[i].write_buf = NULL;
        yalnix_term[i].write_queue_head = NULL;
        yalnix_term[i].write_queue_end = NULL;
        yalnix_term[i].writingProc = NULL;
        yalnix_term[i].read_queue_head = NULL;
        yalnix_term[i].read_queue_end = NULL;
    }

    /* initialize the free phys pages structure */
    free_frames_head = (phys_frame*)malloc(sizeof(phys_frame));
    phys_frame *tmp = free_frames_head;
    for (i= PMEM_BASE; i < PMEM_BASE+pmem_size; i += PAGESIZE){
        tmp->next = (phys_frame*)malloc(sizeof(phys_frame));
        tmp = tmp->next;
        tmp->phys_frame_num = num_free_frame;
        num_free_frame++;
    }
    tmp = free_frames_head;
    phys_frame* t;
    while (tmp->next!=NULL) {
        if (tmp->next->phys_frame_num >= (KERNEL_STACK_BASE>>PAGESHIFT) && tmp->next->phys_frame_num<((unsigned long)kernel_brk>>PAGESHIFT)) {
            t = tmp->next;
            tmp->next = tmp->next->next;
            num_free_frame--;
            free(t);
        }
        else tmp = tmp->next;
    }

    /* initialize r1 page table */
    WriteRegister(REG_PTR1,(RCS421RegVal)(pt_r1));
    for (addr = VMEM_1_BASE; addr<(unsigned long)(&_etext); addr+=PAGESIZE) {
        i = (addr-VMEM_1_BASE)>>PAGESHIFT;
        pt_r1[i].pfn = addr>>PAGESHIFT;
        pt_r1[i].valid = 1;
        pt_r1[i].kprot = PROT_READ|PROT_EXEC;
        pt_r1[i].uprot = PROT_NONE;
    }
    for (; addr<(unsigned long)kernel_brk; addr += PAGESIZE) {
        i = (addr-VMEM_1_BASE)>>PAGESHIFT;
        pt_r1[i].pfn = addr>>PAGESHIFT;
        pt_r1[i].valid = 1;
        pt_r1[i].kprot = PROT_READ|PROT_WRITE;
        pt_r1[i].uprot = PROT_NONE;
    }

    /* build and initialize page table r0 */
    WriteRegister(REG_PTR0,(RCS421RegVal)(idle_pt_r0));
    for (addr = KERNEL_STACK_BASE; addr<KERNEL_STACK_LIMIT; addr+=PAGESIZE) {
        i = addr>>PAGESHIFT;
        idle_pt_r0[i].pfn = addr>>PAGESHIFT;
        idle_pt_r0[i].valid = 1;
        idle_pt_r0[i].kprot = PROT_READ|PROT_WRITE;
        idle_pt_r0[i].uprot = PROT_NONE;
    }

    /* enable virtual memory */
    WriteRegister(REG_VM_ENABLE,1);
    VM_flag = 1;

    /* create idle process */
    idle_process = (pcb*)malloc(sizeof(pcb));
    idle_process->pid = next_pid++;
    idle_process->pt_r0 = idle_pt_r0;
    idle_process->ctx = (SavedContext*)malloc(sizeof(SavedContext));
    idle_process->statusQ = NULL;
    idle_process->parent = NULL;
    idle_process->next = NULL;
    idle_process->n_child = 0;
    idle_process->delay_clock = 0;
    idle_process->brk = MEM_INVALID_PAGES;

    /*set current_process as idle*/
    current_process = idle_process;
    LoadProgram("idle", cmd_args, info);

    /* create init process */
    pcb *initProc = (pcb*)malloc(sizeof(pcb));
    initProc->pid = next_pid++;
    allocate_page_table(initProc);

    initProc->ctx = (SavedContext*)malloc(sizeof(SavedContext));
    initProc->n_child = 0;
    initProc->delay_clock = 0;
    initProc->brk = MEM_INVALID_PAGES;
    initProc->statusQ = NULL;
    initProc->parent = NULL;
    initProc->next = NULL;

    for (addr = KERNEL_STACK_BASE; addr < KERNEL_STACK_LIMIT; addr += PAGESIZE) {
        i = addr>>PAGESHIFT;
        initProc->pt_r0[i].pfn = get_free_page();
        initProc->pt_r0[i].valid = 1;
        initProc->pt_r0[i].kprot = PROT_READ|PROT_WRITE;
        initProc->pt_r0[i].uprot = PROT_NONE;
    }

    /* Perform a context switch to the init process */
    ContextSwitch(init_save_flush,current_process->ctx,current_process,initProc);//XXX

    /* Load programs based on the current process */
    if (current_process->pid == 0)
        LoadProgram("idle", cmd_args, info);
    else if (current_process->pid == 1) {
        if (cmd_args == NULL || cmd_args[0]==NULL) {
            LoadProgram("init", cmd_args, info);
        }
        else {
            LoadProgram(cmd_args[0], cmd_args, info);
        }
    }
}

int SetKernelBrk(void *addr) {
    // Check if virtual memory is disabled
    if (VM_flag == 0) {
        // If address exceeds the upper limit of virtual memory, return -1
        if ((unsigned long)addr > VMEM_1_LIMIT)
            return -1;
        // Set the kernel break to the specified address
        kernel_brk = addr;
    }
    // If virtual memory is enabled
    else {
        unsigned long a, idx;
        // Calculate the difference between the new address and the current kernel break
        if ((unsigned long)addr - UP_TO_PAGE(kernel_brk) > PAGESIZE * num_free_frame) {
            // If the difference exceeds available free frames, return -1
            return -1;
        }
        // Iterate through pages from the current kernel break to the new address
        for (a = UP_TO_PAGE(kernel_brk) - 1; a < (unsigned long)addr; a += PAGESIZE) {
            // Calculate the index of the page table entry for the current address
            idx = (a - VMEM_1_BASE) >> PAGESHIFT;
            // If the page table entry is not valid, allocate a new page
            if (pt_r1[idx].valid == 0) {
                pt_r1[idx].pfn = get_free_page();
                pt_r1[idx].valid = 1;
                pt_r1[idx].kprot = PROT_READ | PROT_WRITE;
                pt_r1[idx].uprot = PROT_NONE;
            }
        }
    }
    // Return 0 indicating success
    return 0;
}


void trap_memory_handler(ExceptionInfo *info) {
    unsigned long addr = (unsigned long)(info->addr);
    unsigned long userstackbottom=user_stack_bot();
    unsigned long i;

    unsigned long addr_vpn = addr>>PAGESHIFT;
    unsigned long usbot_vpn = userstackbottom>>PAGESHIFT;
    unsigned long brk_vpn = UP_TO_PAGE(current_process->brk)>>PAGESHIFT;
    unsigned long down_addr_vpn = DOWN_TO_PAGE(addr)>>PAGESHIFT;

    if (addr_vpn <= usbot_vpn && addr_vpn > brk_vpn && (usbot_vpn-down_addr_vpn) < num_free_frame) {
        for (i = addr>>PAGESHIFT;i <= userstackbottom>>PAGESHIFT;i++) {
            if ((current_process->pt_r0)[i].valid) {
                Halt();
            }
            (current_process->pt_r0)[i].valid = 1;
            (current_process->pt_r0)[i].pfn = get_free_page();
            (current_process->pt_r0)[i].kprot = PROT_READ|PROT_WRITE;
            (current_process->pt_r0)[i].uprot = PROT_READ|PROT_WRITE;
        }
    }
    else {
        printf("Illegal memory access \n");
        kernel_Exit(ERROR);
    }
}

void trap_math_handler(ExceptionInfo *info) {
    switch(info->code){
        case TRAP_MATH_INTDIV:
            printf("Integer divide by zero \n");
            break;
        case TRAP_MATH_INTOVF:
            printf("Integer overflow \n");
            break;
        case TRAP_MATH_FLTDIV:
            printf("Floating divide by zero \n");
            break;
        case TRAP_MATH_FLTOVF:
            printf("Floating overflow \n");
            break;
        case TRAP_MATH_FLTUND:
            printf("Floating underflow \n");
            break;
        case TRAP_MATH_FLTRES:
            printf("Floating inexact result\n");
            break;
        case TRAP_MATH_FLTINV:
            printf("Invalid floating operation \n");
            break;
        case TRAP_MATH_FLTSUB:
            printf("FP subscript out of range \n");
            break;
        case TRAP_MATH_KERNEL:
            printf("Linux kernel sent SIGFPE \n");
            break;
        case TRAP_MATH_USER:
            printf("Received SIGFPE from user \n");
            break;
        default:
            break;
    }
    kernel_Exit(ERROR);
}

void trap_kernel_handler(ExceptionInfo *info) {
    switch(info->code){
        case YALNIX_FORK:
            info->regs[0] = kernel_Fork();
            break;
        case YALNIX_EXEC:
            kernel_Exec((char*)(info->regs[1]), (char**)(info->regs[2]), info);
            break;
        case YALNIX_EXIT:
            kernel_Exit((int)(info->regs[1]));
            break;
        case YALNIX_WAIT:
            info->regs[0] = kernel_Wait((int*)(info->regs[1]));
            break;
        case YALNIX_GETPID:
            info->regs[0] = kernel_Getpid();
            break;
        case YALNIX_BRK:
            info->regs[0] = kernel_Brk((void*)(info->regs[1]));
            break;
        case YALNIX_DELAY:
            info->regs[0] = kernel_Delay((int)(info->regs[1]));
            break;
        case YALNIX_TTY_READ:
            info->regs[0] = kernel_Ttyread((int)(info->regs[1]),(void*)(info->regs[2]),(int)(info->regs[3]));
            break;
        case YALNIX_TTY_WRITE:
            info->regs[0] = kernel_Ttywrite((int)(info->regs[1]),(void*)(info->regs[2]),(int)(info->regs[3]));
            break;
        default:
            break;
    }
}

void trap_illegal_handler(ExceptionInfo *info) {
    switch(info->code) {
        case TRAP_ILLEGAL_ILLOPC:
            printf("Illegal opcode\n");
            break;
        case TRAP_ILLEGAL_PRVOPC:
            printf("Privileged opcode\n");
            break;
        case TRAP_ILLEGAL_OBJERR:
            printf("Object-specific HW error\n");
            break;
        case TRAP_ILLEGAL_ILLOPN:
            printf("Illegal operand\n");
            break;
        case TRAP_ILLEGAL_ILLADR:
            printf("Illegal addressing mode\n");
            break;
        case TRAP_ILLEGAL_ILLTRP:
            printf("Illegal software trap\n");
            break;
        case TRAP_ILLEGAL_PRVREG:
            printf("Illegal register\n");
            break;
        case TRAP_ILLEGAL_COPROC:
            printf("Coprocessor error\n");
            break;
        case TRAP_ILLEGAL_BADSTK:
            printf("Bad stack\n");
            break;
        case TRAP_ILLEGAL_KERNELI:
            printf("Linux kernel sent SIGILL\n");
            break;
        case TRAP_ILLEGAL_ADRALN:
            printf("Invalid address alignment\n");
            break;
        case TRAP_ILLEGAL_KERNELB:
            printf("Linux kernel sent SIGBUS\n");
            break;
        case TRAP_ILLEGAL_ADRERR:
            printf("Non-existant physical address\n");
            break;
        default:
            break;
    }
    kernel_Exit(ERROR);
}

void trap_tty_receive_handler(ExceptionInfo *info) {
    int tty_id = (info->code);
    int receive_count;

    receive_count = TtyReceive(tty_id, yalnix_term[tty_id].read_buf+yalnix_term[tty_id].n_buf_char, TERMINAL_MAX_LINE);
    yalnix_term[tty_id].n_buf_char += receive_count;

    if (yalnix_term[tty_id].read_queue_head!=NULL) {
        ContextSwitch(switch_save_flush, current_process->ctx, current_process, next_read_queue(tty_id));
    }
}

void trap_tty_transmit_handler(ExceptionInfo *info) {
    int tty_id = (info->code);
    ContextSwitch(switch_save_flush, current_process->ctx, current_process, yalnix_term[tty_id].writingProc);
}

void trap_clock_handler(ExceptionInfo *info) {
    update_delay_queue();
    ContextSwitch(switch_save_flush, current_process->ctx, current_process, next_ready_queue());
}