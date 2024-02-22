#include <stdio.h>
#include <stdlib.h>

#include <comp421/yalnix.h>
#include <comp421/hardware.h>

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

extern void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args);

/*
 * Kernel Boot Entry Point
 */
void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args){

}
/*
 * Kernel Memory Management
 */
int SetKernelBrk(void *addr){

}
