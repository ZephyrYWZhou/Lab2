#include "kernelHeader.h"

void add_ready_queue(ProcessControlBlock *p) {
    if(ready_queue_head == NULL)
        ready_queue_head = p;
    else
        ready_queue_end->next = p;
    ready_queue_end = p;
    ready_queue_end->next = NULL;
}

void add_wait_queue(ProcessControlBlock *p) {
    if (wait_queue_head == NULL) {
        wait_queue_head = p;
    }
    else {
        wait_queue_end->next = p;
    }
    wait_queue_end = p;
    wait_queue_end->next = NULL;
}

void add_delay_queue(ProcessControlBlock *p) {
    p->next = delay_queue_head->next;
    delay_queue_head->next = p;
}

void add_read_queue(int tty_id, ProcessControlBlock* p) {
    if (yalnix_term[tty_id].read_queue_head == NULL) {
        yalnix_term[tty_id].read_queue_head = p;
    }
    else {
        yalnix_term[tty_id].read_queue_end->next = p;
    }
    yalnix_term[tty_id].read_queue_end = p;
    yalnix_term[tty_id].read_queue_end->next = NULL;
}

void add_write_queue(int tty_id, ProcessControlBlock* p) {
    if (yalnix_term[tty_id].write_queue_head == NULL) {
        yalnix_term[tty_id].write_queue_head = p;
    }
    else {
        yalnix_term[tty_id].write_queue_end->next = p;
    }
    yalnix_term[tty_id].write_queue_end = p;
    yalnix_term[tty_id].write_queue_end->next = NULL;
}

unsigned long get_free_page(void) {
    if (free_frames_head->next == NULL) {
        return 0;
    } 
    phys_frame *tmp = free_frames_head->next;
    free_frames_head->next = tmp->next;
    num_free_frame--;
    unsigned long ret = tmp->phys_frame_num;
    free(tmp);
    return ret;
}

void remove_used_page(pte *p) {
    phys_frame *tmp = (phys_frame*)malloc(sizeof(phys_frame));
    tmp->phys_frame_num = p->pfn;
    tmp->next = free_frames_head->next;
    free_frames_head->next = tmp;
}

int get_new_page(pte * pt, unsigned long addr) {
    if (free_frames_head->next == NULL) {
        return 1;
    } 
    phys_frame *tmp = free_frames_head->next;
    free_frames_head->next = tmp->next;
    num_free_frame--;
    unsigned long ret = tmp->phys_frame_num;
    free(tmp);
    pt->pfn = ret;
    return 0;
}

int used_pgn_r0(void) {
    int used_pgn = 0;
    int i;
    for (i = 0; i < PAGE_TABLE_LEN; i++) {
        if (current_process->pt_r0[i].valid) {
            used_pgn++;
        }
    }
    return used_pgn;
}

void allocate_page_table(pcb* p) {
    if (half_full == 0) {
        /* set appropriate virtual start address for r0 page table */
        p->pt_r0 = (pte*)next_PT_vaddr;
        next_PT_vaddr += PAGESIZE/2;
        half_full = 1;
        /* get physical frame for r0 page table
         * set the r1 page table entry for the start address of r0 page table */
        unsigned long idx = ((unsigned long)(p->pt_r0)-VMEM_1_BASE)>>PAGESHIFT;
        if(pt_r1[idx].valid) {
            kernel_Exit(ERROR);
        }
        pt_r1[idx].pfn = get_free_page();
        pt_r1[idx].valid = 1;
        pt_r1[idx].kprot = PROT_READ|PROT_WRITE;
        pt_r1[idx].uprot = PROT_NONE;
    }
    else {
        // set appropriate virtual start address for r0 page table
        p->pt_r0 = (struct pte*)next_PT_vaddr;
        next_PT_vaddr -= PAGESIZE*3/2;

        // whole frame is used, so clear half_full_vaddr and half_full_frame
        half_full = 0;
    }
}

void delete_child(void) {
    ProcessControlBlock *tmp;
    tmp = ready_queue_head;
    while (tmp != NULL) {
        if (tmp->parent == current_process) {
            tmp->parent = NULL;
        }
        tmp = tmp->next;
    }
    tmp = wait_queue_head;
    while(tmp != NULL) {
        if(tmp->parent == current_process) {
            tmp->parent = NULL;
        }
        tmp = tmp->next;
    }
    int i;
    for (i = 0;i<NUM_TERMINALS;i++) {
        tmp = yalnix_term[i].read_queue_head;
        while(tmp != NULL) {
            if(tmp->parent == current_process) {
                tmp->parent = NULL;
            }
            tmp = tmp->next;
        }
        tmp = yalnix_term[i].write_queue_head;
        while (tmp != NULL) {
            if (tmp->parent == current_process) {
                tmp->parent = NULL;
            }
            tmp = tmp->next;
        }
    }
    tmp = delay_queue_head;
    while (tmp != NULL) {
        if (tmp->parent == current_process) {
            tmp->parent = NULL;
        }
        tmp = tmp->next;
    }
}

