#include "kernelcall.h"

int Fork(void) {
	TracePrintf(0, "<Fork> for pid: %d\n", current_pcb->pid);
	// Step1: new page Table, return a new pfn for this pt0
	int *new_pt0 = allocate_new_pfn();

	// Step2: Create a new process
	int new_pid = process_id++;

	int stack_counter;
	pcb *new_processPCB = make_pcb(new_pt0, new_pid);
	new_processPCB->brk_pos = current_pcb->brk_pos;

	pcb *pcb1 = new_processPCB;
	// Step3: if child return 0
	if (current_pcb == new_processPCB) {
		TracePrintf(0, "<Fork> child return, pid is: %d\n", current_pcb->pid);
		return 0;
	} else {
		//Step4: If parent, Copy pages and ContextSwitch

		pcb *pcb1 = new_processPCB;
		int last_page = PAGE_TABLE_LEN - KERNEL_STACK_PAGES;
		for (stack_counter = MEM_INVALID_PAGES; stack_counter < last_page; stack_counter++) {
			if (pt0[stack_counter].valid == 1) {
				int empty_pfn = allocate_new_pfn();
				int empty_addr = empty_pfn * PAGESIZE;	
				// map a free vpn to emtpy_pfn

				int free_vpn = current_pcb->brk_pos;
				
				TracePrintf(9, "Free vpn is %d\n", free_vpn);

				pt0[free_vpn].pfn = empty_pfn;
				
				((struct pte*) pt0)[free_vpn].uprot = (PROT_READ | PROT_EXEC);
				((struct pte*) pt0)[free_vpn].kprot = (PROT_READ | PROT_WRITE);
				((struct pte*) pt0)[free_vpn].valid = 1;
				WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

				// empty virtual addrr at vpn * PAGESIZE, copy this page to empty_pfn
				memcpy((void *) (free_vpn * PAGESIZE), (void *) (stack_counter * PAGESIZE), PAGESIZE);


				// make free_vpn point to pcb1's physical pt0
				((struct pte*) pt0)[free_vpn].pfn = pcb1 -> pfn_pt0;

				// Now we can modify new pt0 by accessing free_vpn

				WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
				// finally modify the kernel stack entry in pcb1's pt0
			    ((struct pte*) (free_vpn * PAGESIZE))[stack_counter].pfn = empty_pfn;
			    ((struct pte*) (free_vpn * PAGESIZE))[stack_counter].uprot = pt0[stack_counter].uprot;
			    ((struct pte*) (free_vpn * PAGESIZE))[stack_counter].kprot = pt0[stack_counter].kprot;
			    ((struct pte*) (free_vpn * PAGESIZE))[stack_counter].valid = pt0[stack_counter].valid;

				WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
				// zero out this pt0 entry in the current process to be consistent
				((struct pte*) pt0)[free_vpn].valid = 0;
				WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		
			}
			
		}
		TracePrintf(9, "Fork case done\n");
		// add the child process to parent
        pcb *child_iter = current_pcb -> child;
        if (child_iter == NULL) current_pcb -> child = new_processPCB;
        else {
        	while (child_iter -> next_child != NULL) child_iter = child_iter -> next_child;
        	child_iter -> next_child = new_processPCB;
        }

		linkedList_add(2,new_processPCB);

		// enqueue two process to ready Queue
		linkedList_add(1,new_processPCB);
		linkedList_add(1,current_pcb);

		//ContextSwitch to enable new process
		ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)linkedList_remove(1));

		TracePrintf(0, "Returning from Fork as parent, new process PID is %d\n", new_processPCB->pid);

		TracePrintf(0, "<Fork> DONE for pid: %d\n", current_pcb->pid);
		TracePrintf(0, "[Fork] DONE\n");
        return new_processPCB->pid;

	}
}

int ExecFunc(char *filename, char **argvec, ExceptionInfo *info) {

	/* Error checking if argvec has valid pointers */
	int j = 0;
	int flag = 0;
	int page_iter = (int)(((long) argvec) >> PAGESHIFT);
	while (flag == 0) {
		if ((!pt0[page_iter].kprot & PROT_READ) || !pt0[page_iter].valid) {
			Exit(ERROR);
		}
		while (j * sizeof(char *) < ((page_iter + 1) << PAGESHIFT) - (long)argvec) {
			if (argvec[j] == NULL) {
				flag = 1;
				break;
			}
			j++;
		}
		page_iter++;
	}
	/* End of Error Checking */

	TracePrintf(0, "<Exec> \n", filename);
	int load_success;
	load_success = LoadProgram(filename, argvec, info);

	if (load_success != 0) {
		TracePrintf(0, "<Exec>: Load program failed\n");
		return ERROR;
	}
	TracePrintf(0, "[ExecFunc] DONE\n");
	return 0;
}

