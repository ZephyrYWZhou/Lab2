#include "kernelHeader.h"

/*
 * Load a program into the current process's address space. The program comes from the Unix file identified by "name", and its arguments come from the array at "args", which is in standard argv format.
 *
 * Returns:
 *   0 on success
 *  -1 on any error for which the current process is still runnable
 *  -2 on any error for which the current process is no longer runnable
 *
 * This function, after a series of initial checks, deletes the contents of Region 0, thus making the current process no longer runnable. Before this point, it is possible to return ERROR to an Exec() call that has called LoadProgram, and this function returns -1 for errors up to this point. After this point, the contents of Region 0 no longer exist, so the calling user process is no longer runnable, and this function returns -2 for errors in this case.
 */
int LoadProgram(char *name, char **args, ExceptionInfo *info) {
    int fd;
    int status;
    struct loadinfo li;
    char *cp;
    char *cp2;
    char **cpp;
    char *arg_buffer;
    int i;
    unsigned long arg_num;
    int size;
    int text_npg;
    int data_bss_npg;
    int stack_npg;
    int used_pages_num;
    unsigned long user_stack_limit = (USER_STACK_LIMIT >> PAGESHIFT) - 1;

    TracePrintf(9, "LoadProgram '%s', args %p\n", name, args);

    // Open the program file
    if ((fd = open(name, O_RDONLY)) < 0) {
        TracePrintf(9, "LoadProgram: can't open file '%s'\n", name);
        return (-1);
    }

    // Load information about the program
    status = LoadInfo(fd, &li);
    TracePrintf(9, "LoadProgram: LoadInfo status %d\n", status);

    switch (status) {
        case LI_SUCCESS:
            break;
        case LI_FORMAT_ERROR:
            TracePrintf(9, "LoadProgram: '%s' not in Yalnix format\n", name);
            close(fd);
            return (-1);
        case LI_OTHER_ERROR:
            TracePrintf(9, "LoadProgram: '%s' other error\n", name);
            close(fd);
            return (-1);
        default:
            TracePrintf(9, "LoadProgram: '%s' unknown error\n", name);
            close(fd);
            return (-1);
    }

    TracePrintf(9, "text_size 0x%lx, data_size 0x%lx, bss_size 0x%lx\n", li.text_size, li.data_size, li.bss_size);
    TracePrintf(9, "entry 0x%lx\n", li.entry);

    // Calculate size of arguments
    size = 0;
    for (i = 0; args[i] != NULL; i++) {
        size += strlen(args[i]) + 1;
    }
    arg_num = i;
    TracePrintf(9, "LoadProgram: size %d, arg_num %d\n", size, arg_num);

    // Allocate buffer for arguments
    cp = arg_buffer = (char *)malloc(size);

    for (i = 0; args[i] != NULL; i++) {
        strcpy(cp, args[i]);
        cp += strlen(cp) + 1;
    }

    // Calculate address for arguments on the stack
    cp = ((char *)USER_STACK_LIMIT) - size;
    cpp = (char **)((unsigned long)cp & (-1 << 4)); // align cpp
    cpp = (char **)((unsigned long)cpp - ((arg_num + 4) * sizeof(void *)));

    // Calculate number of pages required for text, data, and stack
    text_npg = li.text_size >> PAGESHIFT;
    data_bss_npg = UP_TO_PAGE(li.data_size + li.bss_size) >> PAGESHIFT;
    stack_npg = (USER_STACK_LIMIT - DOWN_TO_PAGE(cpp)) >> PAGESHIFT;

    // Check if program size is too large for VM
    if (MEM_INVALID_PAGES + text_npg + data_bss_npg + stack_npg + 1 + KERNEL_STACK_PAGES >= PAGE_TABLE_LEN) {
        TracePrintf(9, "LoadProgram: program '%s' size too large for VM\n", name);
        free(arg_buffer);
        close(fd);
        return (-1);
    }

    // Check if enough physical memory is available
    used_pages_num = 0;
    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES; i++) {
        if (current_process->pt_r0[i].valid == 1) {
            current_process->pt_r0[i].uprot = PROT_READ | PROT_WRITE;
            current_process->pt_r0[i].kprot = PROT_READ | PROT_WRITE;
            used_pages_num++;
        }
    }

    if (text_npg + data_bss_npg + stack_npg > num_free_frame + used_pages_num) {
        TracePrintf(9, "LoadProgram: program '%s' size too large for physical memory\n", name);
        free(arg_buffer);
        close(fd);
        return (-1);
    }

    // Set stack pointer for the current process
    info->sp = (char *)cpp;

    // Free all old physical memory belonging to this process
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES; i++) {
        if (current_process->pt_r0[i].valid) {
            remove_used_page(&(current_process->pt_r0[i]));
            current_process->pt_r0[i].valid = 0;
            WriteRegister(REG_TLB_FLUSH, (unsigned long)((current_process->pt_r0) + i));
        }
    }

    // Initialize page table entries
    for (i = 0; i < MEM_INVALID_PAGES; i++) {
        current_process->pt_r0[i].valid = 0;
    }

    // Initialize text pages
    for (; i < MEM_INVALID_PAGES + text_npg; i++) {
        current_process->pt_r0[i].uprot = PROT_READ | PROT_EXEC;
        current_process->pt_r0[i].kprot = PROT_READ | PROT_WRITE;
        current_process->pt_r0[i].valid = 1;
        current_process->pt_r0[i].pfn = get_free_page();
    }

    // Initialize data and bss pages
    for (; i < MEM_INVALID_PAGES + text_npg + data_bss_npg; i++) {
        current_process->pt_r0[i].uprot = PROT_READ | PROT_WRITE;
        current_process->pt_r0[i].kprot = PROT_READ | PROT_WRITE;
        current_process->pt_r0[i].valid = 1;
        current_process->pt_r0[i].pfn = get_free_page();
    }

    // Initialize user stack pages
    for (i = 0; i < stack_npg; i++) {
        current_process->pt_r0[user_stack_limit - i].uprot = PROT_READ | PROT_WRITE;
        current_process->pt_r0[user_stack_limit - i].kprot = PROT_READ | PROT_WRITE;
        current_process->pt_r0[user_stack_limit - i].valid = 1;
        current_process->pt_r0[user_stack_limit - i].pfn = get_free_page();
    }

    // Flush TLB to clear old PTEs
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    // Read the text and data from the file into memory
    if (read(fd, (void *)MEM_INVALID_SIZE, li.text_size + li.data_size) != li.text_size + li.data_size) {
        TracePrintf(9, "LoadProgram: couldn't read for '%s'\n", name);
        free(arg_buffer);
        close(fd);
        // Terminate process with an exit status of ERROR
        kernel_Exit(ERROR);
        return (-2);
    }

    close(fd); // Close the file

    // Set page table entries for program text to be readable and executable
    for (; i < MEM_INVALID_PAGES + text_npg; i++) {
        current_process->pt_r0[i].kprot = PROT_READ | PROT_EXEC;
    }

    // Flush TLB to clear old PTEs
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    // Zero out the bss
    memset((void *)(MEM_INVALID_SIZE + li.text_size + li.data_size), '\0', li.bss_size);

    // Set the entry point in the exception frame
    info->pc = (void *)li.entry;

    // Build the argument list on the new stack
    *cpp++ = (char *)arg_num; // argc
    cp2 = arg_buffer;

    for (i = 0; i < arg_num; i++) {
        *cpp++ = cp; // argv
        strcpy(cp, cp2);
        cp += strlen(cp) + 1;
        cp2 += strlen(cp2) + 1;
    }
    
    free(arg_buffer);
    *cpp++ = NULL; // Last argv is a NULL pointer
    *cpp++ = NULL; // NULL pointer for an empty envp
    *cpp++ = 0;    // Terminate the auxiliary vector

    // Initialize all registers for the current process to 0
    info->psr = 0;
    for (i = 0; i < NUM_REGS; i++) {
        info->regs[i] = 0;
    }
    return (0);
}
