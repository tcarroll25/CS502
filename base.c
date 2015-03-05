/************************************************************************

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.      

        Revision History:       
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
************************************************************************/
#include             <stdlib.h>
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"

#define              LIST_LOCK_ON       1
#define              ONE_LIST_LOCK_ON   1
#define              EVNT_LOCK_ON       1
#define              TIMER_LOCK_ON      1
#define              FRAME_LOCK_ON      1
#define              DISK_LOCK_ON       1

#define              SPART              22
char                 GreatSuccess[] = "      Action Failed\0        Action Succeeded";

/* debugging variables */
static INT32         TIMER_DEBUG =      0;
static INT32         PROC_DEBUG  =      0;
static INT32         ERROR_DEBUG =      0;
static INT32         SVC_DEBUG   =      0;
static INT32         EVENT_DEBUG =      0;
static INT32         LOCK_DEBUG  =      0;
static INT32         SUSP_DEBUG  =      0;
static INT32         RESU_DEBUG  =      0;
static INT32         PRIO_DEBUG  =      0;
static INT32         SEND_DEBUG  =      0;
static INT32         RECV_DEBUG  =      0;
static INT32         MEM_DEBUG   =      0;
static INT32         DISK_DEBUG  =      0;
static INT32         FAULT_DEBUG  =     0;


extern char          MEMORY[];  
//extern BOOL          POP_THE_STACK;
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;
extern INT16         Z502_PROGRAM_COUNTER;
extern INT16         Z502_INTERRUPT_MASK;
extern INT32         SYS_CALL_CALL_TYPE;
extern INT16         Z502_MODE;
extern Z502_ARG      Z502_ARG1;
extern Z502_ARG      Z502_ARG2;
extern Z502_ARG      Z502_ARG3;
extern Z502_ARG      Z502_ARG4;
extern Z502_ARG      Z502_ARG5;
extern Z502_ARG      Z502_ARG6;

extern void          *TO_VECTOR [];
extern INT32         CALLING_ARGC;
extern char          **CALLING_ARGV;

char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ", 
                            "get_pid  ", "create   ", "term_proc", 
                            "suspend  ", "resume   ", "ch_prior ", 
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };
/* global variables */
static INT32        pid = 0;
static INT32        pTotal = 0;
static INT32        eTotal = 0;
static INT32        current_id = 0;
static PCB          *pList = NULL;
static PCB          *pQueue = NULL;
static EVNT         *pEvent = NULL;
static FTBL         *pFrame = NULL;
static char         DISK_BIT_MAP[MAX_NUMBER_OF_DISKS][NUM_LOGICAL_SECTORS];

/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS. 
************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
    
    // Get cause of interrupt
    ZCALL(MEM_READ(Z502InterruptDevice, &device_id )); 

    // Clear event if there are none
    CALL(os_event_clear());
    
    //Loop until we grab all the interrupts that have occoured
    while(device_id != -1){
        // Set this device as target of our query
        ZCALL(MEM_WRITE(Z502InterruptDevice, &device_id ));
    
        // Now read the status of this device
        ZCALL(MEM_READ(Z502InterruptStatus, &status ));
        
        // Add this event to event queue
        CALL(os_event_add(device_id,status));
        
        // Clear out this device - we're done with it
        ZCALL(MEM_WRITE(Z502InterruptClear, &Index ));
    
        // Get cause of interrupt
        ZCALL(MEM_READ(Z502InterruptDevice, &device_id ));
    }
 
    return;
}                                       /* End of interrupt_handler */

/************************************************************************
    HANDLE_EVENTS
        Checks all events that have occured from interrupts
        and handle each one
************************************************************************/
INT32   handle_events( INT32 *ret ){
    INT32              device_id;
    INT32              status;
    INT32              next = 0, events = 0;
    
    if(EVENT_DEBUG) CALL(os_event_print());

    //Get next event
    CALL(next = os_event_get_next(&device_id, &status));
    while(next == 0){
        if(EVENT_DEBUG) printf("Handling event from device: %d and status: %d\n", device_id, status);
        (*ret) = status;
        events++;

        //handle event according to which device it originated from
        switch(device_id){
            case TIMER_INTERRUPT:
                CALL(timer_interrupt());
                break;
            case DISK_INTERRUPT_DISK1:
            case DISK_INTERRUPT_DISK2:
            case DISK_INTERRUPT_DISK3:
            case DISK_INTERRUPT_DISK4:
            case DISK_INTERRUPT_DISK5:
            case DISK_INTERRUPT_DISK6:
            case DISK_INTERRUPT_DISK7:
            case DISK_INTERRUPT_DISK8:
            case DISK_INTERRUPT_DISK9:
            case DISK_INTERRUPT_DISK10:
            case DISK_INTERRUPT_DISK11:
            case DISK_INTERRUPT_DISK12:
                CALL(disk_interrupt(device_id-4,status)); 
                break;
            default:
                break;
        }

        //keep getting events until we run out of them
        CALL(next = os_event_get_next(&device_id, &status));
    }

    return events;
}

/************************************************************************
    TIMER_INTERRUPT
        Handles timer interrupt, wakes up processes and restarts timer
************************************************************************/
void timer_interrupt( void )
{
    INT32 curr_time;

    //Read timer status
    ZCALL(MEM_READ( Z502ClockStatus, &curr_time ));

    //Wake up processes
    if(wakeup_timer(curr_time) <= 0){
       printf("Nothing needed to wake up yet, continuing...\n"); 
    }
    
    //restart timer
    CALL(restart_timer(curr_time));    
}

/************************************************************************
    DISK_INTERRUPT
        Handles disk interrupt
************************************************************************/
void disk_interrupt(INT32 disk, INT32 status)
{
    INT32 id;
    INT32 error;    
    INT32 shadow;
    INT32 page;
    INT32 sector;
    INT32 addr;
    INT16 call_type;
    INT32 mem_write_action = 0;

    call_type = (INT16)SYS_CALL_CALL_TYPE;

    if(DISK_DEBUG) printf("Handling disk event from disk %d with status %d\n", disk, status);

    //check status
    switch(status){
        case ERR_SUCCESS:
            if(DISK_DEBUG) printf("Disk action successful!\n");
            break;
        case ERR_BAD_PARAM:
            if(DISK_DEBUG) printf("Disk action unsuccessful: bad param!\n");
            break;
        case ERR_NO_PREVIOUS_WRITE:
            if(DISK_DEBUG) printf("Disk action unsuccessful: no previous write!\n");
            break;
        case ERR_DISK_IN_USE:
            if(DISK_DEBUG) printf("Disk action unsuccessful: disk in use!\n");
            break;
    }   

    //figure out which process was using this disk
    CALL(id = os_pcb_list_get_id_by_disk_in_use(disk));
    if(DISK_DEBUG) printf("Process %d was waiting for disk %d, waking it up now!\n", id, disk);

    //figure out which sector we were writing to
    CALL(sector = os_pcb_list_get_sector_in_use_by_id( id ));
    
    if(DISK_DEBUG) printf("Process %d was r/w to sector %d!\n", id, sector);

    //figure out if a virtual page was just read, we are putting a page in memory that we just read from disk
    if(call_type == SYSNUM_MEM_READ){
        if(DISK_DEBUG) printf("Checking shadow table for mem read\n");
        CALL(shadow = os_pcb_list_get_shadow_table_page_by_sector(&page, disk, sector, id));
        if(shadow != -1){
            //Get page to write to
            CALL(addr = os_frame_page_to_addr(page));
            if(DISK_DEBUG) printf("shadow table shows disk %d sec %d was for page %d addr %d\n", disk, sector, page, addr);
            //Trick fault handler to think we are doing a write
            SYS_CALL_CALL_TYPE = SYSNUM_MEM_WRITE;
            //We trick the fault handler so that we can call mem_write and it will find
            //this data a frame and write the frame's old contents into disk automatically
            mem_write_action = 1;
        }
    }

    CALL(os_pcb_list_set_disk_in_use_by_id(id,0));
    CALL(os_pcb_list_set_sector_in_use_by_id(id,0));

    //set process that was using the disk back to ready, it will be resumed by the event handler
    CALL(status = os_pcb_list_set_state_by_id(id, READY_STATE));
    if(status < 0){
        if(DISK_DEBUG) printf("Could not set proc back to ready state after disk action\n");
        return;
    }

    if(mem_write_action == 1){
        //Write back into phys memory
        CALL(mem_write(addr, Z502_ARG2.PTR));
    }

    return;
}

/************************************************************************
    SWITCH_TO_NEXT_HIGHEST_PRIORITY
        Grabs highest priority PCB and switches to it if we need to
************************************************************************/
void switch_to_next_highest_priority( void ){
    INT32 high_prior_id, curr_id, state;
    PCB *p_high_prior, *p_curr;

    CALL(high_prior_id = os_pcb_list_get_high_prior_id());
    CALL(state = os_pcb_list_get_state_by_id(high_prior_id));
    if(state < 0){
        if(PROC_DEBUG) printf("SVC: Could not get high priority task\n");
    }else if(state == RUNNING_STATE){
        //highest priority is running, we are done
        return;
    }else{
        //not running highest priority, switch!
        CALL(switch_process(high_prior_id, SWITCH_CONTEXT_SAVE_MODE));
        return;
    }
}


/************************************************************************
    IDLE
        Scans for events that would occur from interrupts and handles
        them
************************************************************************/
void    idle(){
    INT32 events_total = 0, events_handled = 0;
    INT32 ret_status;
    
    //sit in a loop looking for events, switch to higher priority if we handle an event, if one exists
    while(events_handled == 0){
        CALL(events_total = os_event_get_total());
        if(events_total > 0){
            CALL(events_handled = handle_events(&ret_status));
            if(events_handled > 0){
                CALL(switch_to_next_highest_priority());
            }
        }
    }
    
    return;
}

/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void )
    {
    INT32       device_id;
    INT32       status, error;
    INT32       Index = 0;

    // Get cause of interrupt
    ZCALL(MEM_READ(Z502InterruptDevice, &device_id )); 
    // Set this device as target of our query
    ZCALL(MEM_WRITE(Z502InterruptDevice, &device_id ));
    // Now read the status of this device
    ZCALL(MEM_READ(Z502InterruptStatus, &status ));
    
    // Clear out this device - we're done with it
    ZCALL(MEM_WRITE(Z502InterruptClear, &Index ));

    if(FAULT_DEBUG) printf( "Fault_handler: Found vector type %d with value %d\n", 
                        device_id, status );
    switch(device_id){

        case CPU_ERROR:
            //Terminate process that faulted
            if(FAULT_DEBUG) printf("Fault_handler: A CPU ERROR HAS OCCURED\n");
            CALL(terminate_process(-2, &error));

            break;
        case INVALID_MEMORY:
            //Call the memory fault handler
            if(FAULT_DEBUG) printf("Fault_handler: INVALID MEMORY\n");
            CALL(page_fault_handler(status));          

            break;
        case PRIVILEGED_INSTRUCTION:
            //Terminate process that faulted
            if(FAULT_DEBUG) printf("Fault_handler: PRIVILEGED INSTRUCTION\n");
            CALL(terminate_process(-2, &error));

            break;
    }
}                                       /* End of fault_handler */

/************************************************************************
    PAGE_FAULT_HANDLER
        Handle page faults

************************************************************************/

void    page_fault_handler(INT32 page){

    INT16 call_type;
    INT32 curr_id, id;
    INT32 status;
    INT32 frame;
    UINT16 page_entry = 0;
    INT32 error;
    INT32 disk = 1;
    INT32 seg = 0;
    INT32 read_disk;
    INT32 read_seg;
    INT32 addr = 0;
    INT32 old_page = 0;
    INT32 disk_write_action = 0;
    INT32 disk_read_action = 0;
    char buffer[PGSIZE];
    memset(buffer,0,PGSIZE);
    
    if(FAULT_DEBUG) printf("IN PAGE FAULT HANDLER!!!\n");

    //Terminate the process if they try to write to an illegal virtual page
    if(page >= Z502_PAGE_TBL_LENGTH || page < 0){
        printf("This is not a valid virtual page!\n");
        CALL(terminate_process(-2,&error));
        return;
    }

    call_type = (INT16)SYS_CALL_CALL_TYPE;
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());
    
    switch(call_type){
        case SYSNUM_MEM_READ:
            
            //Does this virtual page exist on the disk?
            CALL(status = os_pcb_list_get_shadow_table_page(page, &read_disk, &read_seg, curr_id));
            if(status == 0){
                if(FAULT_DEBUG) printf("shadow table shows page %d is stored at disk %d seg %d, reading now\n", page, read_disk, read_seg);
                CALL(disk_read(read_disk, read_seg, Z502_ARG2.PTR));
            }
            //we will have to finish the mem read in the interrupt handler
            //because we are about to do a disk read and suspend
            break;

        case SYSNUM_MEM_WRITE:

            //Get next emtpy frame
            CALL(frame = os_frame_get_next_empty_frame());
            if(frame == -1 ){
                //Full frame table
                if(FAULT_DEBUG) printf("Frame table is full!!\n");
                //Get frame that has been touched last
                CALL(frame = os_frame_get_last_touched_frame());
                if(FAULT_DEBUG) printf("Last touched frame: %d\n", frame);
                //Get page from last touched frame
                CALL(old_page = os_frame_get_page(frame, &id));
                if(FAULT_DEBUG) printf("Frame %d was being used by id %d and vpg %d\n", frame, id, old_page);
                //Get the disk for this page
                CALL(status = os_pcb_list_get_shadow_table_page(old_page, &disk, &seg, curr_id));
                if(status == 0){
                    if(FAULT_DEBUG) printf("Page %d is already on disk at disk %d seg %d\n", old_page, disk, seg);
                }else{
                    //Doesnt have disk yet, get next available segment on disk
                    for(disk = 1; disk <= MAX_NUMBER_OF_DISKS; disk++){
                        CALL(seg = os_disk_get_next_free_sector(disk));
                        if(seg != -1){
                            break;
                        }
                    }
                    if(seg == -1){
                        printf("We are out of disk space!\n");
                        CALL(terminate_process(-2,&error));
                        return;
                    }
                    if(FAULT_DEBUG) printf("Next free hard drive is disk %d seg %d\n", disk, seg);
                    //Set shadow table
                    CALL(os_pcb_list_set_shadow_table_page(old_page, disk, seg, id));
                    if(FAULT_DEBUG) printf("Setting shadow table of ID %d page %d to disk %d seg %d\n", id, old_page, disk, seg);
                }
                //Get page from phys memory
                CALL(addr = os_frame_page_to_addr(old_page));
                if(FAULT_DEBUG) printf("Reading out of phys mem addr %d to put in disk\n", addr);
                //Read from memory
                CALL(mem_read(addr,(UINT32 *)buffer));
                //Set old page to invalid
                CALL(os_pcb_list_set_page_table_page(id, old_page, 0));
                //Set disk write flag, need to do this last because process will suspenc
                disk_write_action = 1;
            }

            //Set frame table to page that faulted
            if(FAULT_DEBUG) printf("Setting frame %d to page %d\n", frame, page);
            CALL(os_frame_set_page(frame, page, curr_id, NULL));

            //Touch frame so we know when it was used last
            CALL(os_frame_touch_frame(frame, pid));

            //Set page to valid
            if(FAULT_DEBUG) printf("setting page entry %d to point to frame %d\n", page, frame);
            page_entry = frame;
            page_entry |= PTBL_VALID_BIT;
            CALL(os_pcb_list_set_page_table_page(curr_id, page, page_entry));
            
            //Call mem_write
            CALL(mem_write(Z502_ARG1.VAL, Z502_ARG2.PTR));

            break;

        default:
            break;
    }
    
    //CALL(os_frame_print());
    CALL(os_dump_memory());

    //Do disk action
    if(disk_write_action == 1){ 
        //Copy to disk
        if(FAULT_DEBUG) printf("Writing to disk %d seg %d\n", disk, seg);
        CALL(disk_write(disk, seg, buffer));
    }

    return;
}

