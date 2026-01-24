// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2019 The Mesh Authors. All rights reserved.
// Use of this source code is governed by the Apache License,
// Version 2.0, that can be found in the LICENSE file.

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>

#ifdef __linux__
#include <sys/signalfd.h>
#endif

#include "runtime.h"
#include "runtime_impl.h"
#include "thread_local_heap.h"  // Needed for explicit instantiation if it uses TLH

namespace mesh {

ATTRIBUTE_ALIGNED(CACHELINE_SIZE)
const unsigned char SizeMap::class_array_[kClassArraySize] = {
#include "size_classes.def"
};

ATTRIBUTE_ALIGNED(CACHELINE_SIZE)
const int32_t SizeMap::class_to_size_[kClassSizesMax] = {
    16,  16,  32,  48,  64,  80,  96,  112,  128,  160,  192,  224,   256,
    320, 384, 448, 512, 640, 768, 896, 1024, 2048, 4096, 8192, 16384,
};

STLAllocator<char, internal::Heap> internal::allocator{};

size_t internal::measurePssKiB() {
  auto fd = open("/proc/self/smaps_rollup", O_RDONLY | O_CLOEXEC);
  if (unlikely(fd < 0)) {
    mesh::debug("measurePssKiB: no smaps_rollup");
    return 0;
  }

  static constexpr size_t BUF_LEN = 1024;
  char buf[BUF_LEN];
  memset(buf, 0, BUF_LEN);

  auto _ __attribute__((unused)) = read(fd, buf, BUF_LEN - 1);
  close(fd);

  auto start = strstr(buf, "\nPss: ");
  if (unlikely(start == nullptr)) {
    mesh::debug("measurePssKiB: no Pss");
    return 0;
  }

  return atoi(&start[6]);
}

int internal::copyFile(int dstFd, int srcFd, off_t off, size_t sz) {
  d_assert(off >= 0);

#if defined(__APPLE__)
  // macOS doesn't have sendfile for regular files or copy_file_range,
  // so we use pread/pwrite to copy the specific byte range
  constexpr size_t kBufSize = 16384;
  char buf[kBufSize];
  size_t totalCopied = 0;
  off_t currentOff = off;

  while (totalCopied < sz) {
    size_t toRead = sz - totalCopied;
    if (toRead > kBufSize) {
      toRead = kBufSize;
    }
    ssize_t bytesRead = pread(srcFd, buf, toRead, currentOff);
    if (bytesRead <= 0) {
      return totalCopied > 0 ? static_cast<int>(totalCopied) : -1;
    }
    ssize_t bytesWritten = pwrite(dstFd, buf, bytesRead, currentOff);
    if (bytesWritten != bytesRead) {
      return totalCopied > 0 ? static_cast<int>(totalCopied) : -1;
    }
    totalCopied += bytesWritten;
    currentOff += bytesWritten;
  }
  return static_cast<int>(totalCopied);
#elif defined(__FreeBSD__)
  // unlike Linux and Solaris, sendfile works only with sockets here
  // thus copy_file_range is the only viable solution
  int result = copy_file_range(srcFd, &off, dstFd, NULL, sz, 0);
  return result;
#else
  errno = 0;
  // sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+
  int result = sendfile(dstFd, srcFd, &off, sz);
  return result;
#endif
}

// Explicit instantiation of Runtime
template class Runtime<4096>;
template class Runtime<16384>;

}  // namespace mesh
