/* kernel call */

#ifndef KERNELCALL
#define KERNELCALL

extern int kernelFork(void);

extern int kernelExec(char *filename, char **argvec);

extern void kernelExit(int status);

extern int kernelWait(int *status_ptr);

extern int kernelGetpid(void);

extern int kernelBrk(void *addr);

extern int kernelDelay(int clock_ticks);

extern int kernelTtyread(int tty_id, void *buf, int len);

extern int kernelTtywrite(int tty_id, void *buf, int len);

#endif