/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.
************************************************************************/

void    svc( void ) {
    INT16               call_type;
    static INT16        do_print = 10;
    INT32               Time;
    INT32               events_handled, events_total;
    INT32               ret_status;
    
    call_type = (INT16)SYS_CALL_CALL_TYPE;
    if ( do_print > 0 ) {
        if(SVC_DEBUG) printf( "SVC handler: %s %8ld %8ld %8ld %8ld %8ld %8ld\n",
                call_names[call_type], Z502_ARG1.VAL, Z502_ARG2.VAL, 
                Z502_ARG3.VAL, Z502_ARG4.VAL, 
                Z502_ARG5.VAL, Z502_ARG6.VAL );
        do_print--;
    }

    switch(call_type){
        case SYSNUM_GET_TIME_OF_DAY:
            ZCALL (MEM_READ(Z502ClockStatus, &Time));
            *(INT32*)Z502_ARG1.PTR = Time;
            break;
        case SYSNUM_TERMINATE_PROCESS:
            CALL(terminate_process(Z502_ARG1.VAL, Z502_ARG2.PTR));
            break;
        case SYSNUM_SLEEP:
            CALL(process_sleep(Z502_ARG1.VAL));
            break;
        case SYSNUM_CREATE_PROCESS:
            CALL(create_process(Z502_ARG2.PTR, Z502_ARG1.PTR, Z502_ARG3.VAL,
                                    Z502_ARG4.PTR, Z502_ARG5.PTR, USER_MODE));
            break;
        case SYSNUM_GET_PROCESS_ID:
            CALL(get_process_id(Z502_ARG1.PTR, Z502_ARG2.PTR, Z502_ARG3.PTR));
            break;
        case SYSNUM_SUSPEND_PROCESS:
            CALL(suspend_process(Z502_ARG1.VAL, Z502_ARG2.PTR));
            break;
        case SYSNUM_RESUME_PROCESS:
            CALL(resume_process(Z502_ARG1.VAL, Z502_ARG2.PTR));
            break;
        case SYSNUM_CHANGE_PRIORITY:
            CALL(change_priority(Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.PTR));
            break;
        case SYSNUM_SEND_MESSAGE:
            CALL(send_message(Z502_ARG1.VAL, Z502_ARG2.PTR, Z502_ARG3.VAL,
                                    Z502_ARG4.PTR));
            break;
        case SYSNUM_RECEIVE_MESSAGE:
            CALL(receive_message(Z502_ARG1.VAL, Z502_ARG2.PTR, Z502_ARG3.VAL,
                                    Z502_ARG4.PTR, Z502_ARG5.PTR, Z502_ARG6.PTR)); 
            break;
        case SYSNUM_MEM_READ:
            //The sneaky z502 code skips SVC for mem operations
            CALL(mem_read(Z502_ARG1.VAL, Z502_ARG2.PTR));
            break;
        case SYSNUM_MEM_WRITE:
            //The sneaky z502 code skips SVC for mem operations
            CALL(mem_write(Z502_ARG1.VAL, Z502_ARG2.PTR));
            break;
        case SYSNUM_DISK_READ:
            CALL(disk_read(Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.PTR));
            break;
        case SYSNUM_DISK_WRITE:
            CALL(disk_write(Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.PTR));
            break;
        case SYSNUM_DEFINE_SHARED_AREA:
            CALL(define_shared_area(Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.PTR,
                                        Z502_ARG4.PTR, Z502_ARG5.PTR));
            break;
        default:
            printf("ERROR! call_type is not recognized!\n");
            printf("call_type is - %i\n", call_type);
            ZCALL(Z502_HALT());
    }

    //handle all events add by interrupts before next syscall
    if(call_type != SYSNUM_TERMINATE_PROCESS){
        CALL(events_total = os_event_get_total());
        if(events_total > 0){
            CALL(events_handled = handle_events(&ret_status));
            if(events_handled > 0){
                CALL(switch_to_next_highest_priority());
            }
        }
    }    

}                                               // End of svc
 
/************************************************************************
    OS_SWITCH_CONTEXT_COMPLETE
        The hardware, after completing a process switch, calls this routine
        to see if the OS wants to do anything before starting the user 
        process.
************************************************************************/

void    os_switch_context_complete( void )
    {
    static INT16        do_print = TRUE;
    INT32               i;
    INT16               call_type;
    INT32*              temp;
    call_type = (INT16)SYS_CALL_CALL_TYPE;

    if ( do_print == TRUE )
    {
        printf( "os_switch_context_complete  called before user code.\n");
        do_print = FALSE;
    }

    //Point to page table
    Z502_PAGE_TBL_LENGTH = VIRTUAL_MEM_PGS;
    CALL(Z502_PAGE_TBL_ADDR = os_pcb_list_get_page_table_by_id(current_id ));
    
    switch(call_type){
        case SYSNUM_RECEIVE_MESSAGE:
            //If this is as receive message, manually populate return variables
            CALL(os_pcb_list_get_last_msg_from_inbox(current_id, Z502_ARG5.PTR, Z502_ARG2.PTR, Z502_ARG4.PTR));
            break;
        case SYSNUM_DISK_READ:
            //this is purely for debug purposes, prints what we just read from disk
            temp = (INT32 *)Z502_ARG3.PTR;
            if(DISK_DEBUG) printf("disk_read: read: ");
            for(i = 0; i < 4; i++){
                if(DISK_DEBUG) printf(" %d ",temp[i]);
            }
            if(DISK_DEBUG) printf("\n");

            break;
        default:
            break;
    }
}                               /* End of os_switch_context_complete */

/************************************************************************
    OS_INIT
        This is the first routine called after the simulation begins.  This 
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    os_init( void )
{
    void                *next_context;
    INT32               i, id, error;
    void                *funcPtr;

    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", CALLING_ARGC );
    for ( i = 0; i < CALLING_ARGC; i++ )
        printf( " %s", CALLING_ARGV[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

    /*  Setup frame table */
    CALL(os_frame_tbl_init());
    //CALL(os_frame_print());
    //CALL(os_dump_memory());

    /* Setup disk bit map */
    CALL(os_disk_init_map());
    
    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( CALLING_ARGC > 1 ) && ( strcmp( CALLING_ARGV[1], "sample" ) == 0 ) )
        {
        ZCALL( Z502_MAKE_CONTEXT( &next_context, 
                                        (void *)sample_code, KERNEL_MODE ));
        ZCALL( Z502_SWITCH_CONTEXT( SWITCH_CONTEXT_KILL_MODE, &next_context ));
    }                   /* This routine should never return!!           */
    else{
        if(CALLING_ARGC == 1){
            printf("No test specified! Correct usage: ./os <test_name>\n");
            ZCALL(Z502_HALT());
        }
        //get function pointer for test
        CALL(funcPtr = os_get_func_ptr(CALLING_ARGV[1]));
        if(funcPtr == NULL){
            printf("That is not a valid test! Correct usage: ./os <test_name>\n");
            ZCALL(Z502_HALT());
        }
        //create test process
        CALL(create_process( (void *)funcPtr, "test1", 1, &id, &error, USER_MODE));
    }
}

                                               /* End of os_init       */


/************************************************************************
    OS_GET_FUNC_PTR
        Returns the function pointer from the appropriate test.
        Also sets several debugging variables for the appropriate
        output for each test.
************************************************************************/
void    *os_get_func_ptr(const char* name)
{
    if(strcmp("test1a",name) == 0){
        return (void*)test1a;
    }else if(strcmp("test1b",name) == 0){
        return (void*)test1b;
    }else if(strcmp("test1c",name) == 0){
        return (void*)test1c;
    }else if(strcmp("test1d",name) == 0){
        return (void*)test1d;
    }else if(strcmp("test1e",name) == 0){
//        SUSP_DEBUG  = 1;
//        RESU_DEBUG  = 1;
        return (void*)test1e;
    }else if(strcmp("test1f",name) == 0){
//        SUSP_DEBUG  = 1;
//        RESU_DEBUG  = 1;
        return (void*)test1f;
    }else if(strcmp("test1g",name) == 0){
//        PRIO_DEBUG  = 1;
        return (void*)test1g;
    }else if(strcmp("test1h",name) == 0){
//        PRIO_DEBUG  = 1;
        return (void*)test1h;
    }else if(strcmp("test1i",name) == 0){
        SEND_DEBUG  = 1;
        RECV_DEBUG  = 1;
        return (void*)test1i;
    }else if(strcmp("test1j",name) == 0){
        SEND_DEBUG  = 1;
        RECV_DEBUG  = 1;
        return (void*)test1j;
    }else if(strcmp("test1k",name) == 0){
        return (void*)test1k;
    }else if(strcmp("test1l",name) == 0){
        return (void*)test1l;
    }else if(strcmp("test1m",name) == 0){
        return (void*)test1m;
    }else if(strcmp("test2a",name) == 0){
        //MEM_DEBUG  = 1;
        return (void*)test2a;
    }else if(strcmp("test2b",name) == 0){
        //MEM_DEBUG  = 1;
        return (void*)test2b;
    }else if(strcmp("test2c",name) == 0){
        //DISK_DEBUG  = 1;
        return (void*)test2c;
    }else if(strcmp("test2d",name) == 0){
        //DISK_DEBUG  = 1;
        return (void*)test2d;
    }else if(strcmp("test2e",name) == 0){
        //DISK_DEBUG  = 1;
        return (void*)test2e;
    }else if(strcmp("test2f",name) == 0){
        //DISK_DEBUG  = 1;
        return (void*)test2f;
    }else if(strcmp("test2g",name) == 0){
        //DISK_DEBUG  = 1;
        return (void*)test2g;
    }else{
        return NULL;
    }
}

/************************************************************************
    PROCESS_SLEEP
        This routine starts to the timer and puts PCB on waiting queue

************************************************************************/
void    process_sleep( INT32 sleep_time ) {
    INT32 curr_time;
    INT32 status;
    PCB * process;
    INT32 id;
   
    //Read current time
    ZCALL(MEM_READ( Z502ClockStatus, &curr_time ));

    //Move current process to delay queue
    CALL(id = os_pcb_get_curr_proc_id());
    if(TIMER_DEBUG) printf("Putting process ID: %d to sleep!!!!!!!!!!!\n", id);
    if(id < 0){
        if(ERROR_DEBUG) printf("Process_Sleep: Current process trying to sleep and doesn't exist, this should never happen!\n");
        CALL(os_dump_stats());
        return;
    }
    
    //Check current process state
    CALL(status = os_pcb_list_get_state_by_id(id));
    if(status == HALTED_STATE){
        if(PROC_DEBUG) printf("Resume Process: cannot sleep a halted process!\n");
        return;   
    }
    
    //Move process onto delay queue
    sleep_time += curr_time;
    
    CALL(status = os_pcb_list_set_state_by_id(id, WAITING_STATE));
    if(status < 0){
        if(ERROR_DEBUG) printf("Process_Sleep: setting state of proc we are sleeping doesn't exist\n");
        if(ERROR_DEBUG) CALL(os_dump_stats());
        return;
    }
    CALL(process = os_pcb_queue_get_by_id(id));
    if(process != NULL){
        if(TIMER_DEBUG) printf("Process_Sleep: This task is already on delay queue!\n");
        return;
    }
    CALL(os_dump_stats2("SLEEP", id));
    CALL(os_pcb_list_to_queue_copy(id, sleep_time));
    
    //Set timer to shortest wait on delay queue
    CALL(id = os_pcb_queue_get_high_prior_id());
    if(id < 0){
        if(ERROR_DEBUG) printf("Process_Sleep: No tasks on delay queue but we just put one there!\n");
        return;
    }       
    if(TIMER_DEBUG) printf("Setting sleep timer with ID: %d !!!!!!!!!!!\n", id);
    
    //Get wake up time of first task on delay queue 
    CALL(status = os_pcb_queue_get_wakeup_by_id(id));
    if(status <= 0){
        if(ERROR_DEBUG) printf("Process_Sleep: Task just disappeared from delay queue, we just got its ID!\n");
        return;
    }       

    //Calculate sleep time
    sleep_time = status - curr_time;
    if(TIMER_DEBUG) printf("Process_Sleep: Trying to set timer to %d, curr time %d, wakeup: %d!\n", sleep_time, curr_time, status);
    if(sleep_time <= 0){
        if(TIMER_DEBUG) printf("Trying to set timer to invalid value, pcb put back on ready list!\n");
        
        CALL(os_pcb_queue_del(id));
        CALL(status = os_pcb_list_set_state_by_id(id, READY_STATE));
        if(status < 0){
            if(ERROR_DEBUG) printf("Process_Sleep: setting state of proc we are sleeping doesn't exist\n");
            if(ERROR_DEBUG) CALL(os_dump_stats());
        }
        return;
    }

    if(TIMER_DEBUG) CALL(os_dump_stats());

    //Set timer
    CALL(timer_spinlock_get());
    ZCALL(MEM_WRITE( Z502TimerStart, &sleep_time ));
    CALL(timer_spinlock_give());
    
    //Switch to highest priority task or idle if none   
    CALL(id = os_pcb_list_get_high_prior_id());
    CALL(process = os_pcb_list_get_by_id(id));
    if(process == NULL){
        CALL(idle());                //  Let the interrupt for this timer occur
    }else{
        CALL(switch_process(id, SWITCH_CONTEXT_SAVE_MODE));
    }
}

