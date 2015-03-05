/*********************************************************************

 protos.h

     This include file contains prototypes needed by the various routines.
     It does NOT contain internal-only entries, but only those that are
     externally visible.


     Revision History:
        1.0    August 1990     Initial Release.
        2.1    May    2001     Add memory_printer.
        2.2    July   2002     Make code appropriate for undergrads.
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking

*********************************************************************/

/*                      ENTRIES in base.c                         */

void   interrupt_handler( void );
void   fault_handler( void );
void   page_fault_handler( INT32 );
void   svc( void );
void   os_init( void );
void   *os_get_func_ptr( const char* );
void   os_switch_context_complete( void );
void   process_sleep( INT32 );
void   restart_timer( INT32 );
INT32  wakeup_timer( INT32 );
INT32  handle_events( INT32 * );
void   switch_to_next_highest_priority( void );
void   idle( void );
void   disk_interrupt( INT32, INT32 );
void   timer_interrupt( void );
void   create_process( void *, const char *, INT32, INT32 *, INT32 *, INT32 );
void   switch_process( INT32, INT32 );
void   terminate_process( INT32, INT32 *);
void   terminate_children( INT32, INT32 *);
void   get_process_id( const char *, INT32 *, INT32 * );
void   suspend_process( INT32, INT32 * );
void   resume_process( INT32, INT32 * );
void   change_priority( INT32, INT32, INT32 * );
void   send_message( INT32, char*, INT32, INT32 * );
void   receive_message( INT32, char*, INT32, INT32 *, INT32 *, INT32 * );
void   message_transfer( INT32, INT32, INT32, INT32 *, INT32 * );
void   list_spinlock_get( void );
void   list_spinlock_give( void );
void   queue_spinlock_get( void );
void   queue_spinlock_give( void );
void   frame_spinlock_get( void );
void   frame_spinlock_give( void );
void   event_spinlock_get( void );
void   event_spinlock_give( void );
void   timer_spinlock_get( void );
void   timer_spinlock_give( void );
void   os_dump_stats( void );
void   os_dump_stats2( char*, INT32 );
void   os_dump_memory( void );
void   mem_read( INT32, INT32 * );
void   mem_write( INT32, INT32 * );
void   read_modify( INT32, INT32 );
void   disk_read(INT16, INT16, char data[PGSIZE]);
void   disk_write(INT16, INT16, char data[PGSIZE]);
void   define_shared_area( INT32, INT32, char area_tag[MAX_TAG_LENGTH], INT32 *, INT32 * );


/*                      List Operations in base.c                */
void   os_pcb_list_add( PCB * );
void   os_pcb_list_del( INT32 );
PCB   *os_pcb_list_get_by_id( INT32 );
PCB   *os_pcb_list_get_by_name( const char * );
INT32  os_pcb_list_get_id_by_name( const char * );
INT32  os_pcb_list_get_state_by_id( INT32 );
INT32  os_pcb_list_set_state_by_id( INT32, INT32 );
INT32  os_pcb_list_get_msg_num_by_id( INT32 );
INT32  os_pcb_list_get_msg_state_by_id( INT32 );
INT32  os_pcb_list_set_msg_state_by_id( INT32, INT32 );
INT32  os_pcb_list_set_msg_rec_len_by_id( INT32, INT32 );
INT32  os_pcb_list_get_msg_rec_len_by_id( INT32 );
INT32  os_pcb_list_set_msg_rec_id_by_id( INT32, INT32 );
INT32  os_pcb_list_get_msg_rec_id_by_id( INT32 );
INT32  os_pcb_list_get_msg_from_outbox( INT32, INT32, char *, INT32 );
INT32  os_pcb_list_send_msg_to_outbox( INT32, char *, INT32 );
INT32  os_pcb_list_put_msg_in_inbox( INT32, INT32, char *, INT32 );
INT32  os_pcb_list_get_last_msg_from_inbox(INT32, INT32 *, char *, INT32* );
INT32  os_pcb_list_check_outbox_for_receivers( INT32 );
UINT16* os_pcb_list_get_page_table_by_id( INT32 );
INT32  os_pcb_list_get_wakeup_by_id( INT32 );
INT32  os_pcb_list_set_wakeup_by_id( INT32, INT32 );
INT32  os_pcb_list_get_prior_by_id( INT32 );
INT32  os_pcb_list_set_prior_by_id( INT32, INT32 );
INT32  os_pcb_list_get_id_by_disk_in_use( INT32 );
INT32  os_pcb_list_get_disk_in_use_by_id( INT32 );
INT32  os_pcb_list_set_disk_in_use_by_id( INT32, INT32 );
INT32  os_pcb_list_get_sector_in_use_by_id( INT32 );
INT32  os_pcb_list_set_sector_in_use_by_id( INT32, INT32 );
INT32  os_pcb_list_set_shadow_table_page(INT32, INT32, INT32, INT32 );
INT32  os_pcb_list_get_shadow_table_page(INT32, INT32 *, INT32 *, INT32 );
INT32  os_pcb_list_get_shadow_table_page_by_sector(INT32 *, INT32, INT32, INT32 );
INT32  os_pcb_list_set_page_table_page(INT32, INT32, INT32 );
INT32  os_pcb_list_get_page_table_page(INT32, INT32 );
void   os_pcb_list_print( void );
void   os_pcb_list_msgs_print( void );
INT32  os_pcb_list_get_send_broadcast_id( INT32 );
INT32  os_pcb_list_get_rec_broadcast_id( INT32 );
void   os_pcb_list_to_queue( INT32, INT32 );
void   os_pcb_list_to_queue_copy( INT32, INT32 );
INT32  os_pcb_get_curr_proc_id( void );
void   os_pcb_set_curr_proc_id( INT32 );
INT32  os_pcb_list_get_high_prior_id( void );
void   os_pcb_list_sort( void );
void   os_pcb_list_swap( PCB *, PCB * );

