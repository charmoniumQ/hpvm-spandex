#include <CL/cl.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <pthread.h>
#include <string>

#include <unistd.h>

#if _POSIX_VERSION >= 200112L
#include <sys/time.h>
#endif
#include "hpvm-rt.h"

//#define DEBUG_BUILD
#ifndef DEBUG_BUILD
#define DEBUG(s)                                                               \
  {}
#else
#define DEBUG(s) s
#endif

#define BILLION 1000000000LL

typedef struct {
  pthread_t threadID;
  std::vector<pthread_t> *threads;
  // Map from InputPort to Size
  std::map<unsigned, uint64_t> *ArgInPortSizeMap;
  std::vector<unsigned> *BindInSourcePort;
  std::vector<uint64_t> *BindOutSizes;
  std::vector<uint64_t> *EdgeSizes;
  std::vector<CircularBuffer<uint64_t> *> *BindInputBuffers;
  std::vector<CircularBuffer<uint64_t> *> *BindOutputBuffers;
  std::vector<CircularBuffer<uint64_t> *> *EdgeBuffers;
  std::vector<CircularBuffer<uint64_t> *> *isLastInputBuffers;
} DFNodeContext_CPU;

typedef struct {
  cl_context clOCLContext;
  cl_command_queue clCommandQue;
  cl_program clProgram;
  cl_kernel clKernel;
} DFNodeContext_OCL;

cl_context globalOCLContext;
cl_device_id *clDevices;
cl_command_queue globalCommandQue;

MemTracker MTracker;
vector<DFGDepth> DStack;
// Mutex to prevent concurrent access by multiple thereads in pipeline
pthread_mutex_t ocl_mtx;

#define NUM_TESTS 1
hpvm_TimerSet kernel_timer;

static const char *getErrorString(cl_int error) {
  switch (error) {
  // run-time and JIT compiler errors
  case 0:
    return "CL_SUCCESS";
  case -1:
    return "CL_DEVICE_NOT_FOUND";
  case -2:
    return "CL_DEVICE_NOT_AVAILABLE";
  case -3:
    return "CL_COMPILER_NOT_AVAILABLE";
  case -4:
    return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
  case -5:
    return "CL_OUT_OF_RESOURCES";
  case -6:
    return "CL_OUT_OF_HOST_MEMORY";
  case -7:
    return "CL_PROFILING_INFO_NOT_AVAILABLE";
  case -8:
    return "CL_MEM_COPY_OVERLAP";
  case -9:
    return "CL_IMAGE_FORMAT_MISMATCH";
  case -10:
    return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
  case -11:
    return "CL_BUILD_PROGRAM_FAILURE";
  case -12:
    return "CL_MAP_FAILURE";
  case -13:
    return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
  case -14:
    return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
  case -15:
    return "CL_COMPILE_PROGRAM_FAILURE";
  case -16:
    return "CL_LINKER_NOT_AVAILABLE";
  case -17:
    return "CL_LINK_PROGRAM_FAILURE";
  case -18:
    return "CL_DEVICE_PARTITION_FAILED";
  case -19:
    return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

  // compile-time errors
  case -30:
    return "CL_INVALID_VALUE";
  case -31:
    return "CL_INVALID_DEVICE_TYPE";
  case -32:
    return "CL_INVALID_PLATFORM";
  case -33:
    return "CL_INVALID_DEVICE";
  case -34:
    return "CL_INVALID_CONTEXT";
  case -35:
    return "CL_INVALID_QUEUE_PROPERTIES";
  case -36:
    return "CL_INVALID_COMMAND_QUEUE";
  case -37:
    return "CL_INVALID_HOST_PTR";
  case -38:
    return "CL_INVALID_MEM_OBJECT";
  case -39:
    return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
  case -40:
    return "CL_INVALID_IMAGE_SIZE";
  case -41:
    return "CL_INVALID_SAMPLER";
  case -42:
    return "CL_INVALID_BINARY";
  case -43:
    return "CL_INVALID_BUILD_OPTIONS";
  case -44:
    return "CL_INVALID_PROGRAM";
  case -45:
    return "CL_INVALID_PROGRAM_EXECUTABLE";
  case -46:
    return "CL_INVALID_KERNEL_NAME";
  case -47:
    return "CL_INVALID_KERNEL_DEFINITION";
  case -48:
    return "CL_INVALID_KERNEL";
  case -49:
    return "CL_INVALID_ARG_INDEX";
  case -50:
    return "CL_INVALID_ARG_VALUE";
  case -51:
    return "CL_INVALID_ARG_SIZE";
  case -52:
    return "CL_INVALID_KERNEL_ARGS";
  case -53:
    return "CL_INVALID_WORK_DIMENSION";
  case -54:
    return "CL_INVALID_WORK_GROUP_SIZE";
  case -55:
    return "CL_INVALID_WORK_ITEM_SIZE";
  case -56:
    return "CL_INVALID_GLOBAL_OFFSET";
  case -57:
    return "CL_INVALID_EVENT_WAIT_LIST";
  case -58:
    return "CL_INVALID_EVENT";
  case -59:
    return "CL_INVALID_OPERATION";
  case -60:
    return "CL_INVALID_GL_OBJECT";
  case -61:
    return "CL_INVALID_BUFFER_SIZE";
  case -62:
    return "CL_INVALID_MIP_LEVEL";
  case -63:
    return "CL_INVALID_GLOBAL_WORK_SIZE";
  case -64:
    return "CL_INVALID_PROPERTY";
  case -65:
    return "CL_INVALID_IMAGE_DESCRIPTOR";
  case -66:
    return "CL_INVALID_COMPILER_OPTIONS";
  case -67:
    return "CL_INVALID_LINKER_OPTIONS";
  case -68:
    return "CL_INVALID_DEVICE_PARTITION_COUNT";

  // extension errors
  case -1000:
    return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
  case -1001:
    return "CL_PLATFORM_NOT_FOUND_KHR";
  case -1002:
    return "CL_INVALID_D3D10_DEVICE_KHR";
  case -1003:
    return "CL_INVALID_D3D10_RESOURCE_KHR";
  case -1004:
    return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
  case -1005:
    return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
  default:
    return "Unknown OpenCL error";
  }
}

static inline void checkErr(cl_int err, cl_int success, const char *name) {
  if (err != success) {
    cout << "ERROR: " << name << flush << "\n";
    cout << "ErrorCode: " << getErrorString(err) << flush << "\n";
    exit(EXIT_FAILURE);
  }
}

/************************* Depth Stack Routines ***************************/

void llvm_hpvm_cpu_dstack_push(unsigned n, uint64_t limitX, uint64_t iX,
                               uint64_t limitY, uint64_t iY, uint64_t limitZ,
                               uint64_t iZ) {
  DEBUG(cout << "Pushing node information on stack:\n");
  DEBUG(cout << "\tNumDim = " << n << "\t Limit(" << limitX << ", " << limitY
             << ", " << limitZ << ")\n");
  DEBUG(cout << "\tInstance(" << iX << ", " << iY << ", " << iZ << ")\n");
  DFGDepth nodeInfo(n, limitX, iX, limitY, iY, limitZ, iZ);
  pthread_mutex_lock(&ocl_mtx);
  DStack.push_back(nodeInfo);
  DEBUG(cout << "DStack size = " << DStack.size() << flush << "\n");
  pthread_mutex_unlock(&ocl_mtx);
}

void llvm_hpvm_cpu_dstack_pop() {
  DEBUG(cout << "Popping from depth stack\n");
  pthread_mutex_lock(&ocl_mtx);
  DStack.pop_back();
  DEBUG(cout << "DStack size = " << DStack.size() << flush << "\n");
  pthread_mutex_unlock(&ocl_mtx);
}

uint64_t llvm_hpvm_cpu_getDimLimit(unsigned level, unsigned dim) {
  DEBUG(cout << "Request limit for dim " << dim << " of ancestor " << level
             << flush << "\n");
  pthread_mutex_lock(&ocl_mtx);
  unsigned size = DStack.size();
  DEBUG(cout << "\t Return: " << DStack[size - level - 1].getDimLimit(dim)
             << flush << "\n");
  uint64_t result = DStack[size - level - 1].getDimLimit(dim);
  pthread_mutex_unlock(&ocl_mtx);
  return result;
}

uint64_t llvm_hpvm_cpu_getDimInstance(unsigned level, unsigned dim) {
  DEBUG(cout << "Request instance id for dim " << dim << " of ancestor "
             << level << flush << "\n");
  pthread_mutex_lock(&ocl_mtx);
  unsigned size = DStack.size();
  DEBUG(cout << "\t Return: " << DStack[size - level - 1].getDimInstance(dim)
             << flush << "\n");
  uint64_t result = DStack[size - level - 1].getDimInstance(dim);
  pthread_mutex_unlock(&ocl_mtx);
  return result;
}

