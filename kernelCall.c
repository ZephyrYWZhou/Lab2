#include "kernelHeader.h"

int kernel_Fork(void) {
    int child_pid; // Store the child process ID
    unsigned long i; // Loop variable

    ProcessControlBlock *temp; // Temporary pointer to ProcessControlBlock
    ProcessControlBlock *child_process; // Pointer to the child process

    // Allocate memory for the child process and its context
    child_process = (ProcessControlBlock*)malloc(sizeof(ProcessControlBlock));
    child_process->ctx = (SavedContext*)malloc(sizeof(SavedContext));
    allocate_page_table(child_process); // Allocate page table for the child process

    // Check if there is enough physical memory for Region 0
    if (used_pgn_r0() > num_free_frame) {
        // Print error message if there's not enough memory
        TracePrintf(0, "kernel_fork ERROR: not enough phys mem for creating Region 0.\n");
        // Free allocated memory for the child process and its context
        free(child_process->ctx);
        free(child_process->pt_r0);
        free(child_process);
        return ERROR; // Return error code
    }

    // Initialize the child process PCB (Process Control Block)
    child_process->pid = next_pid++;
    child_process->parent = current_process;
    child_process->next = NULL;
    child_process->brk = current_process->brk;
    child_process->n_child = 0;
    child_process->delay_clock = 0;
    child_process->statusQ = NULL;

    child_pid = child_process->pid; // Store the child process ID
    current_process->n_child++; // Increment the number of children of the current process
    temp = current_process; // Save the current process pointer in temp

    // Perform a context switch to switch to the child process
    ContextSwitch(fork_save_flush, temp->ctx, temp, child_process);

    // Check if the current process is the parent or the child
    if (current_process->pid == temp->pid) {
        return child_pid; // Return the child process ID if the current process is the parent
    } else {
        return 0; // Otherwise, return 0
    }
}


int kernel_Exec(char *filename, char **argvec, ExceptionInfo *info) {
    int status; // Store the status of the program loading process

    // Check the filename
    if (filename == NULL) {
        return ERROR;
    }

    status = LoadProgram(filename, argvec, info);

    if (status == -1) {
        return ERROR;
    }
    if (status == -2) {
        kernel_Exit(ERROR);
    }
    return 0; // Return 0 indicating successful execution
}

void kernel_Exit(int status) {
    ProcessControlBlock *temp_process;

    // Check the pid of the current process
    if (current_process->pid==0||current_process->pid==1) {
        Halt();
    }

    delete_child();

    if (current_process->parent == NULL) {
        ContextSwitch(exit_save_flush, current_process->ctx, current_process, next_ready_queue());
        return;
    }

    add_status(status);

    temp_process = next_wait_queue();

    if (temp_process == NULL) {
        fflush(stdout);
        ContextSwitch(exit_save_flush, current_process->ctx, current_process, next_ready_queue());
    }
    else {
        fflush(stdout);
        ContextSwitch(exit_save_flush, current_process->ctx, current_process, temp_process);
    }
}

int kernel_Wait(int *status_ptr) {
    int return_pid;
    status_queue *temp_status;
    if (current_process->n_child == 0) {
        return ERROR;
    }

    if (current_process->statusQ == NULL) {
        ContextSwitch(wait_save_flush, current_process->ctx, current_process, next_ready_queue());
    }

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
    // Check if the address is NULL
    if (addr == NULL) {
        return ERROR; 
    }
    // Check if the new address exceeds the user stack bottom
    if ((unsigned long)addr + PAGESIZE > user_stack_bot()) {
        return ERROR; 
    }

    unsigned long i, pn_addr, pn_brk;
    pn_addr = UP_TO_PAGE((unsigned long)addr) >> PAGESHIFT; // Calculate the page number of the new address
    pn_brk = UP_TO_PAGE((unsigned long)current_process->brk) >> PAGESHIFT; // Calculate the page number of the current brk

    // Check if the new address is greater than or equal to the current brk
    if (pn_addr >= pn_brk) {
        // If the difference between the new address and current brk exceeds the free frame count, return error
        if (pn_addr - pn_brk > num_free_frame) {
            return ERROR;
        }
        // Iterate from current brk to the new address to update page table entries
        for (i = MEM_INVALID_PAGES; i < pn_addr; i++) {
            if (current_process->pt_r0[i].valid == 0) {
                // If the page is not valid, allocate a new page and update page table entries
                current_process->pt_r0[i].valid = 1;
                current_process->pt_r0[i].uprot = PROT_READ | PROT_WRITE;
                current_process->pt_r0[i].kprot = PROT_READ | PROT_WRITE;
                current_process->pt_r0[i].pfn = get_free_page();
            }
        }
    } else {
        // Iterate from current brk to the new address to remove unused pages
        for (i = pn_brk; i > pn_addr; i--) {
            if (current_process->pt_r0[i].valid == 1) {
                // If the page is valid, free the page and update page table entries
                remove_used_page((current_process->pt_r0) + i);
                current_process->pt_r0[i].valid = 0;
            }
        }
    }
    // Update the current brk to the new address
    current_process->brk = (unsigned long)addr;
    return 0; // Return 0 indicating successful execution
}


