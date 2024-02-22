/* 
   Header file for data structures initialization 
*/

#ifndef DS
#define DS

// Include necessary Yalnix header files
#include <comp421/loadinfo.h>
#include <comp421/hardware.h>
#include <comp421/yalnix.h>

// Define a forward declaration for the page table entry (pte) structure
typedef struct pte pte;

// Define a structure for the status queue used for waiting processes
typedef struct StatusQueue{
    int pid;                  // Process ID
    int status;               // Process status
    struct StatusQueue *next; // Pointer to the next node in the queue
} StatusQueue;               

// Define a structure for the process control block (PCB)
typedef struct pcb{
    unsigned int pid;         // Process ID
    struct pte *pt_r0;        // Page table pointer for region 0
    SavedContext *ctx;        // Context pointer for process
    int n_child;              // Number of child processes
    int delay_clock;          // Delay clock for process
    unsigned long brk;        // Break address for process
    StatusQueue *statusQ;     // Pointer to the status queue
    struct pcb *parent;       // Pointer to the parent process
    struct pcb *next;         // Pointer to the next PCB in the list
} pcb;                       

// Typedef for the ProcessControlBlock structure
typedef pcb ProcessControlBlock;

// Define a structure for terminal information
typedef struct terminal{
    int n_buf_char;           // Number of characters in the buffer
    char read_buf[256];       // Read buffer
    char *write_buf;          // Write buffer
    pcb *readQ_head;          // Pointer to the head of read queue
    pcb *readQ_end;           // Pointer to the end of read queue
    pcb *writingProc;         // Pointer to the process currently writing
    pcb *writeQ_head;         // Pointer to the head of write queue
    pcb *writeQ_end;          // Pointer to the end of write queue
} terminal;

// Define a structure for free pages
typedef struct freePage {
    unsigned int phys_frame_num; // Physical frame number
    struct freePage *next;       // Pointer to the next free page in the list
} freePage;                      

#endif // DS