/********************** Memory Tracking Routines **************************/

void llvm_hpvm_track_mem(void *ptr, size_t size) {
  DEBUG(cout << "Start tracking memory: " << ptr << flush << "\n");
  MemTrackerEntry *MTE = MTracker.lookup(ptr);
  if (MTE != NULL) {
    DEBUG(cout << "ID " << ptr << " already present in the MemTracker Table\n");
    return;
  }
  DEBUG(cout << "Inserting ID " << ptr << " in the MemTracker Table\n");
  MTracker.insert(ptr, size, MemTrackerEntry::HOST, ptr);
  DEBUG(MTracker.print());
}

void llvm_hpvm_untrack_mem(void *ptr) {
  DEBUG(cout << "Stop tracking memory: " << ptr << flush << "\n");
  MemTrackerEntry *MTE = MTracker.lookup(ptr);
  if (MTE == NULL) {
    cout << "WARNING: Trying to remove ID " << ptr
         << " not present in the MemTracker Table\n";
    return;
  }
  DEBUG(cout << "Removing ID " << ptr << " from MemTracker Table\n");
  if (MTE->getLocation() == MemTrackerEntry::DEVICE)
    clReleaseMemObject((cl_mem)MTE->getAddress());
  MTracker.remove(ptr);
  DEBUG(MTracker.print());
}

static void *llvm_hpvm_ocl_request_mem(void *ptr, size_t size,
                                       DFNodeContext_OCL *Context, bool isInput,
                                       bool isOutput) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "[OCL] Request memory: " << ptr
             << " for context: " << Context->clOCLContext << flush << "\n");
  MemTrackerEntry *MTE = MTracker.lookup(ptr);
  if (MTE == NULL) {
    MTracker.print();
    cout << "ERROR: Requesting memory not present in Table\n";
    exit(EXIT_FAILURE);
  }
  // If already on device
  if (MTE->getLocation() == MemTrackerEntry::DEVICE &&
      ((DFNodeContext_OCL *)MTE->getContext())->clOCLContext ==
          Context->clOCLContext) {
    DEBUG(cout << "\tMemory found on device at: " << MTE->getAddress() << flush
               << "\n");
    pthread_mutex_unlock(&ocl_mtx);
    return MTE->getAddress();
  }

  DEBUG(cout << "\tMemory found on host at: " << MTE->getAddress() << flush
             << "\n");
  DEBUG(cout << "\t"; MTE->print(); cout << flush << "\n");
  // Else copy and update the latest copy
  cl_mem_flags clFlags;
  cl_int errcode;

  if (isInput && isOutput)
    clFlags = CL_MEM_READ_WRITE;
  else if (isInput)
    clFlags = CL_MEM_READ_ONLY;
  else if (isOutput)
    clFlags = CL_MEM_WRITE_ONLY;
  else
    clFlags = CL_MEM_READ_ONLY;

  hpvm_SwitchToTimer(&kernel_timer, hpvm_TimerID_COPY);
  cl_mem d_input =
      clCreateBuffer(Context->clOCLContext, clFlags, size, NULL, &errcode);
  checkErr(errcode, CL_SUCCESS, "Failure to allocate memory on device");
  DEBUG(cout << "\nMemory allocated on device: " << d_input << flush << "\n");
  if (isInput) {
    DEBUG(cout << "\tCopying ...");
    errcode = clEnqueueWriteBuffer(Context->clCommandQue, d_input, CL_TRUE, 0,
                                   size, MTE->getAddress(), 0, NULL, NULL);
    checkErr(errcode, CL_SUCCESS, "Failure to copy memory to device");
  }

  hpvm_SwitchToTimer(&kernel_timer, hpvm_TimerID_NONE);
  DEBUG(cout << " done\n");
  MTE->update(MemTrackerEntry::DEVICE, (void *)d_input, Context);
  DEBUG(cout << "Updated Table\n");
  DEBUG(MTracker.print());
  pthread_mutex_unlock(&ocl_mtx);
  return d_input;
}

void *llvm_hpvm_cpu_argument_ptr(void *ptr, size_t size) {
  return llvm_hpvm_request_mem(ptr, size);
}

void *llvm_hpvm_request_mem(void *ptr, size_t size) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "[CPU] Request memory: " << ptr << flush << "\n");
  MemTrackerEntry *MTE = MTracker.lookup(ptr);
  if (MTE == NULL) {
    cout << "ERROR: Requesting memory not present in Table\n";
    pthread_mutex_unlock(&ocl_mtx);
    exit(EXIT_FAILURE);
  }
  // If already on host
  if (MTE->getLocation() == MemTrackerEntry::HOST) {
    DEBUG(cout << "\tMemory found on host at: " << MTE->getAddress() << flush
               << "\n");
    pthread_mutex_unlock(&ocl_mtx);
    return MTE->getAddress();
  }

  // Else copy from device and update table
  DEBUG(cout << "\tMemory found on device at: " << MTE->getAddress() << flush
             << "\n");
  DEBUG(cout << "\tCopying ...");
  hpvm_SwitchToTimer(&kernel_timer, hpvm_TimerID_COPY);
  // pthread_mutex_lock(&ocl_mtx);
  cl_int errcode = clEnqueueReadBuffer(
      ((DFNodeContext_OCL *)MTE->getContext())->clCommandQue,
      (cl_mem)MTE->getAddress(), CL_TRUE, 0, size, ptr, 0, NULL, NULL);
  // pthread_mutex_unlock(&ocl_mtx);
  hpvm_SwitchToTimer(&kernel_timer, hpvm_TimerID_NONE);
  DEBUG(cout << " done\n");
  checkErr(errcode, CL_SUCCESS, "[request mem] Failure to read output");
  DEBUG(cout << "Free mem object on device\n");
  clReleaseMemObject((cl_mem)MTE->getAddress());
  DEBUG(cout << "Updated Table\n");
  MTE->update(MemTrackerEntry::HOST, ptr);
  DEBUG(MTracker.print());
  pthread_mutex_unlock(&ocl_mtx);
  return ptr;
}

/*************************** Timer Routines **********************************/

static int is_async(enum hpvm_TimerID timer) {
  return (timer == hpvm_TimerID_KERNEL) || (timer == hpvm_TimerID_COPY_ASYNC);
}

static int is_blocking(enum hpvm_TimerID timer) {
  return (timer == hpvm_TimerID_COPY) || (timer == hpvm_TimerID_NONE);
}

#define INVALID_TIMERID hpvm_TimerID_LAST

static int asyncs_outstanding(struct hpvm_TimerSet *timers) {
  return (timers->async_markers != NULL) &&
         (timers->async_markers->timerID != INVALID_TIMERID);
}

static struct hpvm_async_time_marker_list *
get_last_async(struct hpvm_TimerSet *timers) {
  /* Find the last event recorded thus far */
  struct hpvm_async_time_marker_list *last_event = timers->async_markers;
  if (last_event != NULL && last_event->timerID != INVALID_TIMERID) {
    while (last_event->next != NULL &&
           last_event->next->timerID != INVALID_TIMERID)
      last_event = last_event->next;
    return last_event;
  } else
    return NULL;
}

static void insert_marker(struct hpvm_TimerSet *tset, enum hpvm_TimerID timer) {
  cl_int ciErrNum = CL_SUCCESS;
  struct hpvm_async_time_marker_list **new_event = &(tset->async_markers);

  while (*new_event != NULL && (*new_event)->timerID != INVALID_TIMERID) {
    new_event = &((*new_event)->next);
  }

  if (*new_event == NULL) {
    *new_event = (struct hpvm_async_time_marker_list *)malloc(
        sizeof(struct hpvm_async_time_marker_list));
    (*new_event)->marker = calloc(1, sizeof(cl_event));
    (*new_event)->next = NULL;
  }

  /* valid event handle now aquired: insert the event record */
  (*new_event)->label = NULL;
  (*new_event)->timerID = timer;
  ciErrNum =
      clEnqueueMarker(globalCommandQue, (cl_event *)(*new_event)->marker);
  if (ciErrNum != CL_SUCCESS) {
    fprintf(stderr, "Error Enqueueing Marker!\n");
  }
}