/************************************************************************
    WAKEUP_TIMER
        This function moves all process that are awake off of
        the timer queue
************************************************************************/
INT32    wakeup_timer( INT32 curr_time ){
    INT32 id, status;
    PCB * process;
    INT32 wake_up_time, wake_ups = 0;
        
    while(1){
        //get first item on timer queue
        CALL(id = os_pcb_queue_get_high_prior_id());
        if(id < 0){
            break;
        }
        //get the wake up time
        CALL(wake_up_time = os_pcb_queue_get_wakeup_by_id(id));
        if(wake_up_time < 0){
            if(ERROR_DEBUG) printf("wakeup_timer: trying to wake up process that doesn't exist\n");
            break;
        }
        //see if we can wake up
        if(wake_up_time <= curr_time){
            wake_ups++;
            CALL(os_pcb_queue_del(id));
            CALL(status = os_pcb_list_set_state_by_id(id, READY_STATE));
            CALL(os_dump_stats2("WAKEUP", id));
            //see if anyone can receive now
            CALL(os_pcb_list_check_outbox_for_receivers(id));
            if(status < 0){
                if(ERROR_DEBUG) printf("Resume Process: setting state of proc we are resuming doesn't exist\n");
                if(ERROR_DEBUG) CALL(os_dump_stats());
            }
            if(TIMER_DEBUG) printf("Waking up id: %d\n", id);
        }else{
            break;
        }
    }
    
    return wake_ups;
}

/************************************************************************
    RESTART_TIMER
        This function restarts the timer based on lowest wakeup on list
************************************************************************/
void    restart_timer( INT32 curr_time ){
    INT32 sleep_time;
    INT32 status;
    INT32 id;
    PCB * process;
    
    //Set timer to shortest wait on delay queue
    CALL(id = os_pcb_queue_get_high_prior_id());
    CALL(process = os_pcb_queue_get_by_id(id));
    if(process == NULL){
        return;
    }
    
    //Calculate sleep time
    sleep_time = process->wake_up_time - curr_time;

    if(TIMER_DEBUG) printf("Restarting timer with time %d from proc %d\n", sleep_time, id);

    //Start timer
    CALL(timer_spinlock_get());
    ZCALL(MEM_WRITE( Z502TimerStart, &sleep_time ));
    CALL(timer_spinlock_give());

    return;
}

/************************************************************************
    OS_DUMP_STATS
        This is routine prints out debugging information
************************************************************************/
void    os_dump_stats( void ){
    PCB *list = pList;
    INT32 curr_time, curr_id;
    ZCALL(MEM_READ( Z502ClockStatus, &curr_time ));
 
    printf("**********Dumping OS Stats!**********\n");
    printf("Current system time: %d\n", curr_time);
    CALL(curr_id = os_pcb_get_curr_proc_id());
    printf("Current running process id: %d\n", curr_id);
    
    CALL(os_pcb_print_all());
    printf("*************************************\n");

    return;
}

/************************************************************************
    OS_DUMP_STATS
        This is routine prints out debugging information
        using state printing
************************************************************************/
void    os_dump_stats2( char* action, INT32 id ){
    PCB *list = pList;
    INT32 curr_time, curr_id;
    ZCALL(MEM_READ( Z502ClockStatus, &curr_time ));
    CALL(curr_id = os_pcb_get_curr_proc_id());
    
    //Get lock
    CALL(list_spinlock_get());
    
    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return;
    }
    
    CALL( SP_setup( SP_TIME_MODE, curr_time) );
    CALL( SP_setup_action( SP_ACTION_MODE, action ) );
    CALL( SP_setup( SP_TARGET_MODE, id ) );
    CALL( SP_setup( SP_RUNNING_MODE, curr_id ) );

    //add new state if we created a process
    if(strcmp(action,"CREATE") == 0){
        CALL( SP_setup( SP_NEW_MODE, id ) );
    }
    //add done state if we deleted a process
    if(strcmp(action,"DESTROY") == 0){
        CALL( SP_setup( SP_TERMINATED_MODE, id ) );
    }
    
    if(strcmp(action,"SWAPPED") == 0){
        CALL( SP_setup( SP_TERMINATED_MODE, id ) );
    }

    //Find the end of the list
    while(list != NULL){
        
        switch(list->state){
            case READY_STATE:
                CALL( SP_setup( SP_READY_MODE, list->id ) );
                break;
            case WAITING_STATE:
                CALL( SP_setup( SP_WAITING_MODE, list->id ) );
                break;
            case HALTED_STATE:
                CALL( SP_setup( SP_SUSPENDED_MODE, list->id ) );
                break;
        }
        
        list = list->next;
    }
    
    CALL( SP_print_header() );
    CALL( SP_print_line() );

    //Give lock
    CALL(list_spinlock_give());

    return;
}

/************************************************************************
    OS_DUMP_MEMORY
        This is routine prints out the memory state
************************************************************************/
void    os_dump_memory( void ){
    
    FTBL *frame_tbl = pFrame;
    FRAME *frame;
    INT32 page, pid;
    INT16 page_entry;

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl != NULL){
    
        while(frame_tbl != NULL){
            if(frame_tbl->frames != NULL){
                frame = frame_tbl->frames;
                CALL(page_entry = os_pcb_list_get_page_table_page(frame->pid, frame->page));
                MP_setup(frame_tbl->frame, frame->pid, frame->page, ((page_entry & 0xE000) >> 13));
            }
            frame_tbl = frame_tbl->next;
        }
    }
    MP_print_line();
    
    //Give lock
    CALL(frame_spinlock_give());

    return;
}

/************************************************************************
    CREATE_PROCESS
        This is routine called by os_init and and creates the routine
        passed to it in a new process 
************************************************************************/