void Exit(int status) {
	TracePrintf(0, "    [EXIT] pid %d\n", current_pcb->pid);
	current_pcb -> status = TERMINATED;
	
	/* TERMINATE PROCESS */
	// if this process has children, make them orphan process
	if (current_pcb -> child != NULL) {
		struct pcb *child_pcb = current_pcb -> child;
		while (child_pcb != NULL) {
			child_pcb -> parent = NULL;
			child_pcb = child_pcb->next_child;
		}
	}

	// if this process has a parent
	if (current_pcb -> parent != NULL) {
		// add this child process to its parent's ExitChild queue
		ecb *exited_childq_block =  Malloc(sizeof(ecb));
		exited_childq_block -> pid = current_pcb -> pid;
		exited_childq_block -> status = status;
		enqueue_ecb(current_pcb -> parent, exited_childq_block);

		// adjust parent's Children queue
		update_children_q(current_pcb -> parent, current_pcb);

		// case where if the parent was waiting, change status, and put to ready queue
		if ((current_pcb -> parent) -> status == WAITING) {
			(current_pcb -> parent) -> status = READY;
			linkedList_add(ReadyQueue, current_pcb -> parent);
		}
	}
	/* END */

	ContextSwitch(MySwitchFunc, &(current_pcb -> ctx), (void *) current_pcb, linkedList_remove(ReadyQueue));

	while(1) {} // Exit should never return
}

int Wait(int *status_ptr) {
	TracePrintf(0, "[WAIT] current process has pid: %d\n", current_pcb->pid);
	if (current_pcb -> exited_childq_head == NULL) {
		if (current_pcb -> child == NULL) {
			fprintf(stderr, "   [WAIT_ERROR]: no more children of current process.\n");
			return ERROR;
		}
		current_pcb -> status = WAITING;
		ContextSwitch(MySwitchFunc, &current_pcb -> ctx, (void *) current_pcb, linkedList_remove(ReadyQueue));
	}
	TracePrintf(0, "[WAIT] dubugging. Exited child head: %d\n", current_pcb -> exited_childq_head == NULL);

	// write to the status pointer 
	*status_ptr = (current_pcb -> exited_childq_head -> status);

	// kick this newly exited child out of parent's queue
	ecb *temp = current_pcb -> exited_childq_head;
	int return_id = temp -> pid;
	current_pcb -> exited_childq_head = temp -> next;
	if (current_pcb -> exited_childq_head == NULL) {
		current_pcb -> exited_childq_tail = NULL;
	}
	free(temp);
	TracePrintf(0, "[WAIT] returned child pid %d\n", return_id);


	// return process ID of the head exited child
	return return_id;
}


int GetPid(void) {
	return current_pcb -> pid;
}

extern int Brk(void *addr) {
	TracePrintf(0, "pid %d\n set <Brk>", current_pcb->pid);
	int set_brk = UP_TO_PAGE((int)addr) >> PAGESHIFT;
	void *current_pcb_brk_addr = (current_pcb->brk_pos) << PAGESHIFT;

	if (current_pcb->brk_pos > current_pcb->user_stack_low_vpn - 1) {
		TracePrintf(0, "current process brk addr is bigger than user_stack_low_addr - 1 page");
		return ERROR;
	}

	// case1: set_brk bigger than brk_pos, move up 
	if (set_brk > current_pcb->brk_pos && set_brk <= (current_pcb->user_stack_low_vpn-1)) {
		TracePrintf(0, "Brk Case1\n ");
		int current_pcb_vpn = current_pcb->brk_pos;
		int num_pages_up = (set_brk - current_pcb_vpn);
		int i;
		for (i = 0; i < num_pages_up; i++) {
			//enqueue a page
			pt0[current_pcb->brk_pos + i].pfn = allocate_new_pfn();
			pt0[current_pcb->brk_pos + i].valid = 1;
			pt0[current_pcb->brk_pos + i].uprot = (PROT_READ | PROT_WRITE);
			pt0[current_pcb->brk_pos + i].kprot = (PROT_READ | PROT_WRITE);
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

		}
		current_pcb->brk_pos = set_brk;
	} 
	// case2: set_brk smaller than brk_pos, move down 
	else if (set_brk < current_pcb->brk_pos && set_brk >= MEM_INVALID_PAGES) {
		TracePrintf(0, "Brk Case2\n ");
		//int current_pcb_vpn = current_pcb->brk_pos >> PAGESHIFT;
		int num_pages_down = (current_pcb->brk_pos - set_brk);
		int i;
		for (i = 0; i < num_pages_down; i++) {
			//dequeue a page
			deallocate_new_pfn(pt0[current_pcb->brk_pos+i].pfn);
			pt0[current_pcb->brk_pos+i].valid = 0;
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		}		
		current_pcb->brk_pos = set_brk;
	}
	else {
		TracePrintf(0, "Brk ERROR\n ");
		return ERROR;
	}
	TracePrintf(0, "<Brk> end: pid %d\n", current_pcb->pid);
	TracePrintf(0, "[Brk] DONE \n");
}