void add_status(int status) {
    status_queue *tmp;
    tmp = current_process->parent->statusQ;
    if (tmp == NULL) {
        tmp = (status_queue*)malloc(sizeof(status_queue));
        current_process->parent->statusQ = tmp;
        tmp->pid = current_process->pid;
        tmp->status = status;
        tmp->next = NULL;
    }
    else {
        while(tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = (status_queue*)malloc(sizeof(status_queue));
        tmp = tmp->next;
        tmp->pid = current_process->pid;
        tmp->status = status;
        tmp->next = NULL;
    }
}

unsigned long user_stack_bot(void) {
    unsigned long i;
    i = (USER_STACK_LIMIT>>PAGESHIFT)-1;
    while(current_process->pt_r0[i].valid) {
        i--;
    }
    return i<<PAGESHIFT;
}

ProcessControlBlock *next_ready_queue(void) {
    ProcessControlBlock *return_pcb;
    if (ready_queue_head == NULL) {
        return NULL;
    }

    return_pcb = ready_queue_head;
    ready_queue_head = ready_queue_head->next;
    if (ready_queue_head == NULL) {
        ready_queue_end = NULL;
        return_pcb->next = NULL;
    }
    return return_pcb;
}

ProcessControlBlock *next_read_queue(int tty_id) {
    ProcessControlBlock *return_pcb;
    if (yalnix_term[tty_id].read_queue_head == NULL) {
        return NULL;
    }
    return_pcb = yalnix_term[tty_id].read_queue_head;
    yalnix_term[tty_id].read_queue_head = yalnix_term[tty_id].read_queue_head->next;
    return return_pcb;
}

ProcessControlBlock *next_write_queue(int tty_id) {
    ProcessControlBlock *return_pcb;
    if (yalnix_term[tty_id].write_queue_head == NULL) {
        return NULL;
    }
    return_pcb = yalnix_term[tty_id].write_queue_head;
    yalnix_term[tty_id].write_queue_head = yalnix_term[tty_id].write_queue_head->next;
    return return_pcb;
}

ProcessControlBlock *next_wait_queue(void) {
    ProcessControlBlock *tmp;
    ProcessControlBlock *return_pcb;
    tmp = wait_queue_head;
    if(tmp == NULL) {
        return NULL;
    }
    if (tmp->pid == current_process->parent->pid) {
        wait_queue_head = tmp->next;
        if (wait_queue_head == NULL) wait_queue_end = NULL;
        return tmp;
    }
    while (tmp->next != NULL) {
        if (tmp->next->pid == current_process->parent->pid) {
            return_pcb = tmp->next;
            tmp->next = return_pcb->next;
            if (return_pcb == wait_queue_end) {
                wait_queue_end = tmp;
            }
            return_pcb->next = NULL;
            return return_pcb;
        }
        tmp = tmp->next;
    }
    return NULL;
}

void update_delay_queue(void) {
    pcb *tmp;
    pcb *delay_pcb;
    tmp = delay_queue_head;
    while (tmp->next != NULL) {
        tmp->next->delay_clock--;
        if (tmp->next->delay_clock == 0) {
            delay_pcb = tmp->next;
            tmp->next = tmp->next->next;
            add_ready_queue(delay_pcb);
        }
        else {
            tmp = tmp->next;
        }
    }
}

SavedContext *switch_save_flush(SavedContext *ctpx, void *p1, void *p2) {
    if (p2  ==  NULL) {
        return ((ProcessControlBlock*)p1)->ctx;
    }

    struct pte *pt2;
    pt2 = ((ProcessControlBlock*)p2)->pt_r0;

    WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)pt2));
    WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);

    current_process = (ProcessControlBlock*)p2;
    if((ProcessControlBlock*)p1 != idle_process) {
        add_ready_queue((ProcessControlBlock*)p1);
    }

    return ((ProcessControlBlock*)p2)->ctx;
}

SavedContext *init_save_flush(SavedContext *ctx, void *p1, void *p2) {
    memcpy(kernel_stack_buff, (void*)KERNEL_STACK_BASE, PAGESIZE*KERNEL_STACK_PAGES);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    WriteRegister(REG_PTR0, vaddr2paddr((unsigned long)((pcb*)p2)->pt_r0));
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    memcpy((void*)KERNEL_STACK_BASE, kernel_stack_buff, PAGESIZE*KERNEL_STACK_PAGES);
    memcpy(((pcb*)p2)->ctx, ((pcb*)p1)->ctx, sizeof(SavedContext));
    current_process = p2;
    return ((pcb*)p2)->ctx;
}