void     create_process( void *funcPtr, const char * name, INT32 priority,
                                INT32 * id, INT32 * error, INT32 mode){

    PCB *process;
    PCB *p_curr;
    INT32 id1,id2,curr_id;
    INT32 i;
    STBL *shadow_table, *tmp;
    
    //error checks...
    if(pTotal >= PROCESS_MAX){
        if(ERROR_DEBUG) printf("You have create the max number of processes!\n");
        (*error) = ERR_Z502_INTERNAL_BUG;
        return;
    }
    
    if(name == NULL){
        if(ERROR_DEBUG) printf("You need a process name!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }
    
    if(strlen(name) > NAME_LEN){
        if(ERROR_DEBUG) printf("Your process name is too long!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }

    CALL(id1 = os_pcb_list_get_id_by_name(name));
    CALL(id2 = os_pcb_queue_get_id_by_name(name));
    if( (id1 > 0) || (id2 > 0) ){
        if(ERROR_DEBUG) printf("A process with that name already exists!!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }

    if(priority < 0 || priority > PRIOR_MAX) {
        if(ERROR_DEBUG) printf("You need to pick a legal priority!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }

    //malloc new PCB
    if((process = malloc(sizeof(PCB))) == NULL){
        if(ERROR_DEBUG) printf("Failed to malloc new PCB!\n");
        (*error) = ERR_Z502_INTERNAL_BUG;
        return;
    }

    //increase process number
    pid++;
    pTotal++;

    //form pcb
    process->id = pid;
    process->state = READY_STATE;
    memset(process->name,0,NAME_LEN+1);
    memcpy(process->name,name,strlen(name));
    process->priority = priority;    
    CALL(curr_id = os_pcb_get_curr_proc_id());
    process->parent = curr_id;
    process->wake_up_time = 0;
    process->num_msgs = 0;
    process->msg_state = MSG_READY_STATE;
    process->msg_rec_len = 0;
    process->msg_rec_id = 0;
    process->outbox = NULL;
    process->inbox = NULL;
    process->page_table = (UINT16 *)calloc( sizeof(UINT16), VIRTUAL_MEM_PGS );
    process->disk_in_use = 0;
    process->sector_in_use = 0;
    process->shadow_table = malloc(sizeof(STBL));
    shadow_table = process->shadow_table;
    shadow_table->page = 0;
    shadow_table->disk = -1;
    shadow_table->sector = -1;
    for(i = 1; i < VIRTUAL_MEM_PGS-1; i++){
        tmp = malloc(sizeof(STBL));
        tmp->page = i;
        tmp->disk = -1;
        tmp->sector = -1;
        shadow_table->next = tmp;
        shadow_table = tmp;
    }
    
    //make context
    ZCALL( Z502_MAKE_CONTEXT( &process->context, (void *)funcPtr, mode ));
    CALL(os_pcb_list_add(process));
    if(PROC_DEBUG) printf("Just created process %s id: %d priority: %d\n", name, pid, priority);

    //set return values
    (*id) = pid;
    (*error) = ERR_SUCCESS;
    
    CALL(os_dump_stats2("CREATE", pid));
   
    //switch to this process if none are running
    CALL(p_curr = os_pcb_list_get_by_id(curr_id));
    if(p_curr == NULL){
        CALL(switch_process(process->id, SWITCH_CONTEXT_SAVE_MODE));
    }else{
        //Switch to this process if its higher priority than current process
        if(process->priority < p_curr->priority){
            if(PROC_DEBUG) printf("switching to created process id: %d\n", process->id);
            CALL(switch_process(process->id, SWITCH_CONTEXT_SAVE_MODE));
        }
    }

    return;
}

/************************************************************************
    SWITCH_PROCESS
        This is routine switches contexts to the PCB of ID passed to it

************************************************************************/

void    switch_process( INT32 id, INT32 mode ){

    PCB *process, *p_curr;
    INT32 curr_id, status;

    //get PCB for process we are switching to
    CALL(process = os_pcb_list_get_by_id(id));
    
    //get pcb of current running process
    CALL(curr_id = os_pcb_get_curr_proc_id());
    CALL(p_curr = os_pcb_list_get_by_id(curr_id));

    if(process == NULL){
        if(ERROR_DEBUG) printf("Cannot switch to process that doesn't exist!\n");
        return;
    }
    
    //set current running process to ready if one exists
    CALL(status = os_pcb_list_get_state_by_id(curr_id));
    if(status == RUNNING_STATE){
        CALL(status = os_pcb_list_set_state_by_id(curr_id, READY_STATE));
    }
    
    //set new process to running
    CALL(status = os_pcb_list_set_state_by_id(id, RUNNING_STATE));
    if(status < 0){
        if(ERROR_DEBUG) printf("Switch Process: setting state of proc we are switching to that doesn't exist\n");
        CALL(os_dump_stats());
        return;
    }

    //set current process to new process
    CALL(os_pcb_set_curr_proc_id(id));
            
    if(PROC_DEBUG) printf("Just set running ID to: %d!\n", process->id);
    //CALL(os_dump_stats());

    ZCALL( Z502_SWITCH_CONTEXT( mode, &process->context ));

}

/************************************************************************
    TERMINATE_PROCESS
        This is routine terminates the PCB of ID passed to it

************************************************************************/

void    terminate_process(INT32 id, INT32 *error){

    PCB *process;
    INT32 high_prior_id, curr_id;

    //Invalid ID
    if(id == 0 || id < -2){
        (*error) = ERR_BAD_PARAM;
        return;
    }
    
    //get pcb of current running process
    CALL(curr_id = os_pcb_get_curr_proc_id());

    //kill calling proc
    if(id == -1){
        id = curr_id;
    }

    //kill calling proc and all children
    if(id == -2){
        CALL(terminate_children(curr_id,error));
        return;
    }

    //get PCB of process we are killing
    CALL(process = os_pcb_list_get_by_id(id));
    if(process == NULL){
        (*error) = ERR_BAD_PARAM;
        return;
    }

    if(PROC_DEBUG) printf("terminating process id %d\n", id);

    //remove process from list and queue 
    pTotal--;
    CALL(os_pcb_list_del(id));
    CALL(os_pcb_queue_del(id));
    if(PROC_DEBUG) printf("process terminated: %d\n", id);
    if(PROC_DEBUG) CALL(os_dump_stats());
    
    CALL(os_dump_stats2("DESTROY", id));

    (*error) = ERR_SUCCESS;
    if(pTotal <= 0){
        //Halt if no processes left
        ZCALL(Z502_HALT());
    }else{
        //Check to see if we killed current id
        if(id == curr_id){
            CALL(high_prior_id = os_pcb_list_get_high_prior_id());
            CALL(process = os_pcb_list_get_by_id(high_prior_id));
            if(process == NULL){
                //if no high prior id then idle, processes are sleeping
                CALL(idle());
                return;
            }else{
                //switch to high prior id
                CALL(switch_process(high_prior_id, SWITCH_CONTEXT_KILL_MODE));
                return;
            }
        }
    
    }

}


/************************************************************************
    TERMINATE_CHILDREN
        Recursively find all children and call terminate_process on them

************************************************************************/
void    terminate_children(INT32 id, INT32 *error){
    
    PCB *process;

    //Invalid ID
    if(id == 0){
        (*error) = ERR_BAD_PARAM;
        return;
    }
   
    //recursively search for children
    process = pList;
    while(process != NULL){
        if(process->parent == id){
            CALL(terminate_children(process->id, error));
        }
        process = process->next;
    }

    CALL(terminate_process(id, error));
    (*error) = ERR_SUCCESS;
}

/************************************************************************
    GET_PROCESS_ID
        This routine gets the process id of the process name
        passed to it

************************************************************************/

void     get_process_id(const char *name, INT32 *id, INT32 *error){
    if(strcmp(name,"") == 0){
        CALL((*id) = os_pcb_get_curr_proc_id());
        (*error) = ERR_SUCCESS;
        return;
    }

    CALL((*id) = os_pcb_list_get_id_by_name(name));
    if( (*id) < 0 ){
        (*id) = 0;
        (*error) = ERR_BAD_PARAM;
    }else{
        (*error) = ERR_SUCCESS;
    }
}

/************************************************************************
    SUSPEND_PROCESS
        This routine suspends the process of the id passed to it

************************************************************************/

void     suspend_process(INT32 id, INT32 *error){
    PCB * process;
    INT32 status;
    INT32 high_prior_id, curr_id;
    
    //get pcb of current running process
    CALL(curr_id = os_pcb_get_curr_proc_id());

    if(id == -1){
        id = curr_id;
    }
    
    if(SUSP_DEBUG) printf("Suspend Process: suspending process id: %d!\n", id);

    //check pcb
    CALL(process = os_pcb_list_get_by_id(id));
    if(process == NULL){
        (*error) = ERR_BAD_PARAM;
        if(SUSP_DEBUG) printf("Suspend Process: cannot find PCB to suspend!!\n");
        return;
    }

    //set current running process to ready if one exists
    CALL(status = os_pcb_list_get_state_by_id(id));
    if(status == HALTED_STATE){
        (*error) = ERR_BAD_PARAM;
        if(SUSP_DEBUG) printf("Suspend Process: process already suspended!\n");
        return;   
    }else if(status == WAITING_STATE){
        if(SUSP_DEBUG) printf("Suspend Process: suspending a sleeping process!\n");
        CALL(os_pcb_queue_del(id));
    }

    //set state
    CALL(status = os_pcb_list_set_state_by_id(id, HALTED_STATE));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(SUSP_DEBUG) printf("Suspend Process: setting state of proc we are suspending doesn't exist\n");
        if(SUSP_DEBUG) CALL(os_dump_stats());
        return;
    }
    
    CALL(os_dump_stats2("SUSPEND", id));

    (*error) = ERR_SUCCESS;
    if(SUSP_DEBUG) CALL(os_dump_stats());
    //now we need to switch to high prior task if we suspended current task
    if(id == curr_id){
        CALL(high_prior_id = os_pcb_list_get_high_prior_id());
        CALL(process = os_pcb_list_get_by_id(high_prior_id));
        if(process == NULL){
            //if no high prior id then idle, processes are sleeping
            CALL(idle());
            return;
        }else{
            //switch to high prior id
            CALL(switch_process(high_prior_id, SWITCH_CONTEXT_SAVE_MODE));
            return;
        }
    }

    return;
}

/************************************************************************
    RESUME_PROCESS
        This routine resumes the process of the id passed to it

************************************************************************/

void     resume_process(INT32 id, INT32 *error){
    PCB *process, *p_high_prior, *p_curr;
    INT32 status;
    INT32 high_prior_id, curr_id;
        
    if(RESU_DEBUG) printf("Resume Process: resuming process id: %d\n", id);
    
    CALL(process = os_pcb_list_get_by_id(id));
    if(process == NULL){
        if(RESU_DEBUG) printf("Resume Process: cannot find PCB to resume!!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }

    //set current running process to ready if one exists
    CALL(status = os_pcb_list_get_state_by_id(id));
    if(status == WAITING_STATE){
        (*error) = ERR_BAD_PARAM;
        if(RESU_DEBUG) printf("Resume Process: cannot resume a sleeping process!\n");
        return;   
    }else if(status !=  HALTED_STATE){
        (*error) = ERR_BAD_PARAM;
        if(RESU_DEBUG) printf("Suspend Process: process is not suspended!\n");
        return;
    }   

    //set state
    CALL(status = os_pcb_list_set_state_by_id(id, READY_STATE));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RESU_DEBUG) printf("Resume Process: setting state of proc we are resuming doesn't exist\n");
        if(RESU_DEBUG) CALL(os_dump_stats());
        return;
    }
    
    CALL(os_dump_stats2("RESUME", id));

    (*error) = ERR_SUCCESS;
    if(RESU_DEBUG) CALL(os_dump_stats());

    //see if anyone can receive now
    CALL(os_pcb_list_check_outbox_for_receivers(id));
    
    //Switch to highest priority task or idle if none
    CALL(switch_to_next_highest_priority());
    return;
}

/************************************************************************
    CHANGE_PRIORITY
        This routine changes the priority of the process of the id 
        passed to it

************************************************************************/

void    change_priority(INT32 id, INT32 priority, INT32 *error){
    PCB *process, *p_high_prior, *p_curr;
    INT32 status;
    INT32 high_prior_id, curr_id;
        
    if(PRIO_DEBUG) printf("Change Priority: changing prior of id: %d\n", id);
    
    if(priority < 0 || priority > PRIOR_MAX) {
        if(PRIO_DEBUG) printf("You need to pick a legal priority!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }
    
    //get pcb of current running process
    CALL(curr_id = os_pcb_get_curr_proc_id());

    if(id == -1){
        id = curr_id;
    }

    CALL(process = os_pcb_list_get_by_id(id));
    if(process == NULL){
        (*error) = ERR_BAD_PARAM;
        return;
    }
  
    //set priority 
    CALL(status = os_pcb_list_set_prior_by_id( id, priority ));
    
    //Give lock
    CALL(list_spinlock_get());
    //Call sort
    CALL(os_pcb_list_sort());
    //Give lock
    CALL(list_spinlock_give());
    
    CALL(os_dump_stats2("SWAPPED", id));

    (*error) = ERR_SUCCESS;
    if(PRIO_DEBUG) CALL(os_dump_stats());
    
    //Switch to highest priority task or idle if none
    CALL(switch_to_next_highest_priority());
    return;
}

/************************************************************************
    SEND MESSAGE
        Send a message to another process

************************************************************************/

void   send_message(INT32 target_id, char* message, INT32 send_length,
                        INT32 *error)
{
    PCB *process;
    INT32 status;
    INT32 curr_id;
    char *buffer;
    
    if(SEND_DEBUG) printf("Send Message: sending message to id: %d\n", target_id);
    
    //check to see if target_id is real
    if(target_id != -1){
        CALL(process = os_pcb_list_get_by_id(target_id));
        if(process == NULL){
            if(SEND_DEBUG) printf("Send message: process doesn't exist id: %d!\n", target_id);
            (*error) = ERR_BAD_PARAM;
            return;
        }
    }

    //check send_len
    if(send_length < 0 || send_length > MSG_LEN_MAX){
        if(SEND_DEBUG) printf("You need to pick a legal message length!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }
    
    //get state, cant send a message with proc that is already sending or receiving
    if(target_id != -1){
        CALL(status = os_pcb_list_get_msg_state_by_id(target_id));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(SEND_DEBUG) printf("Send Message: getting state of proc we are sending with doesn't exist\n");
            //if(SEND_DEBUG) CALL(os_dump_stats());
            return;
        }else if( (status == MSG_REC_STATE) ){
            (*error) = ERR_BAD_PARAM;
            if(SEND_DEBUG) printf("Send Message: trying to send message with proc that is not ready to send\n");
            //if(SEND_DEBUG) CALL(os_dump_stats());
            return;
        }
    }    
    
    //get our ID
    CALL(curr_id = os_pcb_get_curr_proc_id());
    
    CALL(os_dump_stats2("SENDMSG", curr_id));

    //malloc new buffer for message because send length might be longer than message
    buffer = malloc(sizeof(char)*(send_length+1));
    //init buffer to null
    memset(buffer,0,send_length+1);
    //copy message
    memcpy(buffer, message, send_length);

    //send message to outbox
    CALL(status = os_pcb_list_send_msg_to_outbox(target_id, buffer, send_length));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(SEND_DEBUG) printf("Send Message: proc with outbox doesnt exist or outbox full!\n");
        //if(SEND_DEBUG) CALL(os_dump_stats());
        return;
    }
    free(buffer);

    //set state
    CALL(status = os_pcb_list_set_msg_state_by_id(curr_id, MSG_SEND_STATE));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(SEND_DEBUG) printf("Send Message: setting state of proc we are sending with doesn't exist\n");
        //if(SEND_DEBUG) CALL(os_dump_stats());
        return;
    }

    (*error) = ERR_SUCCESS;
    //if(SEND_DEBUG) CALL(os_dump_stats());
    if(SEND_DEBUG) CALL(os_pcb_list_msgs_print());

    //see if anyone can receive now
    CALL(os_pcb_list_check_outbox_for_receivers(curr_id));

    return; 
}

/************************************************************************
    RECEIVE MESSAGE
        Receive a message from another process

************************************************************************/

void   receive_message(INT32 source_id, char* message, INT32 rec_length, 
                       INT32 *send_length, INT32 *sender_id, INT32 *error)
{
    PCB *process;
    INT32 status;
    INT32 curr_id;
    char *buffer;
    
    if(RECV_DEBUG) printf("Receive Message: trying to receive message from id: %d\n", source_id);
    
    //check to see if target_id is real
    if(source_id != -1){
        CALL(process = os_pcb_list_get_by_id(source_id));
        if(process == NULL){
            if(RECV_DEBUG) printf("Receive message: process doesn't exist id: %d!\n", source_id);
            (*error) = ERR_BAD_PARAM;
            return;
        }
    }

    //check send_len
    if(rec_length < 0 || rec_length > MSG_LEN_MAX){
        if(RECV_DEBUG) printf("You need to pick a legal message length!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());
    
    CALL(os_dump_stats2("RECVMSG", curr_id));

    //set our message rec length
    CALL(status = os_pcb_list_set_msg_rec_len_by_id(curr_id, rec_length));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Receive Message: cant set our message rec length\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }

    //set our message rec id
    CALL(status = os_pcb_list_set_msg_rec_id_by_id(curr_id, source_id ));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Receive Message: cant set our message rec id\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }

    //match us up with a sender!
    if(source_id == -1){
        //set our receive state
        CALL(status = os_pcb_list_set_msg_state_by_id(curr_id, MSG_REC_ALL_STATE));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(RECV_DEBUG) printf("Receive Message: cant set our rec all state\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            return;
        }
        //we are broadcasting a recieve
        CALL(status = os_pcb_list_get_send_broadcast_id(curr_id));
        if(status < 0){
            //No one is sending to us, suspend and wait for sender
            (*error) = ERR_SUCCESS;
            if(RECV_DEBUG) printf("Receive Message: no one broadcasting to us, suspending self!\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            if(RECV_DEBUG) CALL(os_pcb_list_msgs_print());
            CALL(suspend_process(curr_id, error));
            CALL(status = os_pcb_list_get_last_msg_from_inbox(curr_id, sender_id, message, send_length));
            if(status < 0){
                (*error) = ERR_BAD_PARAM;
                if(RECV_DEBUG) printf("Receive Message: could not get last msg in inbox!\n");
                //if(RECV_DEBUG) CALL(os_dump_stats());
                return;
            }
            return;
        }else{
            (*sender_id) = status;
        }
    }else{
        //set our receive state
        if(source_id != curr_id){
            CALL(status = os_pcb_list_set_msg_state_by_id(curr_id, MSG_REC_STATE));
            if(status < 0){
                (*error) = ERR_BAD_PARAM;
                if(RECV_DEBUG) printf("Receive Message: cant set our rec state\n");
                //if(RECV_DEBUG) CALL(os_dump_stats());
                return;
            }
        }
        //we are receiving from another specific process
        CALL(status = os_pcb_list_get_msg_state_by_id(source_id));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(RECV_DEBUG) printf("Receive Message: getting state of proc we are sending with doesn't exist\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            return;
        }else if(status != MSG_SEND_STATE){
            //No one is sending to us, suspend and wait for sender
            (*error) = ERR_SUCCESS;
            if(RECV_DEBUG) printf("Receive Message: no one sending to us, suspending self!\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            if(RECV_DEBUG) CALL(os_pcb_list_msgs_print());
            CALL(suspend_process(curr_id, error));
            CALL(status = os_pcb_list_get_last_msg_from_inbox(curr_id, sender_id, message, send_length));
            if(status < 0){
                (*error) = ERR_BAD_PARAM;
                if(RECV_DEBUG) printf("Receive Message: could not get last msg in inbox!\n");
                //if(RECV_DEBUG) CALL(os_dump_stats());
                return;
            }
            return;
        }else{
            (*sender_id) = source_id;
        }    
    }

    CALL(message_transfer((*sender_id), curr_id, rec_length, send_length, error));
    //If error < 0 then that means there was no message to get, we should suspend
    if(error < 0){
        (*error) = ERR_SUCCESS;
        if(RECV_DEBUG) printf("Receive Message: suspending ID: %d!\n", curr_id);
        //if(RECV_DEBUG) CALL(os_dump_stats());
        if(RECV_DEBUG) CALL(os_pcb_list_msgs_print());
        CALL(suspend_process(curr_id, error));
        CALL(status = os_pcb_list_get_last_msg_from_inbox(curr_id, sender_id, message, send_length));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(RECV_DEBUG) printf("Receive Message: could not get last msg in inbox!\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            return;
        }
        return;
    }else{
        CALL(status = os_pcb_list_get_last_msg_from_inbox(curr_id, sender_id, message, send_length));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(RECV_DEBUG) printf("Receive Message: could not get last msg in inbox!\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            return;
        }
        return;
    }
 
    //if(RECV_DEBUG) CALL(os_dump_stats());
    if(RECV_DEBUG) CALL(os_pcb_list_msgs_print());
    return; 
}

void   message_transfer(INT32 sender_id, INT32 rec_id, INT32 rec_length, INT32 *send_length, INT32 *error){

    PCB *process;
    INT32 status;
    char *buffer;
    
    if(RECV_DEBUG) printf("Transfer Message: id %d is receiving message from id: %d\n", rec_id, sender_id);

    //malloc new buffer for message because send length might be longer than message
    buffer = malloc(sizeof(char)*(rec_length+1));
    //init buffer to null
    memset(buffer,0,rec_length+1);
    
    //copy msg to buffer
    CALL(status = os_pcb_list_get_msg_from_outbox(rec_id, sender_id, buffer, rec_length));
    if(status < 0){
//        (*error) = ERR_BAD_PARAM;
        (*error) = -1;
        if(RECV_DEBUG) printf("Transfer Message: cant get message in outbox of receiver\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }else{
        (*send_length) = status;
    }
    
    CALL(status = os_pcb_list_put_msg_in_inbox(rec_id, sender_id, buffer, (*send_length)));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Transfer Message: cant put message in inbox\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }
    if(RECV_DEBUG) printf("Transfer Message: received message from id: %d, len %d, msg: %s\n",
                          sender_id, (*send_length), buffer);
    free(buffer);

    //we received successfully, set us back to ready state
    CALL(status = os_pcb_list_set_msg_state_by_id(rec_id, MSG_READY_STATE));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Transfer Message: setting state of recv proc doesn't exist\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }
    //clear our message rec length
    CALL(status = os_pcb_list_set_msg_rec_len_by_id(rec_id, 0));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Transfer Message: cant clear our message rec length\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }
    //clear our message rec id
    CALL(status = os_pcb_list_set_msg_rec_id_by_id(rec_id, 0 ));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Transfer Message: cant clear our message rec id\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }
    //set state of sender back to ready if it has no messages in outbox
    CALL(status = os_pcb_list_get_msg_num_by_id(sender_id));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Transfer Message: cant get number of messages in outbox of receiver\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }else if(status == 0){
        //only get to ready if its not sending
        CALL(status = os_pcb_list_get_msg_state_by_id(sender_id));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(RECV_DEBUG) printf("Transfer Message: cant get msg state of sender\n");
            //if(RECV_DEBUG) CALL(os_dump_stats());
            return;
        }else if(status == MSG_SEND_STATE || status == MSG_SEND_ALL_STATE){
            //if sender has no messages left we can set him back to ready state
            CALL(status = os_pcb_list_set_msg_state_by_id(sender_id, MSG_READY_STATE));
            if(status < 0){
                (*error) = ERR_BAD_PARAM;
                if(RECV_DEBUG) printf("Transfer Message: setting state of proc we recv from doesn't exist\n");
                //if(RECV_DEBUG) CALL(os_dump_stats());
                return;
            }
        }
    }
    //set state of rec process back to ready if it was suspended
    CALL(status = os_pcb_list_get_state_by_id(rec_id));
    if(status < 0){
        (*error) = ERR_BAD_PARAM;
        if(RECV_DEBUG) printf("Transfer Message: could not get sender's state!\n");
        //if(RECV_DEBUG) CALL(os_dump_stats());
        return;
    }else if(status == HALTED_STATE){
        if(RECV_DEBUG) printf("Transfer Message: resuming ID: %d!\n", rec_id);
        //CALL(resume_process(rec_id, error));
        //set state
        CALL(status = os_pcb_list_set_state_by_id(rec_id, READY_STATE));
        if(status < 0){
            (*error) = ERR_BAD_PARAM;
            if(RESU_DEBUG) printf("Transfer Message: setting state of proc we are resuming doesn't exist\n");
            if(RESU_DEBUG) CALL(os_dump_stats());
            return;
        }
    }

    (*error) = ERR_SUCCESS;
    return;
}

/************************************************************************
    MEM READ
        Read from memory at specified address
        This gets skipped in the SVC but it is called by the fault handler        

************************************************************************/
void mem_read(INT32 addr, INT32 *data){

    INT32 page;
    INT32 offset;
    INT32 frame;
    INT32 curr_id;
    
    if(MEM_DEBUG) printf("Reading from addr: %d!\n", addr);
    
    ZCALL(MEM_READ(addr,data));
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());
    
    //Get page from addr
    CALL(os_frame_addr_to_page(addr, &page, &offset));

    //Get frame where page exists
    CALL(frame = os_frame_get_frame_by_page(page, curr_id));

    //Touch frame so we know when it was used last
    CALL(os_frame_touch_frame(frame, curr_id));
}

/************************************************************************
    MEM WRITE
        Write to memory at specified address        
        This gets skipped in the SVC but it is called by the fault handler        

************************************************************************/
void mem_write(INT32 addr, INT32 *data){
    
    INT32 page;
    INT32 offset;
    INT32 frame;
    INT32 curr_id;
    
    if(MEM_DEBUG) printf("Writing %d to addr: %d!\n", *data, addr);
    
    ZCALL(MEM_WRITE(addr,data));
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());
    
    //Get page from addr
    CALL(os_frame_addr_to_page(addr, &page, &offset));

    //Get frame where page exists
    CALL(frame = os_frame_get_frame_by_page(page, curr_id));

    //Touch frame so we know when it was used last
    CALL(os_frame_touch_frame(frame, curr_id));
}

/************************************************************************
    DISK READ
        Read from disk with specified ID and sector

************************************************************************/
void disk_read(INT16 disk_id, INT16 sector, char data[PGSIZE]){

    INT32 read = 0;
    INT32 start = 0;
    INT32 status = 0;
    INT32 error = 0;
    INT32 curr_id = 0;

    //Check disk_id
    if(disk_id < 1 || disk_id > MAX_NUMBER_OF_DISKS){
        if(DISK_DEBUG) printf("disk_read: not a valid disk id!\n");
        return;
    }
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());

    if(DISK_DEBUG) printf("disk_read: reading from disk %d sector %d!\n", disk_id, sector);

    //Set disk ID
    ZCALL(MEM_WRITE(Z502DiskSetID, (INT32 *)&disk_id));
    
    //Read status
    ZCALL(MEM_READ( Z502DiskStatus, &status));
    if ( status != DEVICE_FREE ){
        if(DISK_DEBUG) printf( "This disk is busy! Waiting for it to be free\n" );
        while(status != DEVICE_FREE){
            ZCALL(MEM_READ( Z502DiskStatus, &status));
        }
    }

    //Set disk ID
    ZCALL(MEM_WRITE(Z502DiskSetSector, (INT32 *)&sector));

    //Set buffer to put information read
    ZCALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)data));

    //Set action
    ZCALL(MEM_WRITE(Z502DiskSetAction, &read));
    
    //Start Disk
    ZCALL(MEM_WRITE(Z502DiskStart, &start));
    
    //set disk use flag in pcb
    CALL(status = os_pcb_list_set_disk_in_use_by_id(curr_id, disk_id));
    if(status < 0){
        if(DISK_DEBUG) printf("disk_read: cant set disk in use flag\n");
        return;
    }
    //set sector in use flag in pcb
    CALL(status  = os_pcb_list_set_sector_in_use_by_id(curr_id, sector));
    if(status < 0){
        if(DISK_DEBUG) printf("disk_read: cant set sector in use flag\n");
        return;
    }
    
    CALL(os_dump_stats2("DISKREAD", curr_id));
            
    //Suspend and wait to finish
    if(DISK_DEBUG) printf("disk_read: suspending self and waiting for read!\n");
    CALL(suspend_process(curr_id, &error));

    return;
}