static void insert_submarker(struct hpvm_TimerSet *tset, char *label,
                             enum hpvm_TimerID timer) {
  cl_int ciErrNum = CL_SUCCESS;
  struct hpvm_async_time_marker_list **new_event = &(tset->async_markers);

  while (*new_event != NULL && (*new_event)->timerID != INVALID_TIMERID) {
    new_event = &((*new_event)->next);
  }

  if (*new_event == NULL) {
    *new_event = (struct hpvm_async_time_marker_list *)malloc(
        sizeof(struct hpvm_async_time_marker_list));
    (*new_event)->marker = calloc(1, sizeof(cl_event));
    (*new_event)->next = NULL;
  }

  /* valid event handle now aquired: insert the event record */
  (*new_event)->label = label;
  (*new_event)->timerID = timer;
  ciErrNum =
      clEnqueueMarker(globalCommandQue, (cl_event *)(*new_event)->marker);
  if (ciErrNum != CL_SUCCESS) {
    fprintf(stderr, "Error Enqueueing Marker!\n");
  }
}

/* Assumes that all recorded events have completed */
static hpvm_Timestamp record_async_times(struct hpvm_TimerSet *tset) {
  struct hpvm_async_time_marker_list *next_interval = NULL;
  struct hpvm_async_time_marker_list *last_marker = get_last_async(tset);
  hpvm_Timestamp total_async_time = 0;

  for (next_interval = tset->async_markers; next_interval != last_marker;
       next_interval = next_interval->next) {
    cl_ulong command_start = 0, command_end = 0;
    cl_int ciErrNum = CL_SUCCESS;

    ciErrNum = clGetEventProfilingInfo(*((cl_event *)next_interval->marker),
                                       CL_PROFILING_COMMAND_END,
                                       sizeof(cl_ulong), &command_start, NULL);
    if (ciErrNum != CL_SUCCESS) {
      fprintf(stderr, "Error getting first EventProfilingInfo: %d\n", ciErrNum);
    }

    ciErrNum = clGetEventProfilingInfo(
        *((cl_event *)next_interval->next->marker), CL_PROFILING_COMMAND_END,
        sizeof(cl_ulong), &command_end, NULL);
    if (ciErrNum != CL_SUCCESS) {
      fprintf(stderr, "Error getting second EventProfilingInfo: %d\n",
              ciErrNum);
    }

    hpvm_Timestamp interval =
        (hpvm_Timestamp)(((double)(command_end - command_start)));
    tset->timers[next_interval->timerID].elapsed += interval;
    if (next_interval->label != NULL) {
      struct hpvm_SubTimer *subtimer =
          tset->sub_timer_list[next_interval->timerID]->subtimer_list;
      while (subtimer != NULL) {
        if (strcmp(subtimer->label, next_interval->label) == 0) {
          subtimer->timer.elapsed += interval;
          break;
        }
        subtimer = subtimer->next;
      }
    }
    total_async_time += interval;
    next_interval->timerID = INVALID_TIMERID;
  }

  if (next_interval != NULL)
    next_interval->timerID = INVALID_TIMERID;

  return total_async_time;
}

static void accumulate_time(hpvm_Timestamp *accum, hpvm_Timestamp start,
                            hpvm_Timestamp end) {
#if _POSIX_VERSION >= 200112L
  *accum += end - start;
#else
#error "Timestamps not implemented for this system"
#endif
}

#if _POSIX_VERSION >= 200112L
static hpvm_Timestamp get_time() {
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return (hpvm_Timestamp)(tv.tv_sec * BILLION + tv.tv_nsec);
}
#else
#error "no supported time libraries are available on this platform"
#endif

void hpvm_ResetTimer(struct hpvm_Timer *timer) {
  timer->state = hpvm_Timer_STOPPED;

#if _POSIX_VERSION >= 200112L
  timer->elapsed = 0;
#else
#error "hpvm_ResetTimer: not implemented for this system"
#endif
}

void hpvm_StartTimer(struct hpvm_Timer *timer) {
  if (timer->state != hpvm_Timer_STOPPED) {
    // FIXME: Removing warning statement to avoid printing this error
    // fputs("Ignoring attempt to start a running timer\n", stderr);
    return;
  }

  timer->state = hpvm_Timer_RUNNING;

#if _POSIX_VERSION >= 200112L
  {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    timer->init = tv.tv_sec * BILLION + tv.tv_nsec;
  }
#else
#error "hpvm_StartTimer: not implemented for this system"
#endif
}

void hpvm_StartTimerAndSubTimer(struct hpvm_Timer *timer,
                                struct hpvm_Timer *subtimer) {

  unsigned int numNotStopped = 0x3; // 11
  if (timer->state != hpvm_Timer_STOPPED) {
    fputs("Warning: Timer was not stopped\n", stderr);
    numNotStopped &= 0x1; // Zero out 2^1
  }
  if (subtimer->state != hpvm_Timer_STOPPED) {
    fputs("Warning: Subtimer was not stopped\n", stderr);
    numNotStopped &= 0x2; // Zero out 2^0
  }
  if (numNotStopped == 0x0) {
    return;
  }

  timer->state = hpvm_Timer_RUNNING;
  subtimer->state = hpvm_Timer_RUNNING;

#if _POSIX_VERSION >= 200112L
  {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);

    if (numNotStopped & 0x2) {
      timer->init = tv.tv_sec * BILLION + tv.tv_nsec;
    }

    if (numNotStopped & 0x1) {
      subtimer->init = tv.tv_sec * BILLION + tv.tv_nsec;
    }
  }
#else
#error "hpvm_StartTimer: not implemented for this system"
#endif
}

void hpvm_StopTimer(struct hpvm_Timer *timer) {
  hpvm_Timestamp fini;

  if (timer->state != hpvm_Timer_RUNNING) {
    // fputs("Ignoring attempt to stop a stopped timer\n", stderr);
    return;
  }

  timer->state = hpvm_Timer_STOPPED;

#if _POSIX_VERSION >= 200112L
  {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    fini = tv.tv_sec * BILLION + tv.tv_nsec;
  }
#else
#error "hpvm_StopTimer: not implemented for this system"
#endif

  accumulate_time(&timer->elapsed, timer->init, fini);
  timer->init = fini;
}

void hpvm_StopTimerAndSubTimer(struct hpvm_Timer *timer,
                               struct hpvm_Timer *subtimer) {

  hpvm_Timestamp fini;

  unsigned int numNotRunning = 0x3; // 11
  if (timer->state != hpvm_Timer_RUNNING) {
    fputs("Warning: Timer was not running\n", stderr);
    numNotRunning &= 0x1; // Zero out 2^1
  }
  if (subtimer->state != hpvm_Timer_RUNNING) {
    fputs("Warning: Subtimer was not running\n", stderr);
    numNotRunning &= 0x2; // Zero out 2^0
  }
  if (numNotRunning == 0x0) {
    return;
  }

  timer->state = hpvm_Timer_STOPPED;
  subtimer->state = hpvm_Timer_STOPPED;

#if _POSIX_VERSION >= 200112L
  {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    fini = tv.tv_sec * BILLION + tv.tv_nsec;
  }
#else
#error "hpvm_StopTimer: not implemented for this system"
#endif

  if (numNotRunning & 0x2) {
    accumulate_time(&timer->elapsed, timer->init, fini);
    timer->init = fini;
  }

  if (numNotRunning & 0x1) {
    accumulate_time(&subtimer->elapsed, subtimer->init, fini);
    subtimer->init = fini;
  }
}

/* Get the elapsed time in seconds. */
double hpvm_GetElapsedTime(struct hpvm_Timer *timer) {
  double ret;

  if (timer->state != hpvm_Timer_STOPPED) {
    fputs("Elapsed time from a running timer is inaccurate\n", stderr);
  }

#if _POSIX_VERSION >= 200112L
  ret = timer->elapsed / 1e9;
#else
#error "hpvm_GetElapsedTime: not implemented for this system"
#endif
  return ret;
}

void hpvm_InitializeTimerSet(struct hpvm_TimerSet *timers) {
  int n;

  timers->wall_begin = get_time();
  timers->current = hpvm_TimerID_NONE;

  timers->async_markers = NULL;

  for (n = 0; n < hpvm_TimerID_LAST; n++) {
    hpvm_ResetTimer(&timers->timers[n]);
    timers->sub_timer_list[n] = NULL;
  }
}

void hpvm_AddSubTimer(struct hpvm_TimerSet *timers, char *label,
                      enum hpvm_TimerID hpvm_Category) {

  struct hpvm_SubTimer *subtimer =
      (struct hpvm_SubTimer *)malloc(sizeof(struct hpvm_SubTimer));

  int len = strlen(label);

  subtimer->label = (char *)malloc(sizeof(char) * (len + 1));
  sprintf(subtimer->label, "%s", label);

  hpvm_ResetTimer(&subtimer->timer);
  subtimer->next = NULL;

  struct hpvm_SubTimerList *subtimerlist =
      timers->sub_timer_list[hpvm_Category];
  if (subtimerlist == NULL) {
    subtimerlist =
        (struct hpvm_SubTimerList *)calloc(1, sizeof(struct hpvm_SubTimerList));
    subtimerlist->subtimer_list = subtimer;
    timers->sub_timer_list[hpvm_Category] = subtimerlist;
  } else {
    // Append to list
    struct hpvm_SubTimer *element = subtimerlist->subtimer_list;
    while (element->next != NULL) {
      element = element->next;
    }
    element->next = subtimer;
  }
}

