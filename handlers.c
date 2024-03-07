#include "handlers.h"
#include <stdio.h>

/*----------Trap Handlers---------*/
void trap_kernel(ExceptionInfo *info) {
    TracePrintf(0,"Entering trap kernel\n");
    /* get which kind of kernel call we have */
    int call_type = info -> code;

    if (call_type == YALNIX_FORK) {
        TracePrintf(0, "Fork()\n");
        info->regs[0] = Fork();

        return;
    }
    else if (call_type == YALNIX_EXEC) {
        TracePrintf(0, "Exec()\n");
        info->regs[0] = ExecFunc((char *)(info->regs[1]), (char **)(info->regs[2]), info);

        return;
    }
    else if (call_type == YALNIX_EXIT) {
        TracePrintf(0, "Exit()\n");
        Exit((int)info->regs[1]);

        return;
    }
    else if (call_type == YALNIX_WAIT) {
        TracePrintf(0, "Wait()\n");
        info->regs[0] = Wait(info->regs[1]);

        return;
    }
    else if (call_type == YALNIX_GETPID) {
        TracePrintf(0, "GetPid()\n");
        info->regs[0] = GetPid();
        return;
    }
    else if (call_type == YALNIX_BRK) {
        TracePrintf(0, "Brk()\n");
        info->regs[0] = Brk((void *)info->regs[1]);

        return;
    }
    else if (call_type == YALNIX_DELAY) {
        TracePrintf(0, "Delay()\n");
        info->regs[0] = Delay((int)info->regs[1]);

    }
    else if (call_type == YALNIX_TTY_READ) {
        TracePrintf(0, "TTY_READ()\n");
        info->regs[0] = TtyRead(info->regs[1], info->regs[2], info->regs[3]);

        return; 
    }
    else if (call_type == YALNIX_TTY_WRITE) {
        TracePrintf(0, "TTY_WRITE()\n");
        info->regs[0] = TtyWrite((int)(info->regs[1]), (void *)(info->regs[2]), (int)(info->regs[3]));

        return;
    }
    else {
        TracePrintf(0, "Code not found: Just break\n");
        return;
    }     
}

void trap_clock(ExceptionInfo *info) {
	TracePrintf(0, "trap_clock runs\n");
	total_time++;
	TracePrintf(0, "Time is: %d\n", total_time);

	// Function1: move process from delay queue to ready queue
	//if processes reach time to switch, and delay queue now empty, move from delay Q to ready Q
	if (delay_q_head != NULL) {
		TracePrintf(0, "delay_q_head is %d\n", delay_q_head->pid);
		TracePrintf(0, "123\n");
		TracePrintf(0, "delay_q_head ST is %d\n", delay_q_head->switchTime);
	}
	
    if (delay_q_head != NULL && delay_q_head->switchTime <= total_time) {
    	pcb *temp = linkedList_remove(0);
    	TracePrintf(0, "delay_q_head has pid %d\n", temp->pid);
        linkedList_add(1, temp);
		TracePrintf(0, "ready_q_head has pid %d", ready_q_head->pid);    }

    // Function2: contextSwitch process on ready queue
	if (current_pcb == idle_pcb || current_pcb->switchTime == total_time) {
		if (ready_q_head != NULL) {
			if (current_pcb == idle_pcb) {
				ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)linkedList_remove(1));
			} 
			else {
			    linkedList_add(1, current_pcb);
			    ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)linkedList_remove(1));				
			}

		} else {
			if (current_pcb != idle_pcb) {
				ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void*) current_pcb, (void*) idle_pcb);
			}
		}
	}   
}