/************************************************************************
    DISK WRITE
        Read from disk with specified ID and sector

************************************************************************/
void disk_write(INT16 disk_id, INT16 sector, char data[PGSIZE]){

    INT32 write = 1;
    INT32 start = 0;
    INT32 status = 0;
    INT32 error = 0;
    INT32 curr_id = 0;
    INT32 i = 0;
    INT32 *temp;

    //Check disk_id
    if(disk_id < 1 || disk_id > MAX_NUMBER_OF_DISKS){
        if(DISK_DEBUG) printf("disk_write: not a valid disk id!\n");
        return;
    }
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());

    if(DISK_DEBUG) printf("disk_write: writing: ");
    for(i = 0; i < PGSIZE; i++){
        //if(DISK_DEBUG) printf("%X",data[i]);
    }
    temp = (INT32 *)data;
    for(i = 0; i < 4; i++){
        if(DISK_DEBUG) printf(" %d ",temp[i]);
    }
    
    if(DISK_DEBUG) printf(" to disk %d sector %d!\n", disk_id, sector);

    //Set disk ID
    ZCALL(MEM_WRITE(Z502DiskSetID, (INT32 *)&disk_id));
    
    //Read status
    ZCALL(MEM_READ( Z502DiskStatus, &status));
    if ( status != DEVICE_FREE ){
        if(DISK_DEBUG) printf( "This disk is busy!\n" );
    }

    //Set disk ID
    ZCALL(MEM_WRITE(Z502DiskSetSector, (INT32 *)&sector));

    //Set buffer to put information read
    ZCALL(MEM_WRITE(Z502DiskSetBuffer, (INT32 *)data));

    //Set action
    ZCALL(MEM_WRITE(Z502DiskSetAction, &write));
    
    //Start Disk
    ZCALL(MEM_WRITE(Z502DiskStart, &start));

    //Set disk bit map to occupied
    CALL(os_disk_set_sector(disk_id, sector, 1));
    
    //set disk use flag in pcb
    CALL(status = os_pcb_list_set_disk_in_use_by_id(curr_id, disk_id));
    if(status < 0){
        if(DISK_DEBUG) printf("disk_write: cant set disk in use flag\n");
        return;
    }
    //set sector in use flag in pcb
    CALL(status  = os_pcb_list_set_sector_in_use_by_id(curr_id, sector));
    if(status < 0){
        if(DISK_DEBUG) printf("disk_write: cant set sector in use flag\n");
        return;
    }
    
    CALL(os_dump_stats2("DISKWRIT", curr_id));
            
    //Suspend and wait to finish
    if(DISK_DEBUG) printf("disk_write: suspending self and waiting for write!\n");
    CALL(suspend_process(curr_id, &error));
 
    return;
}

/************************************************************************
    DEFINE SHARED AREA
        This function defines a shared area of memory that 
        can be used by multiple processes

************************************************************************/

