/*
 *
 * (c) 2010 The Board of Trustees of the University of Illinois.
 */
#ifndef HPVM_RT_HEADER
#define HPVM_RT_HEADER

#include <ctime>
#include <iostream>
#include <map>
#include <pthread.h>
#include <string>
#include <vector>

#include "../../include/SupportHPVM/HPVMHint.h"
#include "../../include/SupportHPVM/HPVMTimer.h"

#ifndef DEBUG_BUILD
#define DEBUG(s)                                                               \
  {}
#else
#define DEBUG(s) s
#endif

using namespace std;

extern "C" {

/********************* DFG Depth Stack **********************************/
class DFGDepth {
private:
  unsigned numDim;
  unsigned dimLimit[3];
  unsigned dimInstance[3];

public:
  DFGDepth() = default;

  DFGDepth(unsigned n, unsigned dimX = 0, unsigned iX = 0, unsigned dimY = 0,
           unsigned iY = 0, unsigned dimZ = 0, unsigned iZ = 0) {
    assert(n <= 3 && "Error! More than 3 dimensions not supported");
    numDim = n;
    dimLimit[0] = dimX;
    dimLimit[1] = dimY;
    dimLimit[2] = dimZ;
    dimInstance[0] = iX;
    dimInstance[1] = iY;
    dimInstance[2] = iZ;
  }

  unsigned getDimLimit(unsigned dim) const {
    assert(dim <= numDim &&
           "Error! Requested dimension limit is not specified");
    return dimLimit[dim];
  }

  unsigned getDimInstance(unsigned dim) const {
    assert(dim <= numDim &&
           "Error! Requested dimension instance is not specified");
    return dimInstance[dim];
  }

  unsigned getNumDim() const { return numDim; }
};

void llvm_hpvm_cpu_dstack_push(unsigned n, uint64_t limitX = 0, uint64_t iX = 0,
                               uint64_t limitY = 0, uint64_t iY = 0,
                               uint64_t limitZ = 0, uint64_t iZ = 0);
void llvm_hpvm_cpu_dstack_pop();
uint64_t llvm_hpvm_cpu_getDimLimit(unsigned level, unsigned dim);
uint64_t llvm_hpvm_cpu_getDimInstance(unsigned level, unsigned dim);

/********************* Memory Tracker **********************************/
class MemTrackerEntry {
public:
  enum Location { HOST, DEVICE };

private:
  size_t size;
  Location loc;
  void *addr;
  void *Context;

public:
  MemTrackerEntry(size_t _size, Location _loc, void *_addr, void *_Context)
      : size(_size), loc(_loc), addr(_addr), Context(_Context) {}

  size_t getSize() const { return size; }

  Location getLocation() const { return loc; }

  void *getAddress() const { return addr; }

  void *getContext() const { return Context; }

  void update(Location _loc, void *_addr, void *_Context = NULL) {
    loc = _loc;
    addr = _addr;
    Context = _Context;
  }

  void print() {
    cout << "Size = " << size << "\tLocation = " << loc
         << "\tAddress = " << addr << "\tContext = " << Context;
  }
};

class MemTracker {

private:
  std::map<void *, MemTrackerEntry *> Table;

public:
  MemTracker() {}

  bool insert(void *ID, size_t size, MemTrackerEntry::Location loc, void *addr,
              void *Context = NULL) {
    MemTrackerEntry *MTE = new MemTrackerEntry(size, loc, addr, Context);
    Table.insert(std::pair<void *, MemTrackerEntry *>(ID, MTE));
    return MTE != NULL;
  }

  MemTrackerEntry *lookup(void *ID) {
    if (Table.count(ID) == 0)
      return NULL;
    return Table[ID];
  }

  void remove(void *ID) {
    MemTrackerEntry *MTE = Table[ID];
    free(MTE);
    Table.erase(ID);
  }

  void print() {
    cout << "Printing Table ... Size = " << Table.size() << flush << "\n";
    for (auto &Entry : Table) {
      cout << Entry.first << ":\t";
      Entry.second->print();
      cout << flush << "\n";
    }
  }
};

void llvm_hpvm_track_mem(void *, size_t);
void llvm_hpvm_untrack_mem(void *);
void *llvm_hpvm_request_mem(void *, size_t);

/*********************** OPENCL & PTHREAD API **************************/
void *llvm_hpvm_cpu_launch(void *(void *), void *);
void llvm_hpvm_cpu_wait(void *);
void *llvm_hpvm_ocl_initContext(enum hpvm::Target);

void *llvm_hpvm_cpu_argument_ptr(void *, size_t);

void llvm_hpvm_ocl_clearContext(void *);
void llvm_hpvm_ocl_argument_shared(void *, int, size_t);
void llvm_hpvm_ocl_argument_scalar(void *, void *, int, size_t);
void *llvm_hpvm_ocl_argument_ptr(void *, void *, int, size_t, bool, bool);
void *llvm_hpvm_ocl_output_ptr(void *, int, size_t);
void llvm_hpvm_ocl_free(void *);
void *llvm_hpvm_ocl_getOutput(void *, void *, void *, size_t);
void *llvm_hpvm_ocl_executeNode(void *, unsigned, const size_t *,
                                const size_t *);
void *llvm_hpvm_ocl_launch(const char *, const char *);
void llvm_hpvm_ocl_wait(void *);

void llvm_hpvm_switchToTimer(void **timerSet, enum hpvm_TimerID);
void llvm_hpvm_printTimerSet(void **timerSet, char *timerName = NULL);
void *llvm_hpvm_initializeTimerSet();
}