void hpvm_SwitchToTimer(struct hpvm_TimerSet *timers, enum hpvm_TimerID timer) {
  // cerr << "Switch to timer: " << timer << flush << "\n";
  /* Stop the currently running timer */
  if (timers->current != hpvm_TimerID_NONE) {
    struct hpvm_SubTimerList *subtimerlist =
        timers->sub_timer_list[timers->current];
    struct hpvm_SubTimer *currSubTimer =
        (subtimerlist != NULL) ? subtimerlist->current : NULL;

    if (!is_async(timers->current)) {
      if (timers->current != timer) {
        if (currSubTimer != NULL) {
          hpvm_StopTimerAndSubTimer(&timers->timers[timers->current],
                                    &currSubTimer->timer);
        } else {
          hpvm_StopTimer(&timers->timers[timers->current]);
        }
      } else {
        if (currSubTimer != NULL) {
          hpvm_StopTimer(&currSubTimer->timer);
        }
      }
    } else {
      insert_marker(timers, timer);
      if (!is_async(timer)) { // if switching to async too, keep driver going
        hpvm_StopTimer(&timers->timers[hpvm_TimerID_DRIVER]);
      }
    }
  }

  hpvm_Timestamp currentTime = get_time();

  /* The only cases we check for asynchronous task completion is
   * when an overlapping CPU operation completes, or the next
   * segment blocks on completion of previous async operations */
  if (asyncs_outstanding(timers) &&
      (!is_async(timers->current) || is_blocking(timer))) {

    struct hpvm_async_time_marker_list *last_event = get_last_async(timers);
    /* CL_COMPLETE if completed */

    cl_int ciErrNum = CL_SUCCESS;
    cl_int async_done = CL_COMPLETE;

    ciErrNum = clGetEventInfo(*((cl_event *)last_event->marker),
                              CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int),
                              &async_done, NULL);
    if (ciErrNum != CL_SUCCESS) {
      fprintf(stdout, "Error Querying EventInfo1!\n");
    }

    if (is_blocking(timer)) {
      /* Async operations completed after previous CPU operations:
       * overlapped time is the total CPU time since this set of async
       * operations were first issued */

      // timer to switch to is COPY or NONE
      if (async_done != CL_COMPLETE) {
        accumulate_time(&(timers->timers[hpvm_TimerID_OVERLAP].elapsed),
                        timers->async_begin, currentTime);
      }

      /* Wait on async operation completion */
      ciErrNum = clWaitForEvents(1, (cl_event *)last_event->marker);
      if (ciErrNum != CL_SUCCESS) {
        fprintf(stderr, "Error Waiting for Events!\n");
      }

      hpvm_Timestamp total_async_time = record_async_times(timers);

      /* Async operations completed before previous CPU operations:
       * overlapped time is the total async time */
      if (async_done == CL_COMPLETE) {
        // fprintf(stderr, "Async_done: total_async_type = %lld\n",
        // total_async_time);
        timers->timers[hpvm_TimerID_OVERLAP].elapsed += total_async_time;
      }

    } else
        /* implies (!is_async(timers->current) && asyncs_outstanding(timers)) */
        // i.e. Current Not Async (not KERNEL/COPY_ASYNC) but there are
        // outstanding so something is deeper in stack
        if (async_done == CL_COMPLETE) {
      /* Async operations completed before previous CPU operations:
       * overlapped time is the total async time */
      timers->timers[hpvm_TimerID_OVERLAP].elapsed +=
          record_async_times(timers);
    }
  }

  /* Start the new timer */
  if (timer != hpvm_TimerID_NONE) {
    if (!is_async(timer)) {
      hpvm_StartTimer(&timers->timers[timer]);
    } else {
      // toSwitchTo Is Async (KERNEL/COPY_ASYNC)
      if (!asyncs_outstanding(timers)) {
        /* No asyncs outstanding, insert a fresh async marker */

        insert_marker(timers, timer);
        timers->async_begin = currentTime;
      } else if (!is_async(timers->current)) {
        /* Previous asyncs still in flight, but a previous SwitchTo
         * already marked the end of the most recent async operation,
         * so we can rename that marker as the beginning of this async
         * operation */

        struct hpvm_async_time_marker_list *last_event = get_last_async(timers);
        last_event->label = NULL;
        last_event->timerID = timer;
      }
      if (!is_async(timers->current)) {
        hpvm_StartTimer(&timers->timers[hpvm_TimerID_DRIVER]);
      }
    }
  }
  timers->current = timer;
}

void hpvm_SwitchToSubTimer(struct hpvm_TimerSet *timers, char *label,
                           enum hpvm_TimerID category) {
  struct hpvm_SubTimerList *subtimerlist =
      timers->sub_timer_list[timers->current];
  struct hpvm_SubTimer *curr =
      (subtimerlist != NULL) ? subtimerlist->current : NULL;

  if (timers->current != hpvm_TimerID_NONE) {
    if (!is_async(timers->current)) {
      if (timers->current != category) {
        if (curr != NULL) {
          hpvm_StopTimerAndSubTimer(&timers->timers[timers->current],
                                    &curr->timer);
        } else {
          hpvm_StopTimer(&timers->timers[timers->current]);
        }
      } else {
        if (curr != NULL) {
          hpvm_StopTimer(&curr->timer);
        }
      }
    } else {
      insert_submarker(timers, label, category);
      if (!is_async(category)) { // if switching to async too, keep driver going
        hpvm_StopTimer(&timers->timers[hpvm_TimerID_DRIVER]);
      }
    }
  }

  hpvm_Timestamp currentTime = get_time();

  /* The only cases we check for asynchronous task completion is
   * when an overlapping CPU operation completes, or the next
   * segment blocks on completion of previous async operations */
  if (asyncs_outstanding(timers) &&
      (!is_async(timers->current) || is_blocking(category))) {

    struct hpvm_async_time_marker_list *last_event = get_last_async(timers);
    /* CL_COMPLETE if completed */

    cl_int ciErrNum = CL_SUCCESS;
    cl_int async_done = CL_COMPLETE;

    ciErrNum = clGetEventInfo(*((cl_event *)last_event->marker),
                              CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int),
                              &async_done, NULL);
    if (ciErrNum != CL_SUCCESS) {
      fprintf(stdout, "Error Querying EventInfo2!\n");
    }

    if (is_blocking(category)) {
      /* Async operations completed after previous CPU operations:
       * overlapped time is the total CPU time since this set of async
       * operations were first issued */

      // timer to switch to is COPY or NONE
      // if it hasn't already finished, then just take now and use that as the
      // elapsed time in OVERLAP anything happening after now isn't OVERLAP
      // because everything is being stopped to wait for synchronization it
      // seems that the extra sync wall time isn't being recorded anywhere
      if (async_done != CL_COMPLETE)
        accumulate_time(&(timers->timers[hpvm_TimerID_OVERLAP].elapsed),
                        timers->async_begin, currentTime);

      /* Wait on async operation completion */
      ciErrNum = clWaitForEvents(1, (cl_event *)last_event->marker);
      if (ciErrNum != CL_SUCCESS) {
        fprintf(stderr, "Error Waiting for Events!\n");
      }
      hpvm_Timestamp total_async_time = record_async_times(timers);

      /* Async operations completed before previous CPU operations:
       * overlapped time is the total async time */
      // If it did finish, then accumulate all the async time that did happen
      // into OVERLAP the immediately preceding EventSynchronize theoretically
      // didn't have any effect since it was already completed.
      if (async_done == CL_COMPLETE /*cudaSuccess*/)
        timers->timers[hpvm_TimerID_OVERLAP].elapsed += total_async_time;

    } else
        /* implies (!is_async(timers->current) && asyncs_outstanding(timers)) */
        // i.e. Current Not Async (not KERNEL/COPY_ASYNC) but there are
        // outstanding so something is deeper in stack
        if (async_done == CL_COMPLETE /*cudaSuccess*/) {
      /* Async operations completed before previous CPU operations:
       * overlapped time is the total async time */
      timers->timers[hpvm_TimerID_OVERLAP].elapsed +=
          record_async_times(timers);
    }
    // else, this isn't blocking, so just check the next time around
  }

  subtimerlist = timers->sub_timer_list[category];
  struct hpvm_SubTimer *subtimer = NULL;

  if (label != NULL) {
    subtimer = subtimerlist->subtimer_list;
    while (subtimer != NULL) {
      if (strcmp(subtimer->label, label) == 0) {
        break;
      } else {
        subtimer = subtimer->next;
      }
    }
  }

  /* Start the new timer */
  if (category != hpvm_TimerID_NONE) {
    if (!is_async(category)) {
      if (subtimerlist != NULL) {
        subtimerlist->current = subtimer;
      }

      if (category != timers->current && subtimer != NULL) {
        hpvm_StartTimerAndSubTimer(&timers->timers[category], &subtimer->timer);
      } else if (subtimer != NULL) {
        hpvm_StartTimer(&subtimer->timer);
      } else {
        hpvm_StartTimer(&timers->timers[category]);
      }
    } else {
      if (subtimerlist != NULL) {
        subtimerlist->current = subtimer;
      }

      // toSwitchTo Is Async (KERNEL/COPY_ASYNC)
      if (!asyncs_outstanding(timers)) {
        /* No asyncs outstanding, insert a fresh async marker */
        insert_submarker(timers, label, category);
        timers->async_begin = currentTime;
      } else if (!is_async(timers->current)) {
        /* Previous asyncs still in flight, but a previous SwitchTo
         * already marked the end of the most recent async operation,
         * so we can rename that marker as the beginning of this async
         * operation */

        struct hpvm_async_time_marker_list *last_event = get_last_async(timers);
        last_event->timerID = category;
        last_event->label = label;
      } // else, marker for switchToThis was already inserted

      // toSwitchto is already asynchronous, but if current/prev state is async
      // too, then DRIVER is already running
      if (!is_async(timers->current)) {
        hpvm_StartTimer(&timers->timers[hpvm_TimerID_DRIVER]);
      }
    }
  }

  timers->current = category;
}

