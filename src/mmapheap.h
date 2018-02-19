// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
/*
  Heap Layers: An Extensible Memory Allocation Infrastructure

  Copyright (C) 2000-2014 by Emery Berger
  http://www.cs.umass.edu/~emery
  emery@cs.umass.edu

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#pragma once
#ifndef MESH__MESH_MMAP_H
#define MESH__MESH_MMAP_H

#if defined(_WIN32)
#error "TODO"
#include <windows.h>
#else
// UNIX
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <map>
#endif

#include "internal.h"

#ifndef HL_MMAP_PROTECTION_MASK
#error "define HL_MMAP_PROTECTION_MASK before including mmapheap.h"
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace mesh {

// OneWayMmapHeap allocates address space through calls to mmap and
// will never unmap address space.
class OneWayMmapHeap {
private:
  DISALLOW_COPY_AND_ASSIGN(OneWayMmapHeap);

public:
  enum { Alignment = MmapWrapper::Alignment };

  OneWayMmapHeap() {
  }

  inline void *map(size_t sz, int flags, int fd = -1) {
    if (sz == 0)
      return nullptr;

    // Round up to the size of a page.
    sz = (sz + CPUInfo::PageSize - 1) & (size_t) ~(CPUInfo::PageSize - 1);

    void *ptr = mmap(nullptr, sz, HL_MMAP_PROTECTION_MASK, flags, fd, 0);
    if (ptr == MAP_FAILED)
      abort();

    d_assert(reinterpret_cast<size_t>(ptr) % Alignment == 0);

    return ptr;
  }

  inline void *malloc(size_t sz) {
    return map(sz, MAP_PRIVATE | MAP_ANONYMOUS, -1);
  }

  inline size_t getSize(void *ptr) const {
    return 0;
  }

  inline void free(void *ptr) {
  }
};

// MmapHeap extends OneWayMmapHeap to track allocated address space
// and will free memory with calls to munmap.
class MmapHeap : public OneWayMmapHeap {
private:
  DISALLOW_COPY_AND_ASSIGN(MmapHeap);
  typedef OneWayMmapHeap SuperHeap;

public:
  enum { Alignment = MmapWrapper::Alignment };

  MmapHeap() : SuperHeap() {
  }

  inline void *malloc(size_t sz) {
    auto ptr = map(sz, MAP_PRIVATE | MAP_ANONYMOUS, -1);

    d_assert(_vmaMap.find(ptr) == _vmaMap.end());
    _vmaMap[ptr] = sz;
    d_assert(_vmaMap.find(ptr) != _vmaMap.end());
    d_assert(_vmaMap[ptr] == sz);

    return ptr;
  }

  inline size_t getSize(void *ptr) const {
    auto entry = _vmaMap.find(ptr);
    if (unlikely(entry == _vmaMap.end())) {
      debug("mmap: invalid getSize: %p", ptr);
      abort();
      return 0;
    }
    return entry->second;
  }

  inline bool inBounds(void *ptr) const {
    auto entry = _vmaMap.find(ptr);
    if (unlikely(entry == _vmaMap.end())) {
      return false;
    }
    // FIXME: this isn't right -- we want inclusion not exact match
    return true;
  }

  inline void free(void *ptr) {
    auto entry = _vmaMap.find(ptr);
    if (unlikely(entry == _vmaMap.end())) {
      debug("mmap: invalid free: %p", ptr);
      abort();
      return;
    }

    auto sz = entry->second;

    munmap(ptr, sz);
    // madvise(ptr, sz, MADV_DONTNEED);
    // mprotect(ptr, sz, PROT_NONE);

    _vmaMap.erase(entry);
    d_assert(_vmaMap.find(ptr) == _vmaMap.end());
  }

  // return the sum of the sizes of all large allocations
  size_t arenaSize() const {
    size_t sz = 0;
    for (auto it = _vmaMap.begin(); it != _vmaMap.end(); it++) {
      sz += it->second;
    }
    return sz;
  }

protected:
  internal::unordered_map<void *, size_t> _vmaMap{};
};
}  // namespace mesh

#endif  // MESH__MESH_MMAP_H
