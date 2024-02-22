#include <stdio.h>
#include <stdlib.h>

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

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

void trap_kernel_handler(ExceptionInfo *info) {
        
}

void trap_clock_handler(ExceptionInfo *info) {

}

void trap_illegal_handler(ExceptionInfo *info) {

}

void trap_memory_handler(ExceptionInfo *info) {

}

void trap_math_handler(ExceptionInfo *info) {

}

void trap_tty_receive_handler(ExceptionInfo *info) {

}

void trap_tty_transmit_handler(ExceptionInfo *info) {

}

/*
 * Yalnix Kernel Calls Declaration
 */
int Fork(void);
int Exec(char *filename, char **argvec);
void Exit(int status);
int Wait(int *status_ptr);
int GetPid(void);
int Brk(void *addr);
int Delay(int clock_ticks);
int TtyRead(int tty_id, void *buf, int len);
int TtyWrite(int tty_id, void *buf, int len);

/*
 * Kernel Boot Entry Point
 */
void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args){
        // Create idle process with pid 0 and init process with pid 1

        // Have a corresponding initialized SavedContext to use in loading the CPU state of this process

        // perform any initialization necessary for your kernel or required by the hardware

        /* Initialization */

        // Initialize the interrupt vector table entries for each type of interrupt, exception, or trap, by making
them point to the correct handler functions in your kernel
}



/*
 * Kernel Memory Management
 */
int SetKernelBrk(void *addr){

}