extern int Delay(int clock_ticks) {
	TracePrintf(0, "(Delay) Total time is %d\n", total_time);

    TracePrintf(0, "<Delay> pid %d\n", current_pcb->pid);
    if (clock_ticks < 0) {
    	return ERROR;
    }

    if (clock_ticks == 0) {
    	return 0;
    }
    if (ready_q_head == NULL) {
    	current_pcb->switchTime = total_time + clock_ticks;
    	linkedList_add(0, current_pcb);
    	TracePrintf(0, "First pcb in delay q has pid %d\n", delay_q_head->pid);
    	ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)idle_pcb);
    }

    else {
	    current_pcb->switchTime = total_time + clock_ticks;
	    
	    linkedList_add(0, current_pcb);

	    ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)linkedList_remove(1));    	
    }
    TracePrintf(0, "<Delay> pid end: %d\n", current_pcb->pid);
    TracePrintf(0, "[Delay] DONE \n");
    return 0;
}

extern int TtyRead(int tty_id, void *buf, int len) {
	TracePrintf(0, "Len is %d\n", len);
	/* Check Buffer bits and validity */
	int page_iter;
	int prot_bit = PROT_WRITE;
    for (page_iter = (int)(((long)buf)>>PAGESHIFT); page_iter < (int)(UP_TO_PAGE((long)buf + len)>>PAGESHIFT); page_iter++) {
        if (!(pt0[page_iter].kprot & prot_bit) || !pt0[page_iter].valid) {
        	fprintf(stderr, "   [TtyeRead ERROR]: invalid buffer.\n");
        	Exit(ERROR);
        }
    }
	/* End of checking buffer */

	if (buf == NULL || len < 0) {
		return ERROR;
	}
	TracePrintf(1, "HERE1 \n");
	int BlockQ_number;
	BlockQ_number = tty_id+3;
	int result;

    // If nothing to read, add current pcb to block queue
    if (terminal[tty_id]->num_char == 0) {
    	TracePrintf(1, "nothing to read, add current pcb to block queue \n");
    	linkedList_add(BlockQ_number, current_pcb);
    	pcb* BlockQ_head = whichHead(tty_id);
    	TracePrintf(0, "After adding, head of block Q has pid %d\n", BlockQ_head->pid);
    	if (ready_q_head != NULL) {
    		ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)linkedList_remove(ReadyQueue));    		
    	} 
    	else {
 			ContextSwitch(MySwitchFunc, &current_pcb->ctx, (void *)current_pcb, (void *)idle_pcb);   		
    	}
    	
    }
    TracePrintf(1, "HERE2 \n");
	int char_left_to_read;
	if (terminal[tty_id]->readterm_lineList_head != NULL) {
		TracePrintf(1, "Enter while loop \n");
		Line *current_line_is = terminal[tty_id]->readterm_lineList_head;

		TracePrintf(0,"The current line length is %d\n", current_line_is->length);
		char_left_to_read = current_line_is->length;

	    //copy to buf when len want to read is less than the chars in the line's Read buffer
	    if (len <= current_line_is->length) {
	    	TracePrintf(1, "current line has MORE characters than we can read \n");
	    	memcpy(buf, current_line_is->ReadBuf, len);
	    	memcpy(current_line_is->ReadBuf, current_line_is->ReadBuf + len, (current_line_is->length)-len);
	    	(current_line_is->length) -= len;
	    	terminal[tty_id]->num_char -=len;

	    	// if we read the whole line, update the line linked list
	    	if (current_line_is->length <= 0) {
	    		TracePrintf(1, "read the whole line, update the line linked list \n");
	    		terminal[tty_id]->readterm_lineList_head = current_line_is->nextLine;
	    	}
	    	result = len;
	    } 
	    // if we want to read more than what's inside the one line's read buf, look at the next line
	    else {
	    	TracePrintf(1, "current line has LESS characters than we can read \n");
	    	int current_line_length;
	    	current_line_length = current_line_is->length;
    		memcpy(buf, current_line_is->ReadBuf, current_line_length);
    		result = current_line_is->length;
    		(current_line_is->length) = 0;
    		terminal[tty_id]->num_char -= current_line_length;
    		char_left_to_read = 0;
    		

    		//go to the next line
    		if (terminal[tty_id]->readterm_lineList_head == terminal[tty_id]->readterm_lineList_tail) {
    			terminal[tty_id]->readterm_lineList_head = NULL;
    			terminal[tty_id]->readterm_lineList_tail = NULL;
    		} else {
    			Line *temp = terminal[tty_id]->readterm_lineList_head;
    			terminal[tty_id]->readterm_lineList_head = terminal[tty_id]->readterm_lineList_head->nextLine;
    			temp->nextLine = NULL;
    		}
    		if (terminal[tty_id]->readterm_lineList_head != NULL)
    			TracePrintf(0, "Finished reading a line, next line has length %d\n", terminal[tty_id]->readterm_lineList_head->length);
	    }
	    
	}
	linkedList_remove(BlockQ_number);
	return result;
}