SavedContext *delay_save_flush(SavedContext *ctpx, void *p1, void *p2) {
    if (p2 == NULL) {
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)idle_pt_r0));
        WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        add_delay_queue((ProcessControlBlock*)p1);
        current_process = idle_process;
        return idle_process->ctx;
    }
    else {
        pte *pt2 = ((ProcessControlBlock*)p2)->pt_r0;
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)pt2));
        WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        add_delay_queue((ProcessControlBlock*)p1);
        current_process = (ProcessControlBlock*)p2;
        return ((ProcessControlBlock*)p2)->ctx;
    }
}

SavedContext *fork_save_flush(SavedContext *ctpx, void *p1, void *p2) {

    unsigned long addi_pte_vpn = USER_STACK_LIMIT>>PAGESHIFT;
    struct pte *pt2;
    unsigned long i;
    pt2 = ((ProcessControlBlock*)p2)->pt_r0;
    for (i = 0;i<PAGE_TABLE_LEN;i++) {
        pt2[i].valid = 0;
        if (current_process->pt_r0[i].valid) {
            pt2[i].valid = 1;
            if (i >= PAGE_TABLE_LEN-KERNEL_STACK_PAGES) pt2[i].uprot = PROT_NONE;
            else pt2[i].uprot = PROT_READ | PROT_WRITE;
            pt2[i].kprot =  PROT_READ | PROT_WRITE;
            pt2[i].pfn = get_free_page();
            current_process->pt_r0[addi_pte_vpn].valid = 1;//XXX
            current_process->pt_r0[addi_pte_vpn].uprot = PROT_NONE;//XXX
            current_process->pt_r0[addi_pte_vpn].kprot = PROT_READ | PROT_WRITE;//XXX
            current_process->pt_r0[addi_pte_vpn].pfn = pt2[i].pfn;//XXX
            memcpy((void*)(addi_pte_vpn<<PAGESHIFT), (void *)(i<<PAGESHIFT), PAGESIZE);//XXX
            current_process->pt_r0[addi_pte_vpn].valid = 0;
            WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        }
    }

    WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)pt2));
    WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
    memcpy(((ProcessControlBlock*)p2)->ctx,ctpx,sizeof(SavedContext));
    current_process = (ProcessControlBlock*)p2;
    add_ready_queue((ProcessControlBlock*)p1);

    return ((ProcessControlBlock*)p2)->ctx;
}

SavedContext *exit_save_flush(SavedContext *ctpx, void *p1, void *p2) {
    unsigned long i;
    status_queue *temp_status;
    struct pte *pt1 = ((ProcessControlBlock*)p1)->pt_r0;


    for (i = 0; i<PAGE_TABLE_LEN; i++) {
        if(pt1[i].valid){
            pt1[i].kprot |= PROT_WRITE;
            remove_used_page(&pt1[i]);
            pt1[i].valid = 0;
        }
    }

    if(p2 == NULL) {
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)idle_pt_r0));
    }
    else {
        struct pte *pt2 = ((ProcessControlBlock*)p2)->pt_r0;
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)pt2));
    }
    WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);

    free(((ProcessControlBlock*)p1)->ctx);
    while(((ProcessControlBlock*)p1)->statusQ != NULL) {
        temp_status = ((ProcessControlBlock*)p1)->statusQ;
        ((ProcessControlBlock*)p1)->statusQ = ((ProcessControlBlock*)p1)->statusQ->next;
        free(temp_status);
    }
    free((ProcessControlBlock*)p1);

    if (p2 == NULL) {
        current_process = idle_process;
        return idle_process->ctx;
    }
    else {
        current_process = (ProcessControlBlock*)p2;
        return ((ProcessControlBlock*)p2)->ctx;
    }
}

SavedContext *wait_save_flush(SavedContext *ctpx, void *p1, void *p2) {
    if (p2 == NULL) {
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)idle_pt_r0));
        WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        add_wait_queue((ProcessControlBlock*)p1);
        current_process = idle_process;
        return idle_process->ctx;
    }
    else {
        struct pte *pt2 = ((ProcessControlBlock*)p2)->pt_r0;
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)pt2));
        WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        add_wait_queue((ProcessControlBlock*)p1);
        current_process = (ProcessControlBlock*)p2;

        return ((ProcessControlBlock*)p2)->ctx;
    }
}

SavedContext *tty_save_flush(SavedContext *ctpx, void *p1, void *p2) {
    if (p2 == NULL) {
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)idle_pt_r0));
        WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        current_process = idle_process;
        return idle_process->ctx;
    }
    else {
        pte *pt2 = ((ProcessControlBlock*)p2)->pt_r0;
        WriteRegister(REG_PTR0,vaddr2paddr((unsigned long)pt2));
        WriteRegister(REG_TLB_FLUSH,TLB_FLUSH_0);
        current_process = (ProcessControlBlock*)p2;
        return ((ProcessControlBlock*)p2)->ctx;
    }
}

RCS421RegVal vaddr2paddr(unsigned long vaddr) {
    unsigned long idx = (vaddr-VMEM_1_BASE)>>PAGESHIFT;
    return (RCS421RegVal)(pt_r1[idx].pfn<<PAGESHIFT|(vaddr&PAGEOFFSET));
}