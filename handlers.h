#ifndef HANDLERS
#define HANDLERS

// Function prototypes
void trap_kernel(ExceptionInfo *info);

void trap_clock(ExceptionInfo *info);

void trap_illegal(ExceptionInfo *info);

void trap_memory(ExceptionInfo *info);

void trap_math(ExceptionInfo *info);

void trap_tty_receive(ExceptionInfo *info);

void trap_tty_transmit(ExceptionInfo *info);

#endif