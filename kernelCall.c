#include "kernelHeader.h"

int kernel_Fork(void) {
    int child_pid;
    unsigned long i;
    ProcessControlBlock *temp;
    ProcessControlBlock *child_process;

    child_process = (ProcessControlBlock*)malloc(sizeof(ProcessControlBlock));
    child_process->ctx = (SavedContext*)malloc(sizeof(SavedContext));
    allocate_pt(child_process);

/************************************************************/
/* check mem */

    if (used_pgn_r0() > free_frame_cnt) {
        TracePrintf(0,"kernel_fork ERROR: not enough phys mem for creat Region0.\n");
        free(child_process->ctx);
        free(child_process->pt_r0);
        free(child_process);
        return ERROR;
    }

/************************************************************/
/* initialize the child pcb */

    child_process->pid = next_pid++;
    child_process->parent = current_process;
    child_process->next = NULL;
    child_process->brk = current_process->brk;
    child_process->n_child = 0;
    child_process->delay_clock = 0;
    child_process->statusQ = NULL;

    child_pid=child_process->pid;
    current_process->n_child++;
    temp = current_process;


    ContextSwitch(fork_sf,temp->ctx,temp,child_process);

    if (current_process->pid == temp->pid) {
        return child_pid;
    }
    else{
        return 0;
    }
}

int kernel_Exec(char *filename, char **argvec, ExceptionInfo *info) {
    int status;
    if (filename == NULL) {
        return ERROR;
    }
    // TracePrintf(0, "kernel_exec filename: %s\n", filename);

    status = LoadProgram(filename, argvec, info);

    if (status == -1) {
        return ERROR;
    }
    if (status == -2) {
        kernel_Exit(ERROR);
    }
    return 0;
}

void kernel_Exit(int status) {
    ProcessControlBlock *temp_process;

    if (current_process->pid==0||current_process->pid==1) {
        Halt();
    }

    /*make al the children orphans */
    delete_child();

    if (current_process->parent == NULL) {
        ContextSwitch(exit_sf, current_process->ctx, current_process, next_ready_queue());
        return;
    }

    /* add status to the Q of parent */
    add_status(status);

    temp_process = next_wait_queue();

    if (temp_process == NULL) {
        fflush(stdout);
        ContextSwitch(exit_sf, current_process->ctx, current_process, next_ready_queue());
    }
    else {
        fflush(stdout);
        ContextSwitch(exit_sf, current_process->ctx, current_process, temp_process);
    }
}

int kernel_Wait(int *status_ptr) {
    int return_pid;
    StatusQueue *temp_status;
    if (current_process->n_child == 0) {
        return ERROR;
    }

    if (current_process->statusQ == NULL) {
        ContextSwitch(wait_sf, current_process->ctx, current_process, next_ready_queue());
    }

/************************************************************/
/* free the status in FIFO */

    return_pid = current_process->statusQ->pid;

    *(status_ptr) = current_process->statusQ->status;
    temp_status = current_process->statusQ;
    current_process->statusQ = current_process->statusQ->next;

    free(temp_status);
    return return_pid;
}

int kernel_Getpid(void) {
    return current_process->pid;
}

int kernel_Brk(void *addr) {
    if (addr == NULL) {
        return ERROR;
    }
    if ((unsigned long)addr + PAGESIZE > user_stack_bot()) {
        return ERROR;
    }

    unsigned long i, pn_addr, pn_brk;
    pn_addr = UP_TO_PAGE((unsigned long)addr)>>PAGESHIFT;
    pn_brk = UP_TO_PAGE((unsigned long)current_process->brk)>>PAGESHIFT;

    if (pn_addr >= pn_brk) {
        if (pn_addr-pn_brk > free_frame_cnt) {
            return ERROR;
        }
        for (i = MEM_INVALID_PAGES; i < pn_addr;i++) {
            if (current_process->pt_r0[i].valid == 0) {
                current_process->pt_r0[i].valid = 1;
                current_process->pt_r0[i].uprot = PROT_READ|PROT_WRITE;
                current_process->pt_r0[i].kprot = PROT_READ|PROT_WRITE;
                current_process->pt_r0[i].pfn = get_free_page();
            }
        }
    }
    else {
        for (i = pn_brk; i > pn_addr; i--) {
            if (current_process->pt_r0[i].valid == 1) {
                remove_used_page((current_process->pt_r0)+i);
                current_process->pt_r0[i].valid = 0;
            }
        }
    }
    current_process->brk = (unsigned long)addr;
    return 0;
}

int kernel_Delay(int clock_ticks) {
    int i;

    if (clock_ticks<0) {
        return ERROR;
    }
    current_process->delay_clock = clock_ticks;
    if (clock_ticks>0) {
        ContextSwitch(delay_sf, current_process->ctx, current_process, next_ready_queue());
    }

    return 0;
}

int kernel_Ttyread(int tty_id, void *buf, int len) {
    int return_len = 0;
    if (len < 0||buf == NULL) {
        return ERROR;
    }

    if (yalnix_term[tty_id].n_buf_char == 0) {
        add_read_queue(tty_id, current_process);
        ContextSwitch(tty_sf, current_process->ctx, current_process, next_ready_queue());
    }

/************************************************************/
/* copy chars to buf */

    if (yalnix_term[tty_id].n_buf_char <= len) {
        return_len = yalnix_term[tty_id].n_buf_char;
        memcpy(buf, yalnix_term[tty_id].read_buf,len);
        yalnix_term[tty_id].n_buf_char=0;
    }
    else {
        memcpy(buf, yalnix_term[tty_id].read_buf,len);
        yalnix_term[tty_id].n_buf_char-=len;
        memcpy(yalnix_term[tty_id].read_buf, (yalnix_term[tty_id].read_buf) + len, yalnix_term[tty_id].n_buf_char);

        return_len = len;
        if (yalnix_term[tty_id].readQ_head!=NULL) {
            ContextSwitch(switch_sf, current_process->ctx, current_process, next_read_queue(tty_id));
        }
    }
    return return_len;
}

int kernel_Ttywrite(int tty_id, void *buf, int len) {
    if (buf == NULL||len < 0||len > TERMINAL_MAX_LINE) {
        return ERROR;
    }

/************************************************************/
/* check if writing is busy */
    if (yalnix_term[tty_id].writingProc != NULL) {
        add_write_queue(tty_id, current_process);
        ContextSwitch(tty_sf, current_process->ctx, current_process, next_ready_queue());
    }

    yalnix_term[tty_id].write_buf = buf;
    TtyTransmit(tty_id,yalnix_term[tty_id].write_buf,len);

    yalnix_term[tty_id].writingProc = current_process;
    ContextSwitch(tty_sf, current_process->ctx, current_process, next_ready_queue());
    yalnix_term[tty_id].writingProc = NULL;
    if (yalnix_term[tty_id].writeQ_head != NULL) {
        ContextSwitch(switch_sf, current_process->ctx, current_process, next_write_queue(tty_id));
    }

    return len;
}