void hpvm_PrintTimerSet(struct hpvm_TimerSet *timers) {
  hpvm_Timestamp wall_end = get_time();

  struct hpvm_Timer *t = timers->timers;
  struct hpvm_SubTimer *sub = NULL;

  int maxSubLength;

  const char *categories[] = {
      "IO",          "Kernel",         "Copy",         "Driver",
      "Copy Async",  "Compute",        "Overlap",      "Init_Ctx",
      "Clear_Ctx",   "Copy_Scalar",    "Copy_Ptr",     "Mem_Free",
      "Read_Output", "Setup",          "Mem_Track",    "Mem_Untrack",
      "Misc",        "Pthread_Create", "Arg_Pack",     "Arg_Unpack",
      "Computation", "Output_Pack",    "Output_Unpack"

  };

  const int maxCategoryLength = 20;

  int i;
  for (i = 1; i < hpvm_TimerID_LAST;
       ++i) { // exclude NONE and OVRELAP from this format
    if (hpvm_GetElapsedTime(&t[i]) != 0 || true) {

      // Print Category Timer
      printf("%-*s: %.9f\n", maxCategoryLength, categories[i - 1],
             hpvm_GetElapsedTime(&t[i]));

      if (timers->sub_timer_list[i] != NULL) {
        sub = timers->sub_timer_list[i]->subtimer_list;
        maxSubLength = 0;
        while (sub != NULL) {
          // Find longest SubTimer label
          if (strlen(sub->label) > (unsigned long)maxSubLength) {
            maxSubLength = strlen(sub->label);
          }
          sub = sub->next;
        }

        // Fit to Categories
        if (maxSubLength <= maxCategoryLength) {
          maxSubLength = maxCategoryLength;
        }

        sub = timers->sub_timer_list[i]->subtimer_list;

        // Print SubTimers
        while (sub != NULL) {
          printf(" -%-*s: %.9f\n", maxSubLength, sub->label,
                 hpvm_GetElapsedTime(&sub->timer));
          sub = sub->next;
        }
      }
    }
  }

  if (hpvm_GetElapsedTime(&t[hpvm_TimerID_OVERLAP]) != 0)
    printf("CPU/Kernel Overlap: %.9f\n",
           hpvm_GetElapsedTime(&t[hpvm_TimerID_OVERLAP]));

  float walltime = (wall_end - timers->wall_begin) / 1e9;
  printf("Timer Wall Time: %.9f\n", walltime);
}

void hpvm_DestroyTimerSet(struct hpvm_TimerSet *timers) {
  /* clean up all of the async event markers */
  struct hpvm_async_time_marker_list *event = timers->async_markers;
  while (event != NULL) {

    cl_int ciErrNum = CL_SUCCESS;
    ciErrNum = clWaitForEvents(1, (cl_event *)(event)->marker);
    if (ciErrNum != CL_SUCCESS) {
      // fprintf(stderr, "Error Waiting for Events!\n");
    }

    ciErrNum = clReleaseEvent(*((cl_event *)(event)->marker));
    if (ciErrNum != CL_SUCCESS) {
      fprintf(stderr, "Error Release Events!\n");
    }

    free((event)->marker);
    struct hpvm_async_time_marker_list *next = ((event)->next);

    free(event);

    event = next;
  }

  int i = 0;
  for (i = 0; i < hpvm_TimerID_LAST; ++i) {
    if (timers->sub_timer_list[i] != NULL) {
      struct hpvm_SubTimer *subtimer = timers->sub_timer_list[i]->subtimer_list;
      struct hpvm_SubTimer *prev = NULL;
      while (subtimer != NULL) {
        free(subtimer->label);
        prev = subtimer;
        subtimer = subtimer->next;
        free(prev);
      }
      free(timers->sub_timer_list[i]);
    }
  }
}

/**************************** Pipeline API ************************************/
#define BUFFER_SIZE 1

// Launch API for a streaming dataflow graph
void *llvm_hpvm_streamLaunch(void (*LaunchFunc)(void *, void *), void *args) {
  DFNodeContext_CPU *Context =
      (DFNodeContext_CPU *)malloc(sizeof(DFNodeContext_CPU));

  Context->threads = new std::vector<pthread_t>();
  Context->ArgInPortSizeMap = new std::map<unsigned, uint64_t>();
  Context->BindInSourcePort = new std::vector<unsigned>();
  Context->BindOutSizes = new std::vector<uint64_t>();
  Context->EdgeSizes = new std::vector<uint64_t>();
  Context->BindInputBuffers = new std::vector<CircularBuffer<uint64_t> *>();
  Context->BindOutputBuffers = new std::vector<CircularBuffer<uint64_t> *>();
  Context->EdgeBuffers = new std::vector<CircularBuffer<uint64_t> *>();
  Context->isLastInputBuffers = new std::vector<CircularBuffer<uint64_t> *>();

  DEBUG(cout << "StreamLaunch -- Graph: " << Context << ", Arguments: " << args
             << flush << "\n");
  LaunchFunc(args, Context);
  return Context;
}

// Push API for a streaming dataflow graph
void llvm_hpvm_streamPush(void *graphID, void *args) {
  DEBUG(cout << "StreamPush -- Graph: " << graphID << ", Arguments: " << args
             << flush << "\n");
  DFNodeContext_CPU *Ctx = (DFNodeContext_CPU *)graphID;
  unsigned offset = 0;
  for (unsigned i = 0; i < Ctx->ArgInPortSizeMap->size(); i++) {
    uint64_t element;
    memcpy(&element, (char *)args + offset, Ctx->ArgInPortSizeMap->at(i));
    offset += Ctx->ArgInPortSizeMap->at(i);
    for (unsigned j = 0; j < Ctx->BindInputBuffers->size(); j++) {
      if (Ctx->BindInSourcePort->at(j) == i) {
        // Push to all bind buffers connected to parent node at this port
        llvm_hpvm_bufferPush(Ctx->BindInputBuffers->at(j), element);
      }
    }
  }
  // Push 0 in isLastInput buffers of all child nodes
  for (CircularBuffer<uint64_t> *buffer : *(Ctx->isLastInputBuffers))
    llvm_hpvm_bufferPush(buffer, 0);
}

// Pop API for a streaming dataflow graph
void *llvm_hpvm_streamPop(void *graphID) {
  DEBUG(cout << "StreamPop -- Graph: " << graphID << flush << "\n");
  DFNodeContext_CPU *Ctx = (DFNodeContext_CPU *)graphID;
  unsigned totalBytes = 0;
  for (uint64_t size : *(Ctx->BindOutSizes))
    totalBytes += size;
  void *output = malloc(totalBytes);
  unsigned offset = 0;
  for (unsigned i = 0; i < Ctx->BindOutputBuffers->size(); i++) {
    uint64_t element = llvm_hpvm_bufferPop(Ctx->BindOutputBuffers->at(i));
    memcpy((char *)output + offset, &element, Ctx->BindOutSizes->at(i));
    offset += Ctx->BindOutSizes->at(i);
  }
  return output;
}