int TtyWrite(int tty_id, void *buf, int len) {
	TracePrintf(0, "[TtyWrite] Starts \n");
	/* Check Buffer bits and validity */
	int page_iter;
	int prot_bit = PROT_READ;
    for (page_iter = (int)(((long)buf)>>PAGESHIFT); page_iter < (int)(UP_TO_PAGE((long)buf + len)>>PAGESHIFT); page_iter++) {
        if (!(pt0[page_iter].kprot & prot_bit) || !pt0[page_iter].valid) {
        	fprintf(stderr, "   [TtyeWrite ERROR]: invalid buffer.\n");
        	Exit(ERROR);
        }
    }
	/* End of checking buffer */

	pcb *block_head = whichWrite_Head(tty_id);
	if (len < 0) {
		return ERROR;
	}
	if (len == 0) {
		return 0;
	}
	TracePrintf(0, "[TtyWrite] HERE1 \n");
	// if there the terminal is writing, add current pcb to block queue
	TracePrintf(0, "terminal[tty_id]->term_writing is  %d\n", terminal[tty_id]->term_writing);
    if (terminal[tty_id]->term_writing == 1) {
    	TracePrintf(0, "if there the terminal is writing \n");
    	// add to block queue
    	linkedList_add(tty_id+8, current_pcb);
    	if (ready_q_head != NULL) {
    		TracePrintf(0, "[TtyWrite] ContextSwitch1 \n");
    		ContextSwitch(MySwitchFunc, &(current_pcb -> ctx), (void *) current_pcb, linkedList_remove(ReadyQueue));
    	} else {
    		TracePrintf(0, "[TtyWrite] ContextSwitch2 \n");
    		ContextSwitch(MySwitchFunc, &(current_pcb -> ctx), (void *) current_pcb, idle_pcb);
    	}
    	
    }
    TracePrintf(0, "[TtyWrite] HERE2 \n");
    // otherwise ttyTransmit
    terminal[tty_id]->term_writing = 1;
    char *bufTemp = (char*) Malloc(TERMINAL_MAX_LINE * sizeof(char));
    memcpy(bufTemp, buf, len);
    TracePrintf(0, "[TtyWrite] Before TtyTransmit \n");
    TtyTransmit(tty_id, bufTemp, len);
    TracePrintf(0, "[TtyWrite] After TtyTransmit \n");
	TracePrintf(0, "[TtyWrite] DONE \n");    
    return len;
}

//helper functions
void enqueue_ecb(pcb *parent, ecb *exit_child) {
	if (parent -> exited_childq_head == NULL) {
		parent -> exited_childq_head = exit_child;
		parent -> exited_childq_tail = exit_child;
	} else {
		(parent -> exited_childq_tail) -> next = exit_child;
		parent -> exited_childq_tail = exit_child;
	}
}

void update_children_q(pcb *parent, pcb *exit_child) {
	pcb *child = parent -> child;
	if (child -> pid == exit_child -> pid) {
		parent -> child = (parent -> child) -> next_child;
		return;
	}
	while ((child -> next_child) -> pid != exit_child -> pid) {
		child = child -> next_child;
	}
	child -> next_child = exit_child -> next_child;
}
