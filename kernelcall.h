/* kernel call */

#ifndef KERNELCALL
#define KERNELCALL

extern int Fork(void);

extern int ExecFunc(char *filename, char **argvec);

extern void Exit(int status);

extern int Wait(int *status_ptr);

extern int GetPid(void);

extern int Brk(void *addr);

extern int Delay(int clock_ticks);

extern int TtyRead(int tty_id, void *buf, int len);

extern int TtyWrite(int tty_id, void *buf, int len);

#endif