// Wait API for a streaming dataflow graph
void llvm_hpvm_streamWait(void *graphID) {
  DEBUG(cout << "StreamWait -- Graph: " << graphID << flush << "\n");
  DFNodeContext_CPU *Ctx = (DFNodeContext_CPU *)graphID;
  // Push garbage to all other input buffers
  for (unsigned i = 0; i < Ctx->BindInputBuffers->size(); i++) {
    uint64_t element = 0;
    llvm_hpvm_bufferPush(Ctx->BindInputBuffers->at(i), element);
  }
  // Push 1 in isLastInput buffers of all child nodes
  for (unsigned i = 0; i < Ctx->isLastInputBuffers->size(); i++)
    llvm_hpvm_bufferPush(Ctx->isLastInputBuffers->at(i), 1);

  llvm_hpvm_freeThreads(graphID);
}

// Create a buffer and return the bufferID
void *llvm_hpvm_createBindInBuffer(void *graphID, uint64_t size,
                                   unsigned inArgPort) {
  DEBUG(cout << "Create BindInBuffer -- Graph: " << graphID
             << ", Size: " << size << flush << "\n");
  DFNodeContext_CPU *Context = (DFNodeContext_CPU *)graphID;
  CircularBuffer<uint64_t> *bufferID =
      new CircularBuffer<uint64_t>(BUFFER_SIZE, "BindIn");
  DEBUG(cout << "\tNew Buffer: " << bufferID << flush << "\n");
  Context->BindInputBuffers->push_back(bufferID);
  (*(Context->ArgInPortSizeMap))[inArgPort] = size;
  Context->BindInSourcePort->push_back(inArgPort);
  // Context->BindInSizes->push_back(size);
  return bufferID;
}

void *llvm_hpvm_createBindOutBuffer(void *graphID, uint64_t size) {
  DEBUG(cout << "Create BindOutBuffer -- Graph: " << graphID
             << ", Size: " << size << flush << "\n");
  DFNodeContext_CPU *Context = (DFNodeContext_CPU *)graphID;
  CircularBuffer<uint64_t> *bufferID =
      new CircularBuffer<uint64_t>(BUFFER_SIZE, "BindOut");
  DEBUG(cout << "\tNew Buffer: " << bufferID << flush << "\n");
  Context->BindOutputBuffers->push_back(bufferID);
  Context->BindOutSizes->push_back(size);
  return bufferID;
}
void *llvm_hpvm_createEdgeBuffer(void *graphID, uint64_t size) {
  DEBUG(cout << "Create EdgeBuffer -- Graph: " << graphID << ", Size: " << size
             << flush << "\n");
  DFNodeContext_CPU *Context = (DFNodeContext_CPU *)graphID;
  CircularBuffer<uint64_t> *bufferID =
      new CircularBuffer<uint64_t>(BUFFER_SIZE, "Edge");
  DEBUG(cout << "\tNew Buffer: " << bufferID << flush << "\n");
  Context->EdgeBuffers->push_back(bufferID);
  Context->EdgeSizes->push_back(size);
  return bufferID;
}

void *llvm_hpvm_createLastInputBuffer(void *graphID, uint64_t size) {
  DEBUG(cout << "Create isLastInputBuffer -- Graph: " << graphID
             << ", Size: " << size << flush << "\n");
  DFNodeContext_CPU *Context = (DFNodeContext_CPU *)graphID;
  CircularBuffer<uint64_t> *bufferID =
      new CircularBuffer<uint64_t>(BUFFER_SIZE, "LastInput");
  DEBUG(cout << "\tNew Buffer: " << bufferID << flush << "\n");
  Context->isLastInputBuffers->push_back(bufferID);
  return bufferID;
}

// Free buffers
void llvm_hpvm_freeBuffers(void *graphID) {
  DEBUG(cout << "Free all buffers -- Graph: " << graphID << flush << "\n");
  DFNodeContext_CPU *Context = (DFNodeContext_CPU *)graphID;
  for (CircularBuffer<uint64_t> *bufferID : *(Context->BindInputBuffers))
    delete bufferID;
  for (CircularBuffer<uint64_t> *bufferID : *(Context->BindOutputBuffers))
    delete bufferID;
  for (CircularBuffer<uint64_t> *bufferID : *(Context->EdgeBuffers))
    delete bufferID;
  for (CircularBuffer<uint64_t> *bufferID : *(Context->isLastInputBuffers))
    delete bufferID;
}

// Pop an element from the buffer
uint64_t llvm_hpvm_bufferPop(void *bufferID) {
  CircularBuffer<uint64_t> *buffer = (CircularBuffer<uint64_t> *)bufferID;
  return buffer->pop();
}

// Push an element into the buffer
void llvm_hpvm_bufferPush(void *bufferID, uint64_t element) {
  CircularBuffer<uint64_t> *buffer = (CircularBuffer<uint64_t> *)bufferID;
  buffer->push(element);
}

// Create a thread
void llvm_hpvm_createThread(void *graphID, void *(*Func)(void *),
                            void *arguments) {
  DEBUG(cout << "Create Thread -- Graph: " << graphID << ", Func: " << Func
             << ", Args: " << arguments << flush << "\n");
  DFNodeContext_CPU *Ctx = (DFNodeContext_CPU *)graphID;
  int err;
  pthread_t threadID;
  if ((err = pthread_create(&threadID, NULL, Func, arguments)) != 0)
    cout << "Failed to create thread. Error code = " << err << flush << "\n";

  Ctx->threads->push_back(threadID);
}

// Wait for thread to finish
void llvm_hpvm_freeThreads(void *graphID) {
  DEBUG(cout << "Free Threads -- Graph: " << graphID << flush << "\n");
  DFNodeContext_CPU *Ctx = (DFNodeContext_CPU *)graphID;
  for (pthread_t thread : *(Ctx->threads))
    pthread_join(thread, NULL);
}

/************************ OPENCL & PTHREAD API ********************************/

void *llvm_hpvm_cpu_launch(void *(*rootFunc)(void *), void *arguments) {
  DFNodeContext_CPU *Context =
      (DFNodeContext_CPU *)malloc(sizeof(DFNodeContext_CPU));
  // int err;
  // if((err = pthread_create(&Context->threadID, NULL, rootFunc, arguments)) !=
  // 0) cout << "Failed to create pthread. Error code = " << err << flush <<
  // "\n";
  rootFunc(arguments);
  return Context;
}

void llvm_hpvm_cpu_wait(void *graphID) {
  DEBUG(cout << "Waiting for pthread to finish ...\n");
  free(graphID);
  DEBUG(cout << "\t... pthread Done!\n");
}

// Returns the platform name.
std::string getPlatformName(cl_platform_id pid) {
  cl_int status;

  size_t sz;
  status = clGetPlatformInfo(pid, CL_PLATFORM_NAME, 0, NULL, &sz);
  checkErr(status, CL_SUCCESS, "Query for platform name size failed");

  char *name = new char[sz];
  status = clGetPlatformInfo(pid, CL_PLATFORM_NAME, sz, name, NULL);
  checkErr(status, CL_SUCCESS, "Query for platform name failed");

  const auto &tmp = std::string(name, name + sz);
  delete[] name;
  return tmp;
}

// Searches all platforms for the first platform whose name
// contains the search string (case-insensitive).
cl_platform_id findPlatform(const char *platform_name_search) {
  cl_int status;

  std::string search = platform_name_search;
  std::transform(search.begin(), search.end(), search.begin(), ::tolower);

  // Get number of platforms.
  cl_uint num_platforms;
  status = clGetPlatformIDs(0, NULL, &num_platforms);
  checkErr(status, CL_SUCCESS, "Query for number of platforms failed");

  // Get a list of all platform ids.
  cl_platform_id *pids =
      (cl_platform_id *)malloc(sizeof(cl_platform_id) * num_platforms);
  status = clGetPlatformIDs(num_platforms, pids, NULL);
  checkErr(status, CL_SUCCESS, "Query for all platform ids failed");

  // For each platform, get name and compare against the search string.
  for (unsigned i = 0; i < num_platforms; ++i) {
    std::string name = getPlatformName(pids[i]);

    // Convert to lower case.
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    if (name.find(search) != std::string::npos) {
      // Found!
      cl_platform_id pid = pids[i];
      free(pids);
      return pid;
    }
  }

  free(pids);
  // No platform found.
  assert(false && "No matching platform found!");
}