/*                      Queue Operations in base.c                */
void   os_pcb_queue_add( PCB * );
void   os_pcb_queue_del( INT32 );
PCB   *os_pcb_queue_get_by_id( INT32 );
PCB   *os_pcb_queue_get_by_name( const char * );
INT32  os_pcb_queue_get_id_by_name( const char * );
INT32  os_pcb_queue_get_state_by_id( INT32 );
INT32  os_pcb_queue_set_state_by_id( INT32, INT32 );
INT32  os_pcb_queue_get_wakeup_by_id( INT32 );
INT32  os_pcb_queue_set_wakeup_by_id( INT32, INT32 );
INT32  os_pcb_queue_get_prior_by_id( INT32 );
INT32  os_pcb_queue_set_prior_by_id( INT32, INT32 );
void   os_pcb_queue_print();
void   os_pcb_queue_to_list( INT32 );
INT32  os_pcb_queue_get_high_prior_id( void );
void   os_pcb_print_all();
void   os_pcb_queue_sort( void );
void   os_pcb_queue_swap( PCB *, PCB * );

/*                      Event Operations in base.c                */
void   os_event_add( INT32, INT32 );
INT32  os_event_get_next( INT32 *, INT32 * );
void   os_event_print( void );
void   os_event_clear( void );
INT32  os_event_get_total( void );

/*                      Frame Operations in base.c                */
void   os_frame_tbl_init( void );
void   os_frame_add( INT32, INT32, INT32 );
INT32  os_frame_get_page( INT32, INT32 * );
INT32  os_frame_get_frame_by_page( INT32, INT32 );
INT32  os_frame_set_page( INT32, INT32, INT32, char* );
char*  os_frame_get_tag( INT32 );
INT32  os_frame_set_tag( INT32, char* );
INT32  os_frame_get_next_tagged_frame( INT32, char * );
INT32  os_frame_touch_frame( INT32, INT32 );
INT32  os_frame_get_next_empty_frame( void );
INT32  os_frame_get_last_touched_frame( void );
void   os_frame_print( void );
void   os_frame_addr_to_page( INT32, INT32 *, INT32 *);
INT32  os_frame_page_to_addr( INT32 );

/*                      Frame Operations in base.c                */
void   os_disk_init_map( void );
void   os_disk_set_sector( INT32, INT32, INT32 );
INT32  os_disk_get_next_free_sector( INT32 );

/*                      ENTRIES in sample.c                       */

void   sample_code(void );

/*                      ENTRIES in state_printer.c                */

void   SP_setup( INT16, INT32 );
void   SP_setup_file( INT16, FILE * );
void   SP_setup_action( INT16, char * );
void   SP_print_header( void );
void   SP_print_line( void );
void   SP_do_output( char * );
void   MP_setup( INT32, INT32, INT32, INT32 );
void   MP_print_line( void );

/*                      ENTRIES in test.c                         */                     

void   test0( void );
void   test1a( void );
void   test1b( void );
void   test1c( void );
void   test1d( void );
void   test1e( void );
void   test1f( void );
void   test1g( void );
void   test1h( void );
void   test1i( void );
void   test1j( void );
void   test1k( void );
void   test1l( void );
void   test1m( void );
void   test2a( void );
void   test2b( void );
void   test2c( void );
void   test2d( void );
void   test2e( void );
void   test2f( void );
void   test2g( void );
void   get_skewed_random_number( long *, long );



/*                      ENTRIES in z502.c                       */

void   Z502_HALT( void );
void   Z502_MEM_READ(INT32, INT32 * );
void   Z502_MEM_WRITE(INT32, INT32 * );
void   Z502_READ_MODIFY( INT32, INT32, INT32, INT32 * );
void   Z502_HALT( void );
void   Z502_IDLE( void );
void   Z502_DESTROY_CONTEXT( void ** );
void   Z502_MAKE_CONTEXT( void **, void *, BOOL );
void   Z502_SWITCH_CONTEXT( BOOL, void ** );

int    CreateAThread( void *, INT32 * );
void   DestroyThread( INT32   );

void   CreateLock( INT32 * );
void   CreateCondition( UINT32 * );
int    GetLock( UINT32, char *  );
int    WaitForCondition( UINT32 , UINT32, INT32  );
int    GetTryLock( UINT32 );
int    ReleaseLock( UINT32, char *  );
int    SignalCondition( UINT32, char *  );
void   DoSleep( INT32 millisecs );
void   HandleWindowsError( );
void   GoToExit( int );