/*************************** Pipeline API ******************************/
// Circular Buffer class
unsigned counter = 0;
template <class ElementType> class CircularBuffer {
private:
  int numElements;
  int bufferSize;
  int Head;
  int Tail;
  pthread_mutex_t mtx;
  pthread_cond_t cv;
  vector<ElementType> buffer;
  std::string name;
  unsigned ID;

public:
  CircularBuffer(int maxElements, std::string _name = "ANON") {
    ID = counter;
    Head = 0;
    Tail = 0;
    numElements = 0;
    name = _name;
    bufferSize = maxElements + 1;
    buffer.reserve(bufferSize);
    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init(&cv, NULL);
    counter++;
  }

  bool push(ElementType E);
  ElementType pop();
};

template <class ElementType>
bool CircularBuffer<ElementType>::push(ElementType E) {
  pthread_mutex_lock(&mtx);
  if ((Head + 1) % bufferSize == Tail) {
    pthread_cond_wait(&cv, &mtx);
  }
  buffer[Head] = E;
  Head = (Head + 1) % bufferSize;
  numElements++;
  pthread_mutex_unlock(&mtx);
  pthread_cond_signal(&cv);
  return true;
}

template <class ElementType> ElementType CircularBuffer<ElementType>::pop() {
  pthread_mutex_lock(&mtx);
  if (Tail == Head) {
    pthread_cond_wait(&cv, &mtx);
  }
  ElementType E = buffer[Tail];
  Tail = (Tail + 1) % bufferSize;
  numElements--;
  pthread_mutex_unlock(&mtx);
  pthread_cond_signal(&cv);
  return E;
}

extern "C" {
// Functions to push and pop values from pipeline buffers
uint64_t llvm_hpvm_bufferPop(void *);
void llvm_hpvm_bufferPush(void *, uint64_t);

// Functions to create and destroy buffers
void *llvm_hpvm_createBindInBuffer(void *, uint64_t, unsigned);
void *llvm_hpvm_createBindOutBuffer(void *, uint64_t);
void *llvm_hpvm_createEdgeBuffer(void *, uint64_t);
void *llvm_hpvm_createLastInputBuffer(void *, uint64_t);

void llvm_hpvm_freeBuffers(void *);

// Functions to create and destroy threads
void llvm_hpvm_createThread(void *graphID, void *(*Func)(void *), void *);
void llvm_hpvm_freeThreads(void *);

// Launch API for a streaming graph.
// Arguments:
// (1) Launch Function: void* (void*, void*)
// (2) Push Function:   void (void*, std::vector<uint64_t>**, unsgined)
// (3) Pop Function:    void* (std::vector<uint64_t>**, unsigned)
void *llvm_hpvm_streamLaunch(void (*LaunchFunc)(void *, void *), void *);
void llvm_hpvm_streamPush(void *graphID, void *args);
void *llvm_hpvm_streamPop(void *graphID);
void llvm_hpvm_streamWait(void *graphID);
}

#endif // HPVM_RT_HEADER