void *llvm_hpvm_ocl_initContext(enum hpvm::Target T) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(std::string Target = T == hpvm::GPU_TARGET ? "GPU" : "SPIR");
  DEBUG(cout << "Initializing Context for " << Target << " device\n");
  cl_uint numPlatforms;
  cl_int errcode;
  errcode = clGetPlatformIDs(0, NULL, &numPlatforms);
  checkErr(errcode, CL_SUCCESS, "Failure to get number of platforms");

  // now get all the platform IDs
  cl_platform_id *platforms =
      (cl_platform_id *)malloc(sizeof(cl_platform_id) * numPlatforms);
  errcode = clGetPlatformIDs(numPlatforms, platforms, NULL);
  checkErr(errcode, CL_SUCCESS, "Failure to get platform IDs");

  for (unsigned i = 0; i < numPlatforms; i++) {
    char buffer[10240];
    DEBUG(cout << "Device " << i << " Info -->\n");
    clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 10240, buffer, NULL);
    DEBUG(cout << "\tPROFILE = " << buffer << flush << "\n");
    clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, 10240, buffer, NULL);
    DEBUG(cout << "\tVERSION = " << buffer << flush << "\n");
    clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 10240, buffer, NULL);
    DEBUG(cout << "\tNAME = " << buffer << flush << "\n");
    clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, 10240, buffer, NULL);
    DEBUG(cout << "\tVENDOR = " << buffer << flush << "\n");
    clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, 10240, buffer,
                      NULL);
    DEBUG(cout << "\tEXTENSIONS = " << buffer << flush << "\n");
  }
  cl_platform_id platformId;
  if (T == hpvm::GPU_TARGET) {
    platformId = findPlatform("nvidia");
    char buffer[10240];
    DEBUG(cout << "Found NVIDIA Device \n");
    clGetPlatformInfo(platformId, CL_PLATFORM_PROFILE, 10240, buffer, NULL);
    DEBUG(cout << "\tPROFILE = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_VERSION, 10240, buffer, NULL);
    DEBUG(cout << "\tVERSION = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_NAME, 10240, buffer, NULL);
    DEBUG(cout << "\tNAME = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_VENDOR, 10240, buffer, NULL);
    DEBUG(cout << "\tVENDOR = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
    DEBUG(cout << "\tEXTENSIONS = " << buffer << flush << "\n");
  } else {
    platformId = findPlatform("intel");
    char buffer[10240];
    DEBUG(cout << "Found Intel Device \n");
    clGetPlatformInfo(platformId, CL_PLATFORM_PROFILE, 10240, buffer, NULL);
    DEBUG(cout << "\tPROFILE = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_VERSION, 10240, buffer, NULL);
    DEBUG(cout << "\tVERSION = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_NAME, 10240, buffer, NULL);
    DEBUG(cout << "\tNAME = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_VENDOR, 10240, buffer, NULL);
    DEBUG(cout << "\tVENDOR = " << buffer << flush << "\n");
    clGetPlatformInfo(platformId, CL_PLATFORM_EXTENSIONS, 10240, buffer, NULL);
    DEBUG(cout << "\tEXTENSIONS = " << buffer << flush << "\n");
  }
  DEBUG(cout << "Found plarform with id: " << platformId << "\n");
  cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, (long)platformId,
                                        0};
  globalOCLContext = clCreateContextFromType(
      properties,
      T == hpvm::GPU_TARGET ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, NULL,
      NULL, &errcode);
  checkErr(errcode, CL_SUCCESS, "Failure to create context");
  // get the list of OCL devices associated with context
  size_t dataBytes;
  errcode = clGetContextInfo(globalOCLContext, CL_CONTEXT_DEVICES, 0, NULL,
                             &dataBytes);
  checkErr(errcode, CL_SUCCESS, "Failure to get context info length");

  DEBUG(cout << "Got databytes: " << dataBytes << "\n");

  clDevices = (cl_device_id *)malloc(dataBytes);
  errcode |= clGetContextInfo(globalOCLContext, CL_CONTEXT_DEVICES, dataBytes,
                              clDevices, NULL);
  checkErr(errcode, CL_SUCCESS, "Failure to get context info");
  if (false && T == hpvm::SPIR_TARGET) {
    cl_device_partition_property props[4];
    props[0] = CL_DEVICE_PARTITION_BY_COUNTS;
    props[1] = NUM_CORES;
    props[2] = CL_DEVICE_PARTITION_BY_COUNTS_LIST_END;
    props[3] = 0;
    cl_device_id subdevice_id[8];
    cl_uint num_entries = 8;

    cl_uint numDevices;
    clCreateSubDevices(clDevices[0], props, num_entries, subdevice_id,
                       &numDevices);
    clDevices[0] = subdevice_id[0];
    globalOCLContext =
        clCreateContext(properties, 1, clDevices, NULL, NULL, &errcode);
    checkErr(errcode, CL_SUCCESS, "Failure to create OCL context");
  }

  free(platforms);
  DEBUG(cout << "\tContext " << globalOCLContext << flush << "\n");
  checkErr(errcode, CL_SUCCESS, "Failure to create OCL context");

  DEBUG(cout << "Initialize Kernel Timer\n");
  hpvm_InitializeTimerSet(&kernel_timer);

  pthread_mutex_unlock(&ocl_mtx);
  return globalOCLContext;
}

void llvm_hpvm_ocl_clearContext(void *graphID) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Clear Context\n");
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  // FIXME: Have separate function to release command queue and clear context.
  // Would be useful when a context has multiple command queues
  clReleaseKernel(Context->clKernel);
  free(Context);
  DEBUG(cout << "Done with OCL kernel\n");
  cout << "Printing HPVM Timer: KernelTimer\n";
  hpvm_PrintTimerSet(&kernel_timer);
  pthread_mutex_unlock(&ocl_mtx);
}

void llvm_hpvm_ocl_argument_shared(void *graphID, int arg_index, size_t size) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Set Shared Memory Input:");
  DEBUG(cout << "\tArgument Index = " << arg_index << ", Size = " << size
             << flush << "\n");
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  DEBUG(cout << "Using Context: " << Context << flush << "\n");
  DEBUG(cout << "Using clKernel: " << Context->clKernel << flush << "\n");
  cl_int errcode = clSetKernelArg(Context->clKernel, arg_index, size, NULL);
  checkErr(errcode, CL_SUCCESS, "Failure to set shared memory argument");
  pthread_mutex_unlock(&ocl_mtx);
}

void llvm_hpvm_ocl_argument_scalar(void *graphID, void *input, int arg_index,
                                   size_t size) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Set Scalar Input:");
  DEBUG(cout << "\tArgument Index = " << arg_index << ", Size = " << size
             << flush << "\n");
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  DEBUG(cout << "Using Context: " << Context << flush << "\n");
  DEBUG(cout << "Using clKernel: " << Context->clKernel << flush << "\n");
  cl_int errcode = clSetKernelArg(Context->clKernel, arg_index, size, input);
  checkErr(errcode, CL_SUCCESS, "Failure to set constant input argument");
  pthread_mutex_unlock(&ocl_mtx);
}

void *llvm_hpvm_ocl_argument_ptr(void *graphID, void *input, int arg_index,
                                 size_t size, bool isInput, bool isOutput) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Set Pointer Input:");
  DEBUG(cout << "\tArgument Index = " << arg_index << ", Ptr = " << input
             << ", Size = " << size << flush << "\n");
  // Size should be non-zero
  assert(size != 0 && "Size of data pointed to has to be non-zero!");
  DEBUG(cout << "\tInput = " << isInput << "\tOutput = " << isOutput << flush
             << "\n");
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;

  pthread_mutex_unlock(&ocl_mtx);
  // Check with runtime the location of this memory
  cl_mem d_input = (cl_mem)llvm_hpvm_ocl_request_mem(input, size, Context,
                                                     isInput, isOutput);

  pthread_mutex_lock(&ocl_mtx);
  // Set Kernel Argument
  cl_int errcode = clSetKernelArg(Context->clKernel, arg_index, sizeof(cl_mem),
                                  (void *)&d_input);
  checkErr(errcode, CL_SUCCESS, "Failure to set pointer argument");
  DEBUG(cout << "\tDevicePtr = " << d_input << flush << "\n");
  pthread_mutex_unlock(&ocl_mtx);
  return d_input;
}

void *llvm_hpvm_ocl_output_ptr(void *graphID, int arg_index, size_t size) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Set device memory for Output Struct:");
  DEBUG(cout << "\tArgument Index = " << arg_index << ", Size = " << size
             << flush << "\n");
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  cl_int errcode;
  cl_mem d_output = clCreateBuffer(Context->clOCLContext, CL_MEM_WRITE_ONLY,
                                   size, NULL, &errcode);
  checkErr(errcode, CL_SUCCESS, "Failure to create output buffer on device");
  errcode = clSetKernelArg(Context->clKernel, arg_index, sizeof(cl_mem),
                           (void *)&d_output);
  checkErr(errcode, CL_SUCCESS, "Failure to set pointer argument");
  DEBUG(cout << "\tDevicePtr = " << d_output << flush << "\n");
  pthread_mutex_unlock(&ocl_mtx);
  return d_output;
}