void    define_shared_area( INT32 starting_addr, INT32 pages, char area_tag[MAX_TAG_LENGTH], INT32 *num_sharers, INT32 *error){
    
    INT32 curr_id = 0;
    INT32 starting_page, offset;
    INT32 frame;
    INT32 i;
    UINT16 page_entry = 0;
    INT32 exists = 0;

    //Check starting addr
    if(starting_addr < 0 || starting_addr > (VIRTUAL_MEM_PGS*PGSIZE)){
        if(ERROR_DEBUG) printf("define_shared_area: bad starting address!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }

    //Check number of virtual pages
    if(pages > VIRTUAL_MEM_PGS){
        if(ERROR_DEBUG) printf("define_shared_area: bad number of pages!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }

    //Check to see if shared area goes over end of virtual memory
    if( (starting_addr + pages*PGSIZE) > (VIRTUAL_MEM_PGS*PGSIZE)){
        if(ERROR_DEBUG) printf("define_shared_area: shared area exceeds end of virtual memory!\n");
        (*error) = ERR_BAD_PARAM;
        return;
    }
    
    //get current proc id
    CALL(curr_id = os_pcb_get_curr_proc_id());

    //get starting frame
    CALL(os_frame_addr_to_page(starting_addr, &starting_page, &offset));

    //see if this tag already exists
    CALL(frame = os_frame_get_next_tagged_frame(0, area_tag));
    if(frame != -1){
        exists = 1;
    }

    //printf("DEFINE SHARED AREA pID %d start addr %d end addr %d for %d pages\n", curr_id, starting_addr, (starting_addr+(pages*PGSIZE)), pages);

    //Add this proc as a sharer of this shared memory
    for(i = starting_page; i < starting_page + pages; i++){

        //Get next emtpy frame
        if(exists == 0){
            CALL(frame = os_frame_get_next_empty_frame());
        }else{
            CALL(frame = os_frame_get_next_tagged_frame(frame, area_tag));
        }

        //Set tag to shared frame
        CALL(os_frame_set_tag(frame, area_tag));
            
        //Set frame to shared by this proc
        CALL((*num_sharers) = os_frame_set_page(frame, i, curr_id, area_tag));

        //Touch frame so we know when it was used last
        CALL(os_frame_touch_frame(frame, curr_id));

        //Set page to valid for this proc
        page_entry = frame;
        page_entry |= PTBL_VALID_BIT;
        CALL(os_pcb_list_set_page_table_page(curr_id, i, page_entry));
        frame++;
    }
    //printf("there were %d previous sharers of this area\n", *num_sharers);
    CALL(os_frame_print());
        
    (*error) = ERR_SUCCESS;

    return;
}

/************************************************************************
    OS Spin Lock Operations
        These routines are used for all spin lock operations

************************************************************************/
void    DoLock( INT32 offset )
{
    INT32     LockResult;
    if(LOCK_DEBUG) printf( "      Thread 2 - about to do a lock\n");
    Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE+offset, DO_LOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
    if(LOCK_DEBUG) printf( "      Thread 2 Lock:  %s\n", &(GreatSuccess[ SPART * LockResult ]) );
    //DestroyThread( 0 );
}
void    DoTrylock( INT32 offset )
{
    INT32     LockResult;
    Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE+offset, DO_LOCK, DO_NOT_SUSPEND, &LockResult );
    if(LOCK_DEBUG) printf( "      Thread 2 TryLock:  %s\n", &(GreatSuccess[ SPART * LockResult ]) );
    //DestroyThread( 0 );
}
void    DoUnlock( INT32 offset )
{
    INT32     LockResult;
    Z502_READ_MODIFY( MEMORY_INTERLOCK_BASE+offset, DO_UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult );
    if(LOCK_DEBUG) printf( "      Thread 2 UnLock:  %s\n", &(GreatSuccess[ SPART * LockResult ]) );
    //DestroyThread( 0 );
}

void   list_spinlock_get(void){
    INT32 LockResult;
    #if LIST_LOCK_ON == 1
        #if ONE_LIST_LOCK_ON == 1
        DoLock(0);
        #else
        DoLock(1);
        #endif
    #endif
}

void    list_spinlock_give(void){
    INT32 LockResult;
    #if LIST_LOCK_ON == 1
        #if ONE_LIST_LOCK_ON == 1
        DoUnlock(0);
        #else
        DoUnlock(1);
        #endif
    #endif
}

void   queue_spinlock_get(void){
    INT32 LockResult;
    #if LIST_LOCK_ON == 1
        #if ONE_LIST_LOCK_ON == 1
        DoLock(0);
        #else
        DoLock(2);
        #endif
    #endif
}

void    queue_spinlock_give(void){
    INT32 LockResult;
    #if LIST_LOCK_ON == 1
        #if ONE_LIST_LOCK_ON == 1
        DoUnlock(0);
        #else
        DoUnlock(2);
        #endif
    #endif
}

void   frame_spinlock_get(void){
    INT32 LockResult;
    #if FRAME_LOCK_ON == 1
    DoLock(3);
    #endif
}

void    frame_spinlock_give(void){
    INT32 LockResult;
    #if FRAME_LOCK_ON == 1
    DoUnlock(3);
    #endif
}

void   event_spinlock_get(void){
    INT32 LockResult;
    #if EVNT_LOCK_ON == 1
    DoLock(4);
    #endif
}

void    event_spinlock_give(void){
    INT32 LockResult;
    #if EVNT_LOCK_ON == 1
    DoUnlock(4);
    #endif
}

void   timer_spinlock_get(void){
    INT32 LockResult;
    #if TIMER_LOCK_ON == 1
    DoLock(5);
    #endif
}

void    timer_spinlock_give(void){
    INT32 LockResult;
    #if TIMER_LOCK_ON == 1
    DoUnlock(5);
    #endif
}

void   disk_spinlock_get(void){
    INT32 LockResult;
    #if DISK_LOCK_ON == 1
    DoLock(6);
    #endif
}

void    disk_spinlock_give(void){
    INT32 LockResult;
    #if DISK_LOCK_ON == 1
    DoUnlock(6);
    #endif
}


/************************************************************************
    OS List Operations
        These routines are used for all operations to the ready list

************************************************************************/

//adds pcb to list
void    os_pcb_list_add( PCB *pcb ){

    PCB *list = pList;

    //Get lock
    CALL(list_spinlock_get());

    //Check if list has been initiatilzed
    if(list == NULL){
        pList = pcb;
        pcb->prev = NULL;
        pcb->next = NULL;
        CALL(list_spinlock_give());
        return;
    }

    //Find the end of the list
    while(list->next != NULL){
        list = list->next;
    }
    
    list->next = pcb;
    pcb->prev = list;
    pcb->next = NULL;

    //Call sort
    CALL(os_pcb_list_sort());
    
    //Give lock
    CALL(list_spinlock_give());
    
    return;
}

//deletes pcb from list
void    os_pcb_list_del( INT32 id ){

    PCB *list = pList;
    PCB *tmp;
    PCB *del;

    CALL(del = os_pcb_list_get_by_id(id));
    
    //Get lock
    CALL(list_spinlock_get());

    if(del == NULL){
        CALL(list_spinlock_give());
        return;
    }
    
    tmp = del->prev;
    if(tmp != NULL){
        tmp->next = del->next;
    }else{
        pList = del->next;
    }
    tmp = del->next;
    if(tmp != NULL){
        tmp->prev = del->prev;
    }

    //del->next = NULL;
    //del->prev = NULL;
    free(del);
    
    //Give lock
    CALL(list_spinlock_give());

    return;
}

//gets pcb by id and returns a pointer to it
PCB     *os_pcb_list_get_by_id( INT32 id ){

    PCB *list = pList;

    //Get lock
    CALL(list_spinlock_get());
    
    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return NULL;
    }

    //Find the end of the list
    while(list != NULL){
        if(list->id == id){
            //Give lock
            CALL(list_spinlock_give());
            return list;
        }
        list = list->next;
    }

    //Give lock
    CALL(list_spinlock_give());

    return NULL;
}

//gets pcb by name and returns pointer to it
PCB     *os_pcb_list_get_by_name( const char * name ){

    PCB *list = pList;
    
    //Get lock
    CALL(list_spinlock_get());

    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return NULL;
    }

    //Find the end of the list
    while(list != NULL){
        if(strcmp(list->name,name) == 0){
            //Give lock
            CALL(list_spinlock_give());
            return list;
        }
        list = list->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return NULL;
}

//gets ID of a pcb by name and returns it
INT32    os_pcb_list_get_id_by_name( const char * name ){

    PCB *list = pList;
    
    //Get lock
    CALL(list_spinlock_get());
    
    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return -1;
    }

    //Find the end of the list
    while(list != NULL){
        if(strcmp(list->name,name) == 0){
            //Give lock
            CALL(list_spinlock_give());
            return list->id;
        }
        list = list->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return -1;
}

//gets state of a pcb by id and returns it
INT32    os_pcb_list_get_state_by_id( INT32 id ){

    PCB *process;
    INT32 state;

    CALL(process = os_pcb_list_get_by_id(id));
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        state = -1;
    }else{
        state = process->state;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return state;
}

//sets the state of pcb id
INT32    os_pcb_list_set_state_by_id( INT32 id, INT32 state ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->state = state;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//get number of messages of pcb id
INT32    os_pcb_list_get_msg_num_by_id( INT32 id ){

    PCB *process;
    INT32 num;

    CALL(process = os_pcb_list_get_by_id(id));
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        num = -1;
    }else{
        num = process->num_msgs;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return num;
}

//get the message state of pcb id
INT32    os_pcb_list_get_msg_state_by_id( INT32 id ){

    PCB *process;
    INT32 state;

    CALL(process = os_pcb_list_get_by_id(id));
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        state = -1;
    }else{
        state = process->msg_state;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return state;
}

//sets the pcb message state of id
INT32    os_pcb_list_set_msg_state_by_id( INT32 id, INT32 state ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->msg_state = state;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//gets the pcb message rec len of id
INT32    os_pcb_list_get_msg_rec_len_by_id( INT32 id ){

    PCB *process;
    INT32 len;

    CALL(process = os_pcb_list_get_by_id(id));
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        len = -1;
    }else{
        len = process->msg_rec_len;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return len;
}

//sets the pcb message rec len of id
INT32    os_pcb_list_set_msg_rec_len_by_id( INT32 id, INT32 len ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->msg_rec_len = len;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//gets the pcb message rec id of id
INT32    os_pcb_list_get_msg_rec_id_by_id( INT32 id ){

    PCB *process;
    INT32 ret;

    CALL(process = os_pcb_list_get_by_id(id));
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        ret = process->msg_rec_id;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//sets the pcb message rec id of id
INT32    os_pcb_list_set_msg_rec_id_by_id( INT32 id, INT32 rec_id ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->msg_rec_id = rec_id;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//get message with msg from outbox of outbox_id that matches len and rec_id
INT32  os_pcb_list_get_msg_from_outbox(INT32 rec_id, INT32 outbox_id, char *msg, INT32 len){

    PCB *process;
    INT32 ret = -1;
    MSG *tmp, *prev;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(outbox_id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        tmp = process->outbox;
        prev = NULL;
        while(tmp != NULL){
            if( (tmp->dest_id == rec_id) ||
                (tmp->dest_id == -1) ) {
                if(tmp->length <= len){
                    process->num_msgs--;
                    if(prev == NULL){
                        process->outbox = tmp->next;
                    }else{
                        prev->next = tmp->next;
                    }
                    memcpy(msg, tmp->message, len);
                    ret = tmp->length;
                    break; 
                }
            }
            prev = tmp;
            tmp = tmp->next;
        }
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//put a message on the outbox of pcb id
INT32  os_pcb_list_send_msg_to_outbox(INT32 id, char *msg, INT32 len){

    PCB *process;
    INT32 ret;
    INT32 curr_id;
    MSG *message, *tmp;
    
    //get pcb of current running process
    CALL(curr_id = os_pcb_get_curr_proc_id());
    CALL(process = os_pcb_list_get_by_id(curr_id));

    //make message
    message = malloc(sizeof(MSG));
    message->dest_id = id;
    message->length = len;
    memcpy(message->message, msg, len);
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        if(process->num_msgs >= MSG_OUTBOX_MAX){
            ret = -1;
        }else{
            process->num_msgs++;
            tmp = process->outbox;
            if(tmp == NULL){
                process->outbox = message;
            }else{
                while(tmp->next != NULL){
                    tmp = tmp->next;
                }
                tmp->next = message;
                message->next = NULL;
            } 
            ret = 0;
        }
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//put message in inbox of pcb rec_id
INT32  os_pcb_list_put_msg_in_inbox(INT32 rec_id, INT32 sender_id, char *msg, INT32 len){

    PCB *process;
    INT32 ret;
    MSG *message, *tmp;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(rec_id));

    //make message
    message = malloc(sizeof(MSG));
    message->dest_id = sender_id;
    message->length = len;
    memcpy(message->message, msg, len);
        
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        tmp = process->inbox;
        if(tmp == NULL){
            process->inbox = message;
        }else{
            while(tmp->next != NULL){
                tmp = tmp->next;
            }
            tmp->next = message;
            message->next = NULL;
        } 
        ret = 0;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//get last received message of inbox from pcb id
INT32  os_pcb_list_get_last_msg_from_inbox(INT32 id, INT32 *send_id, char *msg, INT32 *len){

    PCB *process;
    INT32 ret;
    INT32 curr_id;
    MSG *tmp;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        tmp = process->inbox;
        if(tmp == NULL){
            ret = -1;
        }else{
            while(tmp->next != NULL){
                tmp = tmp->next;
            }
            //This is the last message
            (*send_id) = tmp->dest_id;
            (*len) = tmp->length;
            memcpy(msg,tmp->message,(*len));
            ret = 0;
        } 
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//look at each message in the outbox of pcb id and see if anyone is ready to receive it,
//if so, transfer that message to their inbox
INT32  os_pcb_list_check_outbox_for_receivers(INT32 id){

    PCB *process;
    INT32 ret = -1;
    INT32 curr_id, dest_id, dest_state, rec_length, send_length, error;
    MSG *tmp, sending;
    
    //get pcb of current running process
    CALL(curr_id = os_pcb_get_curr_proc_id());
    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }

    //Get lock
    CALL(list_spinlock_get());
    
    tmp = process->outbox;

    while(tmp != NULL){
        dest_id = tmp->dest_id;

        //Give lock
        CALL(list_spinlock_give());
        
        //if we are broadcasting then look for a receiver
        if(dest_id == -1){
            CALL(dest_id = os_pcb_list_get_rec_broadcast_id(id));
        }
        
        CALL(rec_length = os_pcb_list_get_msg_rec_len_by_id(dest_id));
        
        //Get state of dest id
        CALL(dest_state = os_pcb_list_get_msg_state_by_id(dest_id));

        if(dest_state == MSG_REC_STATE || dest_state == MSG_REC_ALL_STATE){
            //do message transfer
            CALL(message_transfer(id, dest_id, rec_length, &send_length, &error));
        }
        
        //Get lock
        CALL(list_spinlock_get());

        tmp = tmp->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//get page table of pcb id
UINT16*  os_pcb_list_get_page_table_by_id( INT32 id ){

    PCB *process;
    UINT16 *tbl;

    CALL(process = os_pcb_list_get_by_id(id));
    
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        tbl = NULL;
    }else{
        tbl = process->page_table;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return tbl;
}

//get wakeup time of pcb id
INT32    os_pcb_list_get_wakeup_by_id( INT32 id ){

    PCB *process;
    INT32 wakeup;

    CALL(process = os_pcb_list_get_by_id(id));
    
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        wakeup =  -1;
    }else{
        wakeup = process->wake_up_time;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return wakeup;
}

//set wakeup time of pcb id
INT32    os_pcb_list_set_wakeup_by_id( INT32 id, INT32 wakeup ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->wake_up_time = wakeup;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//get priority of pcb id
INT32    os_pcb_list_get_prior_by_id( INT32 id ){

    PCB *process;
    INT32 prior;

    CALL(process = os_pcb_list_get_by_id(id));
    
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        prior =  -1;
    }else{
        prior = process->priority;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return prior;
}

//set priority of pcb id
INT32    os_pcb_list_set_prior_by_id( INT32 id, INT32 prior ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->priority = prior;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//get id that is using disk disk
INT32    os_pcb_list_get_id_by_disk_in_use( INT32 disk ){

    PCB *process = pList;
    INT32 id;
    
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        disk =  -1;
    }else{
        while(process != NULL){
            if(process->disk_in_use == disk){
                id = process->id;
                break;
            }
            process = process->next;
        }
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return id;
}

//get disk in use of pcb id
INT32    os_pcb_list_get_disk_in_use_by_id( INT32 id ){

    PCB *process;
    INT32 disk;

    CALL(process = os_pcb_list_get_by_id(id));
    
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        disk =  -1;
    }else{
        disk = process->disk_in_use;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return disk;
}

//set disk in use of pcb id
INT32    os_pcb_list_set_disk_in_use_by_id( INT32 id, INT32 disk ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->disk_in_use = disk;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//get sector in use of pcb id
INT32    os_pcb_list_get_sector_in_use_by_id( INT32 id ){

    PCB *process;
    INT32 sector;

    CALL(process = os_pcb_list_get_by_id(id));
    
    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        sector =  -1;
    }else{
        sector = process->sector_in_use;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return sector;
}

//set sector in use of pcb id
INT32    os_pcb_list_set_sector_in_use_by_id( INT32 id, INT32 sector ){

    PCB *process;

    CALL(process = os_pcb_list_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(list_spinlock_get());
       
        process->sector_in_use = sector;
 
        //Give lock
        CALL(list_spinlock_give());
        return 0;
    }

    return -1;
}

//set shadow table page of id
INT32    os_pcb_list_set_shadow_table_page(INT32 page, INT32 disk, INT32 sector, INT32 id){

    PCB *process;
    INT32 ret = -1;
    STBL *tmp;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        tmp = process->shadow_table;
        while(tmp->next != NULL){
            if(tmp->page == page){
                tmp->disk = disk;
                tmp->sector = sector;
                ret = 0;
                break;
            }
            tmp = tmp->next;
        }
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//get shadow table page of id
INT32    os_pcb_list_get_shadow_table_page(INT32 page, INT32 *disk, INT32 *sector, INT32 id){

    PCB *process;
    INT32 ret = -1;
    STBL *tmp;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        tmp = process->shadow_table;
        while(tmp->next != NULL){
            if( (tmp->page == page) &&
                (tmp->disk != -1) &&
                (tmp->sector != -1) ){
                (*disk) = tmp->disk;
                (*sector) = tmp->sector;
                ret = 0;
                break;
            }
            tmp = tmp->next;
        }
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//get shadow table page by sector and disk in use
INT32    os_pcb_list_get_shadow_table_page_by_sector(INT32 *page, INT32 disk, INT32 sector, INT32 id){

    PCB *process;
    INT32 ret = -1;
    STBL *tmp;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        tmp = process->shadow_table;
        while(tmp->next != NULL){
            if( (tmp->disk == disk) &&
                (tmp->sector == sector) ){
                (*page) = tmp->page;
                ret = 0;
                break;
            }
            tmp = tmp->next;
        }
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//set page table of id
INT32    os_pcb_list_set_page_table_page(INT32 id, INT32 page, INT32 val){

    PCB *process;
    INT32 ret = -1;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        process->page_table[page] = val;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//get page table of id
INT32    os_pcb_list_get_page_table_page(INT32 id, INT32 page){

    PCB *process;
    INT32 ret = -1;
    STBL *tmp;
    
    //get pcb of current running process
    CALL(process = os_pcb_list_get_by_id(id));

    //Get lock
    CALL(list_spinlock_get());
    
    if(process == NULL){
        ret = -1;
    }else{
        ret = process->page_table[page];
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return ret;
}

//print out list of pcbs
void os_pcb_list_print(){
    
    PCB *list = pList;
    
    //Get lock
    CALL(list_spinlock_get());
    
    printf("List:\n");
    printf("Name             ID  Priority State    Parent WakeUp     MsgState NumMsgs RecLen RecID\n");
    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return;
    }

    //Find the end of the list
    while(list != NULL){
        /*printf("%-16s %-3d %-3d      %-2d    %-3d    %-10d %-2d       %-3d     %-3d    %-3d\n",
                list->name,list->id,list->priority,list->state,list->parent,
                list->wake_up_time, list->msg_state, list->num_msgs,
                list->msg_rec_len, list->msg_rec_id);*/
        printf("%-16s %-3d %-3d      ",list->name,list->id,list->priority);
        switch(list->state){
            case RUNNING_STATE:
                printf("RUNNING  ");
                break;
            case READY_STATE:
                printf("READY    ");
                break;
            case WAITING_STATE:
                printf("SLEEPING ");
                break;
            case HALTED_STATE:
                printf("HALTED   ");
                break;
        }
        printf("%-3d    %-10d ", list->parent,list->wake_up_time);
        switch(list->msg_state){
            case MSG_SEND_STATE:
                printf("SEND     ");
                break;
            case MSG_SEND_ALL_STATE:
                printf("SEND ANY ");
                break;
            case MSG_REC_STATE:
                printf("RECV     ");
                break;
            case MSG_REC_ALL_STATE:
                printf("RECV ANY ");
                break;
            case MSG_READY_STATE:
                printf("READY    ");
                break;
        }
        printf("%-3d     %-3d    %-3d\n",list->num_msgs,list->msg_rec_len, list->msg_rec_id);
        /*printf("%-3d    %-10d %-2d       %-3d     %-3d    %-3d\n",
                list->parent,list->wake_up_time, list->msg_state,
                list->num_msgs,list->msg_rec_len, list->msg_rec_id);*/
        list = list->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return;
}

//print out inbox and outbox of each message
void os_pcb_list_msgs_print(){
    
    PCB *list = pList;
    MSG *messages;
    
    //Get lock
    CALL(list_spinlock_get());
    
    printf("List:\n");
    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return;
    }

    //Find the end of the list
    while(list != NULL){
        printf("Messages for Proc: %s ID: %d\n",list->name,list->id);
        messages = list->outbox;
        printf("\tOutbox:\n");
        printf("\tDestID Length Message\n");
        while(messages != NULL){
            printf("\t%-3d    %-3d    %s\n",messages->dest_id,messages->length,messages->message);
            messages = messages->next;
        }
        messages = list->inbox;
        printf("\tInbox:\n");
        printf("\tRecvID Length Message\n");
        while(messages != NULL){
            printf("\t%-3d    %-3d    %s\n",messages->dest_id,messages->length,messages->message);
            messages = messages->next;
        }
        list = list->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return;
}

//find a pcb that we are able to send a message to
INT32 os_pcb_list_get_send_broadcast_id( INT32 curr_id ){
    
    PCB *list = pList;
    MSG *messages;
    INT32 id = -1;
    INT32 found = 0;
    
    //Get lock
    CALL(list_spinlock_get());
    
    //Check if list has been initiatilzed
    if(list == NULL){
        id = -1;
        return;
    }

    //Find the end of the list
    while(list != NULL && found == 0){
        //cant receive from sleeping or suspended proccess
        if( (list->state == READY_STATE) ||
            (list->state == RUNNING_STATE) ||
            (list->state == HALTED_STATE) ){
            messages = list->outbox;
            while(messages != NULL){
                //This is a broadcast message!
                if( (messages->dest_id == -1) ||
                    (messages->dest_id == curr_id) ){
                    id = list->id;
                    found = 1;
                    break;
                }
                messages = messages->next;
            }
        }
        list = list->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return id;
}

//find a pcb that we are able to recieve a message from
INT32 os_pcb_list_get_rec_broadcast_id( INT32 curr_id ){
    
    PCB *list = pList;
    MSG *messages;
    INT32 id = -1;
    
    //Get lock
    CALL(list_spinlock_get());
    
    //Check if list has been initiatilzed
    if(list == NULL){
        id = -1;
        return;
    }

    //Find the end of the list
    while(list != NULL){
        //cant receive from sleeping or suspended proccess
        if( (list->state == READY_STATE) ||
            (list->state == RUNNING_STATE) ||
            (list->state == HALTED_STATE) ){
            if( (list->msg_rec_id == curr_id) ||
                (list->msg_rec_id == -1) ){
                    id = list->id;
                    break;
            }
        }
        list = list->next;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return id;
}

//move a pcb to the timer queue with a wake up time
void    os_pcb_list_to_queue( INT32 id, INT32 wake_up_time ){

    PCB *queue = pQueue;
    PCB *list = pList;
    PCB *tmp, *pcb;
    INT32 id2;
  
    CALL(tmp = os_pcb_list_get_by_id(id));
   
    if(tmp == NULL){
        return;
    }
 
    //malloc new PCB
    if((pcb = malloc(sizeof(PCB))) == NULL){
        printf("Failed to malloc new PCB!\n");
        return;
    }

    memcpy(pcb,tmp,sizeof(PCB));

    pcb->wake_up_time = wake_up_time;
    pcb->state = WAITING_STATE;
    pcb->next = NULL;
    pcb->prev = NULL;

    CALL(os_pcb_queue_add(pcb));
    CALL(os_pcb_list_del(id));    

    return;
}

//copy a pcb to the timer queue with a wake up time
void    os_pcb_list_to_queue_copy( INT32 id, INT32 wake_up_time ){

    PCB *queue = pQueue;
    PCB *list = pList;
    PCB *tmp, *pcb;
    INT32 id2;
  
    CALL(tmp = os_pcb_list_get_by_id(id));
   
    if(tmp == NULL){
        return;
    }
 
    //malloc new PCB
    if((pcb = malloc(sizeof(PCB))) == NULL){
        printf("Failed to malloc new PCB!\n");
        return;
    }

    memcpy(pcb,tmp,sizeof(PCB));

    pcb->wake_up_time = wake_up_time;
    pcb->state = WAITING_STATE;
    pcb->next = NULL;
    pcb->prev = NULL;

    CALL(os_pcb_queue_add(pcb));

    return;
}

//get current running process id
INT32   os_pcb_get_curr_proc_id( void ){
    
    return current_id;
}

//set current running process id
void    os_pcb_set_curr_proc_id( INT32 id ){
    
    //Get lock
    CALL(list_spinlock_get());

    current_id = id;    

    //Give lock
    CALL(list_spinlock_give());

    return;
}

//get high priority id on PCB list
INT32   os_pcb_list_get_high_prior_id( void ){
    
    INT32 id = -1;
    PCB *list = pList;
    
    //Get lock
    CALL(list_spinlock_get());

    if(pList == NULL){
        //Give lock
        CALL(list_spinlock_give());
        return -1;
    }else{
        //Give lock
        while(list != NULL){
            if( (list->state == READY_STATE) ||
                (list->state == RUNNING_STATE) ){
                id = list->id;
                break;
            }
            list = list->next;
        }
        //Give lock
        CALL(list_spinlock_give());
        return id;
    }
    
    //Give lock
    CALL(list_spinlock_give());

    return -1;
}

//sort list
void    os_pcb_list_sort( void ){

    if(pList == NULL || pList->next == NULL){
        return;
    }

    PCB * curr = pList;
    PCB * prev;

    INT32 swap = 1;

    while(swap){
        swap = 0;
        //Find the end of the list
        while(curr->next != NULL){
            curr = curr->next;
        }
        prev = curr->prev;
        while(prev != NULL){
            if(curr->priority < prev->priority){
                CALL(os_pcb_list_swap(curr, prev));
                prev = curr->prev;
                swap = 1;
            }else{
                curr = curr->prev;
                prev = curr->prev;
            }
        }
    }      
    return;

}

//swap two entries on pcb list
void    os_pcb_list_swap(PCB* one, PCB* two){

    PCB* temp;

    if(one == NULL || two == NULL){
        return;
    }

    if( (one->next != two) || (two->prev != one) ){
        if( (two->next != one) || (one->prev != two) ){
            printf("nodes are not next to each other!\n");
            return;
        }else{
            temp = one;
            one = two;
            two = temp;
        }
    }

    temp = one->prev;
    if(temp != NULL){
        temp->next = two;
        two->prev = temp;
    }else{
        two->prev = NULL;
        pList = two;
    }

    temp = two->next;
    if(temp != NULL){
        temp->prev = one;
        one->next = temp;
    }else{
        one->next = NULL;
    }

    one->prev = two;
    two->next = one;

    return;
}
    

/************************************************************************
    OS Queue Operations
        These routines are used for all operations to the timer queue

************************************************************************/

void    os_pcb_queue_add( PCB *pcb ){

    PCB *queue = pQueue;
    
    //Get lock
    CALL(queue_spinlock_get());

    //Check if queue has been initiatilzed
    if(queue == NULL){
        pQueue = pcb;
        pcb->prev = NULL;
        pcb->next = NULL;
        CALL(queue_spinlock_give());
        return;
    }

    //Find the end of the queue
    while(queue->next != NULL){
        queue = queue->next;
    }
    
    queue->next = pcb;
    pcb->prev = queue;
    pcb->next = NULL;

    //Sort queue
    CALL(os_pcb_queue_sort());
    
    //Give lock
    CALL(queue_spinlock_give());

    return;
}

void    os_pcb_queue_del( INT32 id ){

    PCB *queue = pQueue;
    PCB *tmp;
    PCB *del;
   
    CALL(del = os_pcb_queue_get_by_id(id));
    
    //Get lock
    CALL(queue_spinlock_get());
    
    if(del == NULL){
        CALL(queue_spinlock_give());
        return;
    }
    
    tmp = del->prev;
    if(tmp != NULL){
        tmp->next = del->next;
    }else{
        pQueue = del->next;
    }
    tmp = del->next;
    if(tmp != NULL){
        tmp->prev = del->prev;
    }
    
    //del->next = NULL;
    //del->prev = NULL;
    free(del);
    
    //Give lock
    CALL(queue_spinlock_give());
    
    return;
}

PCB     *os_pcb_queue_get_by_id( INT32 id ){

    PCB *queue = pQueue;
    
    //Get lock
    CALL(queue_spinlock_get());
    
    //Check if queue has been initiatilzed
    if(queue == NULL){
        //Give lock
        CALL(queue_spinlock_give());
        return NULL;
    }

    //Find the end of the queue
    while(queue != NULL){
        if(queue->id == id){
            //Give lock
            CALL(queue_spinlock_give());
            return queue;
        }
        queue = queue->next;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return NULL;
}

PCB     *os_pcb_queue_get_by_name( const char * name ){

    PCB *queue = pList;
    
    //Get lock
    CALL(queue_spinlock_get());
    
    //Check if queue has been initiatilzed
    if(queue == NULL){
        //Give lock
        CALL(queue_spinlock_give());
        return NULL;
    }

    //Find the end of the queue
    while(queue != NULL){
        if(strcmp(queue->name,name) == 0){
            //Give lock
            CALL(queue_spinlock_give());
            return queue;
        }
        queue = queue->next;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return NULL;
}

INT32    os_pcb_queue_get_id_by_name( const char * name ){

    PCB *queue = pList;
    
    //Get lock
    CALL(queue_spinlock_get());
    
    //Check if queue has been initiatilzed
    if(queue == NULL){
        //Give lock
        CALL(queue_spinlock_give());
        return -1;
    }

    //Find the end of the queue
    while(queue != NULL){
        if(strcmp(queue->name,name) == 0){
            //Give lock
            CALL(queue_spinlock_give());
            return queue->id;
        }
        queue = queue->next;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return -1;
}

INT32    os_pcb_queue_get_state_by_id( INT32 id ){

    PCB *process;
    INT32 state;

    CALL(process = os_pcb_queue_get_by_id(id));
    
    //Get lock
    CALL(queue_spinlock_get());
    
    if(process == NULL){
        state =  -1;
    }else{
        state =  process->state;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return state;
}

INT32    os_pcb_queue_set_state_by_id( INT32 id, INT32 state ){

    PCB *process;

    CALL(process = os_pcb_queue_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(queue_spinlock_get());
       
        process->state = state;
 
        //Give lock
        CALL(queue_spinlock_give());
        return 0;
    }

    return -1;
}

INT32    os_pcb_queue_get_wakeup_by_id( INT32 id ){

    PCB *process;
    INT32 wake_up;

    CALL(process = os_pcb_queue_get_by_id(id));
    
    //Get lock
    CALL(queue_spinlock_get());
    
    if(process == NULL){
        wake_up = -1;
    }else{
        wake_up =  process->wake_up_time;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return wake_up;
}

INT32    os_pcb_queue_set_wakeup_by_id( INT32 id, INT32 wakeup ){

    PCB *process;

    CALL(process = os_pcb_queue_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(queue_spinlock_get());
       
        process->wake_up_time = wakeup;
 
        //Give lock
        CALL(queue_spinlock_give());
        return 0;
    }

    return -1;
}

INT32    os_pcb_queue_get_prior_by_id( INT32 id ){

    PCB *process;
    INT32 prior;

    CALL(process = os_pcb_queue_get_by_id(id));
    
    //Get lock
    CALL(queue_spinlock_get());
    
    if(process == NULL){
        prior = -1;
    }else{
        prior =  process->priority;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return prior;
}

INT32    os_pcb_queue_set_prior_by_id( INT32 id, INT32 prior ){

    PCB *process;

    CALL(process = os_pcb_queue_get_by_id(id));
    
    if(process == NULL){
        return -1;
    }else{
        //Get lock
        CALL(queue_spinlock_get());
       
        process->priority = prior;
 
        //Give lock
        CALL(queue_spinlock_give());
        return 0;
    }

    return -1;
}

void os_pcb_queue_print(){
    
    PCB *queue = pQueue;
    
    //Get lock
    CALL(queue_spinlock_get());
    
    printf("Queue:\n");
    printf("Name             ID  Priority State    Parent WakeUp     MsgState NumMsgs RecLen RecID\n");
    
    //Check if queue has been initiatilzed
    if(queue == NULL){
        //Give lock
        CALL(queue_spinlock_give());
        return;
    }

    //Find the end of the queue
    while(queue != NULL){
        /*printf("%-16s %-3d %-3d      %-2d    %-3d    %-10d %-2d       %-3d \n",
                queue->name,queue->id,queue->priority,queue->state,queue->parent,
                queue->wake_up_time, queue->msg_state, queue->num_msgs);*/
        printf("%-16s %-3d %-3d      ",queue->name,queue->id,queue->priority);
        switch(queue->state){
            case RUNNING_STATE:
                printf("RUNNING  ");
                break;
            case READY_STATE:
                printf("READY    ");
                break;
            case WAITING_STATE:
                printf("SLEEPING ");
                break;
            case HALTED_STATE:
                printf("HALTED   ");
                break;
        }
        printf("%-3d    %-10d ", queue->parent,queue->wake_up_time);
        switch(queue->msg_state){
            case MSG_SEND_STATE:
                printf("SEND     ");
                break;
            case MSG_SEND_ALL_STATE:
                printf("SEND ANY ");
                break;
            case MSG_REC_STATE:
                printf("RECV     ");
                break;
            case MSG_REC_ALL_STATE:
                printf("RECV ANY ");
                break;
            case MSG_READY_STATE:
                printf("READY    ");
                break;
        }
        printf("%-3d     %-3d    %-3d\n",queue->num_msgs,queue->msg_rec_len, queue->msg_rec_id);
        /*printf("%-3d    %-10d %-2d       %-3d     %-3d    %-3d\n",
                queue->parent,queue->wake_up_time, queue->msg_state,
                queue->num_msgs,queue->msg_rec_len, queue->msg_rec_id);*/
        queue = queue->next;
    }
    
    //Give lock
    CALL(queue_spinlock_give());

    return;
}

void    os_pcb_queue_to_list( INT32 id ){

    PCB *queue = pQueue;
    PCB *list = pList;
    PCB *tmp, *pcb;
    INT32 id2;

    CALL(tmp = os_pcb_queue_get_by_id(id));
   
    if(tmp == NULL){
        return;
    }
    
    //malloc new PCB
    if((pcb = malloc(sizeof(PCB))) == NULL){
        printf("Failed to malloc new PCB!\n");
        return;
    }

    memcpy(pcb,tmp,sizeof(PCB));

    pcb->state = READY_STATE; 
    pcb->wake_up_time = 0;
    pcb->next = NULL;
    pcb->prev = NULL;

    CALL(os_pcb_queue_del(id));   
    CALL(os_pcb_list_add(pcb));
    
    return; 
}

void  os_pcb_print_all( void ){

    os_pcb_list_print();
    os_pcb_queue_print();
}

INT32   os_pcb_queue_get_high_prior_id( void ){
    
    //Get lock
    CALL(queue_spinlock_get());

    if(pQueue == NULL){
        //Give lock
        CALL(queue_spinlock_give());
        return -1;
    }else{
        //Give lock
        CALL(queue_spinlock_give());
        return pQueue->id;
    }
    
    //Give lock
    CALL(queue_spinlock_give());
    return -1;
}

void    os_pcb_queue_sort( void ){

    if(pQueue == NULL || pQueue->next == NULL){
        return;
    }

    PCB * curr = pQueue;
    PCB * prev;

    INT32 swap = 1;

    while(swap){
        swap = 0;
        //Find the end of the list
        while(curr->next != NULL){
            curr = curr->next;
        }
        prev = curr->prev;
        while(prev != NULL){
            if(curr->wake_up_time < prev->wake_up_time){
                CALL(os_pcb_queue_swap(curr, prev));
                prev = curr->prev;
                swap = 1;
            }else{
                curr = curr->prev;
                prev = curr->prev;
            }
        }
    }      
    return;

}

void    os_pcb_queue_swap(PCB* one, PCB* two){

    PCB* temp;

    if(one == NULL || two == NULL){
        return;
    }

    if( (one->next != two) || (two->prev != one) ){
        if( (two->next != one) || (one->prev != two) ){
            printf("nodes are not next to each other!\n");
            return;
        }else{
            temp = one;
            one = two;
            two = temp;
        }
    }

    temp = one->prev;
    if(temp != NULL){
        temp->next = two;
        two->prev = temp;
    }else{
        two->prev = NULL;
        pQueue = two;
    }

    temp = two->next;
    if(temp != NULL){
        temp->prev = one;
        one->next = temp;
    }else{
        one->next = NULL;
    }

    one->prev = two;
    two->next = one;

    return;
}

/************************************************************************
    OS Event Queue Operations
        These routines are used for all operations to the event list

************************************************************************/

void    os_event_add( INT32 device_id, INT32 status ){

    EVNT *list = pEvent;
    EVNT *event;

    event = malloc(sizeof(EVNT));
    if(event == NULL){
        printf("MALLOC FAIL ON EVENT!\n");
    }
    event->device_id = device_id;
    event->status = status;

    //Get lock
    CALL(event_spinlock_get());
    eTotal++;

    //Check if list has been initiatilzed
    if(list == NULL){
        pEvent = event;
        list = event;
        event->next = NULL;
        CALL(event_spinlock_give());
        return;
    }

    //Find the end of the list
    while(list->next != NULL){
        list = list->next;
    }
    
    list->next = event;
    event->next = NULL;
    
    //Give lock
    CALL(event_spinlock_give());
    
    return;
}

INT32  os_event_get_next( INT32 *device_id, INT32 *status ){

    EVNT *tmp;
    INT32 ret;
    
    //Get lock
    CALL(event_spinlock_get());
   
    if(pEvent == NULL){
        ret = -1;
    }else{
        tmp = pEvent;
        if(tmp->next == NULL){
            pEvent = NULL;
        }else{
            pEvent = tmp->next;
        }
        eTotal--;
        ret = 0;
    }

    if(eTotal == 0){
        pEvent = NULL;
    }
 
    //Give lock
    CALL(event_spinlock_give());

    if(ret == 0){
        (*device_id) = tmp->device_id;
        (*status) = tmp->status;
        free(tmp);
    }else{
        (*device_id) = -1;
        (*status) = -1;
    }

    return ret;
}

void os_event_print( void ){
    
    EVNT *list = pEvent;
    
    //Get lock
    CALL(event_spinlock_get());
    
    printf("Events: %d\n", eTotal);
    printf("DeviceID Status\n");
    //Check if list has been initiatilzed
    if(list == NULL){
        //Give lock
        CALL(event_spinlock_give());
        return;
    }

    //Find the end of the list
    while(list != NULL){
        printf("%-2d       %-2d\n",list->device_id,list->status);
        list = list->next;
    }
    
    //Give lock
    CALL(event_spinlock_give());

    return;
}

void os_event_clear( void ){
 
    //Get lock
    CALL(event_spinlock_get());
    
    if(eTotal == 0){
        pEvent = NULL;
    }

    //Give lock
    CALL(event_spinlock_give());

    return;
}

INT32 os_event_get_total( void ){
    INT32 ret;   
 
    //Get lock
    CALL(event_spinlock_get());
    
    ret = eTotal;

    //Give lock
    CALL(event_spinlock_give());

    return ret;
}

/************************************************************************
    OS Frame Table Operations
        These routines are used for all operations to the frame table

************************************************************************/

//initialize frame table to PHYS_MEM_PGS length with all frames empty
void    os_frame_tbl_init( void ){
    INT32 i;

    for(i = 0; i < PHYS_MEM_PGS; i++){
        CALL(os_frame_add(i, -1, -1));

    }    

    return;
}

//add a frame to frame list
void    os_frame_add( INT32 frame_num, INT32 page_num, INT32 pid){

    FTBL *frame_tbl = pFrame;
    FTBL *frame;
    INT32 i;

    frame = malloc(sizeof(FTBL));
    frame->frame = frame_num;
    //frame->page = page_num;
    //frame->pid = pid;
    frame->last_touched = 0;
    memset(frame->tag,0,MAX_TAG_LENGTH);
    frame->frames = NULL;

    //Get lock
    CALL(frame_spinlock_get());

    //Check if list has been initiatilzed
    if(frame_tbl == NULL){
        pFrame = frame;
        frame->next = NULL;
        CALL(frame_spinlock_give());
        return;
    }

    //Find the end of the list
    while(frame_tbl->next != NULL){
        frame_tbl = frame_tbl->next;
    }
    
    frame_tbl->next = frame;
    frame->next = NULL;
    
    //Give lock
    CALL(frame_spinlock_give());
    
    return;
}

//get the virtual page and id using a frame
INT32   os_frame_get_page(INT32 frame_num, INT32* pid){

    FTBL *frame_tbl = pFrame;
    FRAME *frame;
    INT32 ret = -1;

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = -1;
    }else{
        while(frame_tbl != NULL){

            if(frame_tbl->frame == frame_num){

                //ADD SHARED MEMORY CHECK
                if(frame_tbl->frames != NULL){
                    frame = frame_tbl->frames;
                    ret = frame->page;
                    *pid = frame->pid;
                    break;
                }

            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;
}

//gets frame being used by id and virtual page
INT32   os_frame_get_frame_by_page(INT32 page, INT32 pid){

    FTBL *frame_tbl = pFrame;
    FRAME *frame;
    INT32 ret = -1;

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = -1;
    }else{
        while(frame_tbl != NULL){

            if(frame_tbl->frames != NULL){
                frame = frame_tbl->frames;

                while(frame != NULL){
                    if( (frame->page == page) &&
                        (frame->pid == pid) ){
                        ret = frame_tbl->frame;
                        break;
                    }
                    frame = frame->next;
                }
                if(ret != -1){
                    break;
                }
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;
}

//set the page, id and tag of a frame
INT32   os_frame_set_page(INT32 frame_num, INT32 page_num, INT32 pid, char *tag){
    
    FTBL *frame_tbl = pFrame;
    FRAME *frame_entry;
    FRAME *tmp;
    INT32 ret = -1;

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = -1;
    }else{
        while(frame_tbl != NULL){
            if(frame_tbl->frame == frame_num){
                if(frame_tbl->frames == NULL){
                    frame_entry = malloc(sizeof(FRAME));
                    frame_entry->page = page_num;
                    frame_entry->pid = pid;
                    frame_entry->next = NULL;
                    frame_tbl->frames = frame_entry;
                    ret = 0;
                }else if(tag != NULL){
                    if(strcmp(tag, frame_tbl->tag) == 0){
                        frame_entry = malloc(sizeof(FRAME));
                        frame_entry->page = page_num;
                        frame_entry->pid = pid;
                        frame_entry->next = NULL;
                        tmp = frame_tbl->frames;
                        if(tmp == NULL){
                            ret = 0;
                            tmp = frame_entry;
                        }else{
                            ret = 1;
                            while(tmp->next != NULL){
                                tmp = tmp->next;
                                ret++;
                            }
                            tmp->next = frame_entry;
                        }              
                    }              
                }else{
                    frame_entry = frame_tbl->frames;
                    frame_entry->page = page_num;
                    frame_entry->pid = pid;
                    ret = 0;
                }
                
                break;
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;

}

//get the tag of a frame
char*   os_frame_get_tag(INT32 frame_num){

    FTBL *frame_tbl = pFrame;
    char* ret = NULL;

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = NULL;
    }else{
        while(frame_tbl != NULL){
            if(frame_tbl->frame == frame_num){
                ret = frame_tbl->tag;
                break;
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;
}

//set the tag of a frame
INT32   os_frame_set_tag(INT32 frame_num, char* tag){
    
    FTBL *frame_tbl = pFrame;
    INT32 ret = -1;

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = -1;
    }else{
        while(frame_tbl != NULL){
            if(frame_tbl->frame == frame_num){
                memset(frame_tbl->tag,0,MAX_TAG_LENGTH);
                memcpy(frame_tbl->tag,tag,MAX_TAG_LENGTH);
                ret = 0;
                break;
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;

}

//get the next frame with a specific tag
INT32   os_frame_get_next_tagged_frame(INT32 frame_num, char *tag){
    INT32 i;
    INT32 ret = -1;    
    
    for(i = frame_num; i < PHYS_MEM_PGS; i++){
        if(strcmp(tag, os_frame_get_tag(i)) == 0){
            ret = i;
            break;
        }
    }

    return ret;
}

//update timestamp of a frame
INT32   os_frame_touch_frame(INT32 frame_num, INT32 pid){
    
    FTBL *frame_tbl = pFrame;
    FRAME *frame;
    INT32 ret = -1;
    INT32 Time;
    ZCALL (MEM_READ(Z502ClockStatus, &Time));

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = -1;
    }else{
        while(frame_tbl != NULL){
            if(frame_tbl->frame == frame_num){
                if(frame_tbl->frames != NULL){
                    frame = frame_tbl->frames;
                    while(frame != NULL){
                        if(frame->pid == pid){
                            frame_tbl->last_touched = Time;
                            ret = 0;
                            break;
                        }
                        frame = frame->next;
                    }
                    if(ret == 0){
                        break;
                    }
                }
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;

}

//return the number of the next empty frame if one exists
INT32   os_frame_get_next_empty_frame( void ){
    FTBL *frame_tbl = pFrame;
    INT32 ret = -1;
    
    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        ret = -1;
    }else{
        while(frame_tbl != NULL){
            if(frame_tbl->frames == NULL){
                ret = frame_tbl->frame;
                break;
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return ret;
}

//get the frame with the oldest timestamp
INT32   os_frame_get_last_touched_frame( void ){
    FTBL *frame_tbl = pFrame;
    INT32 touched_time;
    INT32 touched_frame;
    
    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl == NULL){
        touched_frame = -1;
    }else{
        touched_time = frame_tbl->last_touched;
        touched_frame = frame_tbl->frame;
        while(frame_tbl != NULL){
            if(frame_tbl->last_touched < touched_time){
                touched_time = frame_tbl->last_touched;
                touched_frame = frame_tbl->frame;
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return touched_frame;
}

//print out all frames
void   os_frame_print( void ){
    FTBL *frame_tbl = pFrame;
    FRAME *frame;
    
    printf("**********Frame Table*********\n");
    printf("Frame  Time  TAG                 (ID  PG  )  \n");

    //Get lock
    CALL(frame_spinlock_get());

    if(frame_tbl != NULL){
    
        while(frame_tbl != NULL){
            printf("%-7d%-6d%-20s", frame_tbl->frame, frame_tbl->last_touched, frame_tbl->tag);
            if(frame_tbl->frames != NULL){
                frame = frame_tbl->frames;
                while(frame != NULL){
                    printf("(%-4d%-4d) ", frame->pid, frame->page);
                    frame = frame->next;
                }
                printf("\n");
            }else{
                printf("\n");
                break;
            }
            frame_tbl = frame_tbl->next;
        }
    }
    
    //Give lock
    CALL(frame_spinlock_give());

    return;
}

//translate addr to page
void  os_frame_addr_to_page( INT32 addr, INT32 *page, INT32 *offset){

    *page = addr/PGSIZE;
    *offset = addr % PGSIZE;

    return;
}

//translate page to addr
INT32 os_frame_page_to_addr( INT32 page){

    return (page * PGSIZE);
}


/************************************************************************
    OS Disk Operations
        These routines are used for all operations to the disks

************************************************************************/

//init disk bit map to zero
void  os_disk_init_map( void ){
    
    INT32 i;
    INT32 j; 

    CALL(disk_spinlock_get());

    for(i = 0; i < MAX_NUMBER_OF_DISKS; i++){
        for(j = 0; j < NUM_LOGICAL_SECTORS; j++){
            //Initialize disk bit map to zero
            DISK_BIT_MAP[i][j] = 0;
        }
    }

    CALL(disk_spinlock_give());

    return;
}

//set disk and sector of bit map
void  os_disk_set_sector(INT32 disk, INT32 sector, INT32 value){
    
    CALL(disk_spinlock_get());

    DISK_BIT_MAP[disk-1][sector] = value;

    CALL(disk_spinlock_give());

    return;
}

//return the next empty segment of disk disk
INT32 os_disk_get_next_free_sector(INT32 disk){
    
    INT32 ret = -1;
    INT32 j; 

    CALL(disk_spinlock_get());

    for(j = 0; j < NUM_LOGICAL_SECTORS; j++){
        if(DISK_BIT_MAP[disk-1][j] == 0){
            ret = j;
            break;
        }
    }

    CALL(disk_spinlock_give());

    return ret;
}