void trap_illegal(ExceptionInfo *info) {
	/* get the type for illegal */
	int illegal_type = info->code;
	char *text;

	if (illegal_type == TRAP_ILLEGAL_ILLOPC) {
		fprintf(stderr, "Illegal opcode, PID is%d\n", GetPid());
	}

	if (illegal_type == TRAP_ILLEGAL_ILLOPN) {
		fprintf(stderr, "Illegal operand, PID is%d\n", GetPid());
	}

	if (illegal_type == TRAP_ILLEGAL_ILLADR) {
		fprintf(stderr, "Illegal addressing mode, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_ILLTRP) {
		fprintf(stderr,  "Illegal software trap, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_PRVOPC) {
		fprintf(stderr, "Privileged opcode, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_PRVREG) {
		fprintf(stderr, "Privileged register, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_COPROC) {
		fprintf(stderr, "Coprocessor error, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_BADSTK) {
		fprintf(stderr, "Bad stack, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_KERNELI) {
		fprintf(stderr, "Linux kernel sent SIGILL, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_USERIB) {
		fprintf(stderr, "Received SIGILL or SIGBUS from user, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_ADRALN) {
		fprintf(stderr, "Invalid address alignment, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_ADRERR) {
		fprintf(stderr, "Non-existent physical address, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_OBJERR) {
		fprintf(stderr, "Object-specific HW error, PID is%d\n", GetPid());
	}
	if (illegal_type == TRAP_ILLEGAL_KERNELB) {
		fprintf(stderr, "Linux kernel sent SIGBUS, PID is%d\n", GetPid());
	}
	TracePrintf(0,"Entered TRAP \n");
	Exit(ERROR);
}

void trap_memory(ExceptionInfo *info) {
	TracePrintf(0,"Entered TRAP MEMORY\n");
	TracePrintf(0, "Address for TRAP MEMORY is %p\n", info->addr);
	TracePrintf(0, "Page is %d\n", (int)UP_TO_PAGE(info->addr)>>PAGESHIFT);
	TracePrintf(0, "Code for TRAP MEMORY is %p\n", info->code);
	int memory_trap_type = info->code;
	TracePrintf(0,"BEFORE\n");
	void *this_addr = info->addr;
	int this_vpn = (long)DOWN_TO_PAGE(this_addr) >> PAGESHIFT;
	int brk_pos_vpn = current_pcb->brk_pos;
	TracePrintf(0,"AFTER\n");

	//If current pcb's user stack's lowest used position is valid, we grow the user stack down
	if (this_vpn < current_pcb->user_stack_low_vpn && this_vpn > (brk_pos_vpn + 1)) {
		TracePrintf(0,"Entered IF: we grow \n");
		int start = current_pcb->user_stack_low_vpn;
		TracePrintf(0, "First addr of user stack is %d\n", start);
		int end = (long)DOWN_TO_PAGE((long)this_addr) >> PAGESHIFT;
		int i;
		for (i = start - 1; i >= end; i--) {
			pt0[i].valid = 1;
			pt0[i].kprot = (PROT_READ | PROT_WRITE);
			pt0[i].uprot = (PROT_READ | PROT_WRITE);
			pt0[i].pfn = allocate_new_pfn();
		}
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		current_pcb->user_stack_low_vpn = this_vpn;
		
	} 

	else {
		// Print error msg
		if (memory_trap_type == TRAP_MEMORY_MAPERR) {
			fprintf(stderr, "TRAP_MEMORY_MAPERR: No mapping at addr %p\n", (int) info->addr);
		}
		else if (memory_trap_type == TRAP_MEMORY_ACCERR) {
			fprintf(stderr, "TRAP_MEMORY_ACCERR: Protection violation at addr %p\n", (int) info->addr);
		} 
		else if (memory_trap_type == TRAP_MEMORY_KERNEL) {
			fprintf(stderr, "TRAP_MEMORY_KERNEL: Linux kernel sent SIGSEGV at addr %p\n", (int) info->addr);	
		}
		else if (memory_trap_type == TRAP_MEMORY_USER) {
			fprintf(stderr, "TRAP_MEMORY_USER: Received SIGSEGV from user.\n");			
		}
		else {
			fprintf(stderr, "ERROR in trap_memory\n");
		}

		//terminate process
		Exit(ERROR);
		TracePrintf(0,"Exit ERROR\n");		
		Halt();
	}
}

void trap_math(ExceptionInfo *info) {
	TracePrintf(0, "Entering TRAP MATH\n");
	char* text;

	/* get the type of math trap */
	int trap_math_type = info->code;

	if (trap_math_type == TRAP_MATH_INTDIV) {
		text = "integer divide by zero";		
	}
	if (trap_math_type == TRAP_MATH_INTOVF) {
		text = "Integer overflow";		
	}
	if (trap_math_type == TRAP_MATH_FLTDIV) {	
		text = "Floating divide by zero";	
	}
	if (trap_math_type == TRAP_MATH_FLTOVF) {
		text = "Floating overflow";		
	}
	if (trap_math_type == TRAP_MATH_FLTUND) {
		text = "Floating underflow";		
	}
	if (trap_math_type == TRAP_MATH_FLTRES) {
		text = "Floating inexact result";	
	}
	if (trap_math_type == TRAP_MATH_FLTINV) {
		text = "Invalid floating operation";
	}
	if (trap_math_type == TRAP_MATH_FLTSUB) {
		text = "FP subscript out of range";		
	}
	if (trap_math_type == TRAP_MATH_KERNEL) {
		text = "Linux kernel sent SIGFPE";		
	}
	if (trap_math_type == TRAP_MATH_USER) {
		text = "Received SIGFPE from user";		
	}
	fprintf(stderr, "TRAP_MATH: %s\n", text);
    Exit(ERROR);
}

void trap_tty_receive(ExceptionInfo *info) {
	TracePrintf(1, "HERE11 \n");
	//when the user types a line, we enter this trap
	int term_num = info->code;
	Terminal *current_terminal = terminal[term_num];
	Line *line_list_head;
	int BlockQ_number = term_num+3;

	TracePrintf(1, "HERE22 \n");

	int addition_char;
	//copy to terminal 0 Read Buffer
	char *buf = (char*) Malloc(TERMINAL_MAX_LINE * sizeof(char));
	//buf = current_terminal->BufRead + current_terminal->num_char;
	addition_char = TtyReceive(term_num, buf, TERMINAL_MAX_LINE);
    //update number of chars can be read
    current_terminal->num_char += addition_char;
    // build a new line struct for the newly read line
    Line *new_line = Malloc(sizeof(Line));
    new_line->ReadBuf = buf;
    new_line->length = addition_char;
    new_line->nextLine = NULL;

    if (new_line == NULL) {
    	TracePrintf(0, "NEW LINE IS NULLLLLLL\n");
    }

    //add to line linked list 
    lineList_add(term_num,new_line);

    Line *first = (terminal[term_num]->readterm_lineList_head);

    if (terminal[term_num]->readterm_lineList_head == NULL) {
    	TracePrintf(0, "First IS NULLLLLLL\n");
    }    

    TracePrintf(1, "HERE33 \n");

    if (current_pcb == idle_pcb && addition_char > 0 && whichHead(info->code) != NULL) {
    	TracePrintf(1, "current_pcb == idle_pcb && addition_char > 0 \n");

    	pcb* BlockQ_head = whichHead(info->code);

    	pcb *next_on_blockq = BlockQ_head;
    	TracePrintf(0, "Head of terminal has pid %d\n", next_on_blockq->pid);
    	BlockQ_head = next_on_blockq->nextProc;
    	ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void*)next_on_blockq);
    } 
    if (ready_q_head != NULL) {
    	TracePrintf(1, "ready_q_head != NULL \n");
    	ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)linkedList_remove(ReadyQueue));
    }    
    TracePrintf(1, "[trap_tty_receive] DONE \n");
}

void trap_tty_transmit(ExceptionInfo *info) {
	TracePrintf(0, "[trap_tty_transmit], starts, pid %d\n", current_pcb->pid);
	int term_num = info->code;
	pcb *block_head = whichWrite_Head(term_num);
	terminal[term_num]->term_writing = 0;
	// add blcok queue head to readyQueue
	if(block_head != NULL) {
		TracePrintf(0, "trap_tty_transmit, add blcok queue head to readyQueue, blcok queue pid is %d\n", block_head->pid);
		linkedList_add(ReadyQueue,linkedList_remove(term_num+8));
	}
	TracePrintf(0, "[trap_tty_transmit] DONE \n");
}