void llvm_hpvm_ocl_free(void *ptr) {
}

void *llvm_hpvm_ocl_getOutput(void *graphID, void *h_output, void *d_output,
                              size_t size) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Get Output:\n");
  DEBUG(cout << "\tHostPtr = " << h_output << ", DevicePtr = " << d_output
             << ", Size = " << size << flush << "\n");
  if (h_output == NULL)
    h_output = malloc(size);
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  cl_int errcode =
      clEnqueueReadBuffer(Context->clCommandQue, (cl_mem)d_output, CL_TRUE, 0,
                          size, h_output, 0, NULL, NULL);
  checkErr(errcode, CL_SUCCESS, "[getOutput] Failure to read output");
  pthread_mutex_unlock(&ocl_mtx);
  return h_output;
}

void *llvm_hpvm_ocl_executeNode(void *graphID, unsigned workDim,
                                const size_t *localWorkSize,
                                const size_t *globalWorkSize) {
  pthread_mutex_lock(&ocl_mtx);

  size_t GlobalWG[3];
  size_t LocalWG[3];

  // OpenCL EnqeueNDRangeKernel function results in segementation fault if we
  // directly use local and global work groups arguments. Hence, allocating it
  // on stack and copying.
  for (unsigned i = 0; i < workDim; i++) {
    GlobalWG[i] = globalWorkSize[i];
  }

  // OpenCL allows local workgroup to be null.
  if (localWorkSize != NULL) {
    for (unsigned i = 0; i < workDim; i++) {
      LocalWG[i] = localWorkSize[i];
    }
  }

  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  // TODO: Would like to use event to ensure better scheduling of kernels.
  // Currently passing the event paratemeter results in seg fault with
  // clEnqueueNDRangeKernel.
  DEBUG(cout << "Enqueuing kernel:\n");
  DEBUG(cout << "\tCommand Queue: " << Context->clCommandQue << flush << "\n");
  DEBUG(cout << "\tKernel: " << Context->clKernel << flush << "\n");
  DEBUG(cout << "\tNumber of dimensions: " << workDim << flush << "\n");
  DEBUG(cout << "\tGlobal Work Group: ( ");
  for (unsigned i = 0; i < workDim; i++) {
    DEBUG(cout << GlobalWG[i] << " ");
  }
  DEBUG(cout << ")\n");
  if (localWorkSize != NULL) {
    DEBUG(cout << "\tLocal Work Group: ( ");
    for (unsigned i = 0; i < workDim; i++) {
      DEBUG(cout << LocalWG[i] << " ");
    }
    DEBUG(cout << ")\n");
  }
  clFinish(Context->clCommandQue);
  hpvm_SwitchToTimer(&kernel_timer, hpvm_TimerID_COMPUTATION);
  cl_int errcode = clEnqueueNDRangeKernel(
      Context->clCommandQue, Context->clKernel, workDim, NULL, GlobalWG,
      (localWorkSize == NULL) ? NULL : LocalWG, 0, NULL, NULL);
  checkErr(errcode, CL_SUCCESS, "Failure to enqueue kernel");
  clFinish(Context->clCommandQue);
  hpvm_SwitchToTimer(&kernel_timer, hpvm_TimerID_NONE);

  pthread_mutex_unlock(&ocl_mtx);
  return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//! Loads a Program binary file.
//!
//! @return the source string if succeeded, 0 otherwise
//! @param Filename        program filename
//! @param szFinalLength    returned length of the code string
//////////////////////////////////////////////////////////////////////////////
static char *LoadProgSource(const char *Filename, size_t *szFinalLength) {
  DEBUG(cout << "Load Prog Source\n");
  // locals
  FILE *pFileStream = NULL;
  size_t szSourceLength;

  // open the OpenCL source code file
  pFileStream = fopen(Filename, "rb");
  if (pFileStream == 0) {
    return NULL;
  }

  // get the length of the source code
  fseek(pFileStream, 0, SEEK_END);
  szSourceLength = ftell(pFileStream);
  fseek(pFileStream, 0, SEEK_SET);

  // allocate a buffer for the source code string and read it in
  char *cSourceString = (char *)malloc(szSourceLength + 1);
  if (fread((cSourceString), szSourceLength, 1, pFileStream) != 1) {
    fclose(pFileStream);
    free(cSourceString);
    return 0;
  }

  // close the file and return the total length of the combined (preamble +
  // source) string
  fclose(pFileStream);
  if (szFinalLength != 0) {
    *szFinalLength = szSourceLength;
  }
  cSourceString[szSourceLength] = '\0';

  return cSourceString;
}

void *llvm_hpvm_ocl_launch(const char *FileName, const char *KernelName) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Launch OCL Kernel\n");
  // Initialize OpenCL

  // OpenCL specific variables
  DFNodeContext_OCL *Context =
      (DFNodeContext_OCL *)malloc(sizeof(DFNodeContext_OCL));

  size_t kernelLength;
  cl_int errcode;

  // For a single context for all kernels
  Context->clOCLContext = globalOCLContext;

  // Create a command-queue
  Context->clCommandQue = clCreateCommandQueue(
      Context->clOCLContext, clDevices[0], CL_QUEUE_PROFILING_ENABLE, &errcode);
  globalCommandQue = Context->clCommandQue;
  checkErr(errcode, CL_SUCCESS, "Failure to create command queue");

  DEBUG(cout << "Loading program binary: " << FileName << flush << "\n");
  char *programSource = LoadProgSource(FileName, &kernelLength);
  checkErr(programSource != NULL, 1 /*bool true*/,
           "Failure to load Program Binary");

  Context->clProgram = clCreateProgramWithSource(
      Context->clOCLContext, 1, (const char **)&programSource, NULL, &errcode);
  checkErr(errcode, CL_SUCCESS, "Failure to create program from binary");

  DEBUG(cout << "Building kernel - " << KernelName << " from file " << FileName
             << flush << "\n");
  errcode =
      clBuildProgram(Context->clProgram, 1, &clDevices[0], "", NULL, NULL);
  // If build fails, get build log from device
  if (errcode != CL_SUCCESS) {
    cout << "ERROR: Failure to build program\n";
    size_t len = 0;
    errcode = clGetProgramBuildInfo(Context->clProgram, clDevices[0],
                                    CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
    cout << "LOG LENGTH: " << len << flush << "\n";
    checkErr(errcode, CL_SUCCESS,
             "Failure to collect program build log length");
    char *log = (char *)malloc(len * sizeof(char));
    errcode = clGetProgramBuildInfo(Context->clProgram, clDevices[0],
                                    CL_PROGRAM_BUILD_LOG, len, log, NULL);
    checkErr(errcode, CL_SUCCESS, "Failure to collect program build log");

    cout << "Device Build Log:\n" << log << flush << "\n";
    free(log);
    pthread_mutex_unlock(&ocl_mtx);
    exit(EXIT_FAILURE);
  }

  Context->clKernel = clCreateKernel(Context->clProgram, KernelName, &errcode);
  checkErr(errcode, CL_SUCCESS, "Failure to create kernel");

  DEBUG(cout << "Kernel ID = " << Context->clKernel << "\n");
  free(programSource);

  pthread_mutex_unlock(&ocl_mtx);
  return Context;
}

void llvm_hpvm_ocl_wait(void *graphID) {
  pthread_mutex_lock(&ocl_mtx);
  DEBUG(cout << "Wait\n");
  DFNodeContext_OCL *Context = (DFNodeContext_OCL *)graphID;
  clFinish(Context->clCommandQue);
  pthread_mutex_unlock(&ocl_mtx);
}

void llvm_hpvm_switchToTimer(void **timerSet, enum hpvm_TimerID timer) {
  pthread_mutex_lock(&ocl_mtx);
  pthread_mutex_unlock(&ocl_mtx);
}
void llvm_hpvm_printTimerSet(void **timerSet, char *timerName) {
  pthread_mutex_lock(&ocl_mtx);
  cout << "Printing HPVM Timer: ";
  if (timerName != NULL)
    cout << timerName << flush << "\n";
  else
    cout << "Anonymous\n";
  hpvm_PrintTimerSet((hpvm_TimerSet *)(*timerSet));
  pthread_mutex_unlock(&ocl_mtx);
}

void *llvm_hpvm_initializeTimerSet() {
  pthread_mutex_lock(&ocl_mtx);
  hpvm_TimerSet *TS = (hpvm_TimerSet *)malloc(sizeof(hpvm_TimerSet));
  hpvm_InitializeTimerSet(TS);
  pthread_mutex_unlock(&ocl_mtx);
  return TS;
}
