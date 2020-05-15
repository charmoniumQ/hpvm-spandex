#include "utility.h"
#include "defs.h"
#include <assert.h>
#include <stdlib.h>

void *malloc_aligned(size_t size) {
  void *ptr = NULL;
  int err = posix_memalign((void **)&ptr, CACHELINE_SIZE, size);
  assert(err == 0 && "Failed to allocate memory!");
  return ptr;
}