int kernel_Delay(int clock_ticks) {
    int i;

    if (clock_ticks<0) {
        return ERROR;
    }
    current_process->delay_clock = clock_ticks;
    if (clock_ticks>0) {
        ContextSwitch(delay_save_flush, current_process->ctx, current_process, next_ready_queue());
    }

    return 0;
}

int kernel_Ttyread(int tty_id, void *buf, int len) {
    int return_len = 0;

    // Check for invalid length or NULL buffer pointer
    if (len < 0 || buf == NULL) {
        return ERROR; 
    }

    // Check if there are characters in the buffer
    if (yalnix_term[tty_id].n_buf_char == 0) {
        // If buffer is empty, add the process to the read queue and switch to the next ready process
        add_read_queue(tty_id, current_process);
        ContextSwitch(tty_save_flush, current_process->ctx, current_process, next_ready_queue());
    }

    // Check if the number of characters in the buffer is less than or equal to the requested length
    if (yalnix_term[tty_id].n_buf_char <= len) {
        // If buffer contains fewer characters than requested, read all available characters
        return_len = yalnix_term[tty_id].n_buf_char;
        memcpy(buf, yalnix_term[tty_id].read_buf, len); 
        yalnix_term[tty_id].n_buf_char = 0; 
    } else {
        // If buffer contains more characters than requested, read requested number of characters
        memcpy(buf, yalnix_term[tty_id].read_buf, len); 
        yalnix_term[tty_id].n_buf_char -= len;
        memcpy(yalnix_term[tty_id].read_buf, (yalnix_term[tty_id].read_buf) + len, yalnix_term[tty_id].n_buf_char); // Shift remaining characters in buffer

        return_len = len; 

        // If there are processes waiting in the read queue, switch to the next process in the queue
        if (yalnix_term[tty_id].read_queue_head != NULL) {
            ContextSwitch(switch_save_flush, current_process->ctx, current_process, next_read_queue(tty_id));
        }
    }
    return return_len; // Return the number of characters read
}


int kernel_Ttywrite(int tty_id, void *buf, int len) {
    // Check for invalid buffer pointer, negative length, or length exceeding TERMINAL_MAX_LINE
    if (buf == NULL || len < 0 || len > TERMINAL_MAX_LINE) {
        return ERROR; 
    }

    // Check if another process is currently writing to the terminal
    if (yalnix_term[tty_id].writingProc != NULL) {
        // If another process is writing, add the current process to the write queue and switch to the next ready process
        add_write_queue(tty_id, current_process);
        ContextSwitch(tty_save_flush, current_process->ctx, current_process, next_ready_queue());
    }

    // Store the buffer and transmit its contents to the terminal
    yalnix_term[tty_id].write_buf = buf;
    TtyTransmit(tty_id, yalnix_term[tty_id].write_buf, len);

    // Mark the current process as the writing process
    yalnix_term[tty_id].writingProc = current_process;
    ContextSwitch(tty_save_flush, current_process->ctx, current_process, next_ready_queue());
    yalnix_term[tty_id].writingProc = NULL; 

    // If there are processes waiting in the write queue, switch to the next process in the queue
    if (yalnix_term[tty_id].write_queue_head != NULL) {
        ContextSwitch(switch_save_flush, current_process->ctx, current_process, next_write_queue(tty_id));
    }

    return len; 
}
