// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2017 University of Massachusetts, Amherst

#include "thread_local_heap.h"

namespace mesh {

__thread ThreadLocalHeap::ThreadLocalData ThreadLocalHeap::_threadLocalData ATTR_INITIAL_EXEC CACHELINE_ALIGNED;

ThreadLocalHeap *ThreadLocalHeap::CreateThreadLocalHeap() {
  void *buf = mesh::internal::Heap().malloc(sizeof(ThreadLocalHeap) + 2 * CACHELINE_SIZE);
  if (buf == nullptr) {
    mesh::debug("mesh: unable to allocate ThreadLocalHeap, aborting.\n");
    abort();
  }

  void *alignedPtr = (void *)(((uintptr_t)buf + CACHELINE_SIZE - 1) & ~(CACHELINE_SIZE - 1));

  return new (alignedPtr) ThreadLocalHeap(&mesh::runtime().heap());
}

ThreadLocalHeap *ThreadLocalHeap::GetHeap() {
  auto heap = GetFastPathHeap();
  if (heap == nullptr) {
    heap = CreateThreadLocalHeap();
    _threadLocalData.fastpathHeap = heap;
  }
  return heap;
}

// we get here if the freelist is exhausted
void *ThreadLocalHeap::smallAllocSlowpath(size_t sizeClass) {
  Freelist &freelist = _freelist[sizeClass];

  const size_t sizeMax = SizeMap::ByteSizeForClass(sizeClass);

  _global->allocSmallMiniheaps(sizeClass, sizeMax, freelist.miniheaps());

  freelist.attach(_global->arenaBegin());

  d_assert(freelist.isAttached());
  d_assert(!freelist.isExhausted());
  d_assert(mh->isAttached());

  void *ptr = freelist.malloc();
  d_assert(ptr != nullptr);
  _last = &freelist;

  return ptr;
}
}  // namespace mesh
