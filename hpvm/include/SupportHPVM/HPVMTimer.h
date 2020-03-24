//===------------ HPVMTimer.h - Header file for "HPVM Timer API" ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef HPVM_TIMER_HEADER
#define HPVM_TIMER_HEADER

/************************** Timer Routines ***************************/
extern "C" {

/* A time or duration. */
//#if _POSIX_VERSION >= 200112L
typedef unsigned long long hpvm_Timestamp; /* time in microseconds */
//#else
//# error "Timestamps not implemented"
//#endif

enum hpvm_TimerState {
  hpvm_Timer_STOPPED,
  hpvm_Timer_RUNNING,
};

struct hpvm_Timer {
  enum hpvm_TimerState state;
  hpvm_Timestamp elapsed; /* Amount of time elapsed so far */
  hpvm_Timestamp init;    /* Beginning of the current time interval,
                           * if state is RUNNING.  End of the last
                           * recorded time interfal otherwise.  */
};

/* Reset a timer.
 * Use this to initialize a timer or to clear
 * its elapsed time.  The reset timer is stopped.
 */
void hpvm_ResetTimer(struct hpvm_Timer *timer);

/* Start a timer.  The timer is set to RUNNING mode and
 * time elapsed while the timer is running is added to
 * the timer.
 * The timer should not already be running.
 */
void hpvm_StartTimer(struct hpvm_Timer *timer);

/* Stop a timer.
 * This stops adding elapsed time to the timer.
 * The timer should not already be stopped.
 */
void hpvm_StopTimer(struct hpvm_Timer *timer);

/* Get the elapsed time in seconds. */
double hpvm_GetElapsedTime(struct hpvm_Timer *timer);

/* Execution time is assigned to one of these categories. */
enum hpvm_TimerID {
  hpvm_TimerID_NONE = 0,
  hpvm_TimerID_IO,         /* Time spent in input/output */
  hpvm_TimerID_KERNEL,     /* Time spent computing on the device,
                            * recorded asynchronously */
  hpvm_TimerID_COPY,       /* Time spent synchronously moving data
                            * to/from device and allocating/freeing
                            * memory on the device */
  hpvm_TimerID_DRIVER,     /* Time spent in the host interacting with the
                            * driver, primarily for recording the time
                            * spent queueing asynchronous operations */
  hpvm_TimerID_COPY_ASYNC, /* Time spent in asynchronous transfers */
  hpvm_TimerID_COMPUTE,    /* Time for all program execution other
                            * than parsing command line arguments,
                            * I/O, kernel, and copy */
  hpvm_TimerID_OVERLAP,    /* Time double-counted in asynchronous and
                            * host activity: automatically filled in,
                            * not intended for direct usage */
  // GPU FUNCTION
  hpvm_TimerID_INIT_CTX,
  hpvm_TimerID_CLEAR_CTX,
  hpvm_TimerID_COPY_SCALAR,
  hpvm_TimerID_COPY_PTR,
  hpvm_TimerID_MEM_FREE,
  hpvm_TimerID_READ_OUTPUT,
  hpvm_TimerID_SETUP,
  hpvm_TimerID_MEM_TRACK,
  hpvm_TimerID_MEM_UNTRACK,
  hpvm_TimerID_MISC,
  // LAUNCH FUNCTION
  hpvm_TimerID_PTHREAD_CREATE,
  hpvm_TimerID_ARG_PACK,
  hpvm_TimerID_ARG_UNPACK,
  hpvm_TimerID_COMPUTATION,
  hpvm_TimerID_OUTPUT_PACK,
  hpvm_TimerID_OUTPUT_UNPACK,

  hpvm_TimerID_LAST /* Number of timer IDs */
};

/* Dynamic list of asynchronously tracked times between events */
struct hpvm_async_time_marker_list {
  char *label;               // actually just a pointer to a string
  enum hpvm_TimerID timerID; /* The ID to which the interval beginning
                              * with this marker should be attributed */
  void *marker;
  // cudaEvent_t marker; 		/* The driver event for this marker */
  struct hpvm_async_time_marker_list *next;
};

struct hpvm_SubTimer {
  char *label;
  struct hpvm_Timer timer;
  struct hpvm_SubTimer *next;
};

struct hpvm_SubTimerList {
  struct hpvm_SubTimer *current;
  struct hpvm_SubTimer *subtimer_list;
};

/* A set of timers for recording execution times. */
struct hpvm_TimerSet {
  enum hpvm_TimerID current;
  struct hpvm_async_time_marker_list *async_markers;
  hpvm_Timestamp async_begin;
  hpvm_Timestamp wall_begin;
  struct hpvm_Timer timers[hpvm_TimerID_LAST];
  struct hpvm_SubTimerList *sub_timer_list[hpvm_TimerID_LAST];
};

/* Reset all timers in the set. */
void hpvm_InitializeTimerSet(struct hpvm_TimerSet *timers);

void hpvm_AddSubTimer(struct hpvm_TimerSet *timers, char *label,
                      enum hpvm_TimerID hpvm_Category);

/* Select which timer the next interval of time should be accounted
 * to. The selected timer is started and other timers are stopped.
 * Using hpvm_TimerID_NONE stops all timers. */
inline void hpvm_SwitchToTimer(struct hpvm_TimerSet *timers,
                               enum hpvm_TimerID timer);

void hpvm_SwitchToSubTimer(struct hpvm_TimerSet *timers, char *label,
                           enum hpvm_TimerID category);

/* Print timer values to standard output. */
void hpvm_PrintTimerSet(struct hpvm_TimerSet *timers);

/* Release timer resources */
void hpvm_DestroyTimerSet(struct hpvm_TimerSet *timers);
}
#endif // HPVM_RT_HEADER
