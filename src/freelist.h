// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2017 University of Massachusetts, Amherst

#pragma once
#ifndef MESH__FREELIST_H
#define MESH__FREELIST_H

#include <iterator>
#include <random>
#include <utility>

// #include "rng/xoroshiro128plus.h"
#include "rng/mwc.h"

#include "internal.h"

#include "miniheap.h"

using mesh::debug;

namespace mesh {

class Freelist {
private:
  DISALLOW_COPY_AND_ASSIGN(Freelist);

public:
  Freelist() : _prng(internal::seed(), internal::seed()) {
    // set initialized = false;
  }

  ~Freelist() {
    detach();
  }

  // post: list has the index of all bits set to 1 in it, in a random order
  size_t init(MWC &fastPrng, internal::Bitmap &bitmap) {
    d_assert(_maxCount > 0);
    d_assert_msg(_maxCount <= kMaxFreelistLength, "objCount? %zu <= %zu", _maxCount, kMaxFreelistLength);

    // off == maxCount means 'empty'
    _off = _maxCount;

    // TODO: iterate over the bitmap?
    for (size_t i = 0; i < _maxCount; i++) {
      // if we were passed in a bitmap and the current object is
      // already allocated, don't add its offset to the freelist
      if (bitmap.isSet(i)) {
        continue;
      } else {
        bitmap.tryToSet(i);
      }

      _off--;
      d_assert(_off < _maxCount);
      _list[_off] = i;
    }

    if (kEnableShuffleFreelist) {
      internal::mwcShuffle(&_list[_off], &_list[_maxCount], fastPrng);
    }

    return length();
  }

  void detach() {
    d_assert(_attachedMiniheap != nullptr);
    // auto list = _list;
    // _list = nullptr;
    _attachedMiniheap->unref();
    _attachedMiniheap = nullptr;
    _start = 0;
    _end = 0;
    // atomic_thread_fence(memory_order_seq_cst);
    // mesh::internal::Heap().free(list);
  }

  inline bool isExhausted() const {
    return _off >= _maxCount;
  }

  inline size_t maxCount() const {
    return _maxCount;
  }

  // number of items in the list
  inline size_t ATTRIBUTE_ALWAYS_INLINE length() const {
    return _maxCount - _off;
  }

  // Pushing an element onto the freelist does a round of the
  // Fisher-Yates shuffle.
  inline void ATTRIBUTE_ALWAYS_INLINE push(size_t freedOff) {
    d_assert(_off > 0);  // we must have at least 1 free space in the list

    _off--;
    _list[_off] = freedOff;

    if (kEnableShuffleFreelist) {
      size_t swapOff = _prng.inRange(_off, maxCount() - 1);
      _lastOff = swapOff;
      std::swap(_list[_off], _list[swapOff]);
    }
  }

  inline size_t ATTRIBUTE_ALWAYS_INLINE pop() {
    d_assert(_off < _maxCount);

    auto val = _list[_off];
    _off++;

    return val;
  }

  inline void ATTRIBUTE_ALWAYS_INLINE free(void *ptr) {
    const auto ptrval = reinterpret_cast<uintptr_t>(ptr);
    // const size_t off = (ptrval - _start) / _objectSize;
    const size_t off = (ptrval - _start) * _objectSizeReciprocal;
    // hard_assert_msg(off == off2, "%zu != %zu", off, off2);

    push(off);
  }

  inline MiniHeap *getAttached() const {
    return _attachedMiniheap;
  }

  inline bool isAttached() const {
    return _attachedMiniheap != nullptr;
  }

  // an attach takes ownership of the reference to mh
  inline void attach(MWC &prng, MiniHeap *mh) {
    d_assert(_attachedMiniheap == nullptr);
    d_assert(mh->refcount() > 0);
    _attachedMiniheap = mh;

    _start = mh->getSpanStart();
    _end = _start + mh->spanSize();

    const auto allocCount = init(prng, mh->writableBitmap());
#ifndef NDEBUG
    if (allocCount == 0) {
      mh->dumpDebug();
    }
#endif
    d_assert_msg(allocCount > 0, "no free bits in MH %p", mh->getSpanStart());

    // tell the miniheap how many offsets we pulled out/preallocated into our freelist
    mh->incrementInUseCount(allocCount);
  }

  inline bool ATTRIBUTE_ALWAYS_INLINE contains(void *ptr) const {
    const auto ptrval = reinterpret_cast<uintptr_t>(ptr);
    return ptrval >= _start && ptrval < _end;
  }

  inline void *ATTRIBUTE_ALWAYS_INLINE ptrForOffset(size_t off) const {
    return reinterpret_cast<void *>(_start + off * _objectSize);
  }

  inline void *ATTRIBUTE_ALWAYS_INLINE malloc() {
    d_assert(!isExhausted());
    const auto off = pop();
    return ptrForOffset(off);
  }

  inline size_t getSize() {
    return _objectSize;
  }

  // called once, on initialization of ThreadLocalHeap
  inline void setObjectSize(size_t sz) {
    _objectSize = sz;
    _objectSizeReciprocal = 1.0 / (float) sz;
    _maxCount = max(HL::CPUInfo::PageSize / sz, kMinStringLen);
    // initially, we are unattached and therefor have no capacity.
    // Setting _off to _maxCount causes isExhausted() to return true
    // so that we don't separately have to check !isAttached() in the
    // malloc fastpath.
    _off = _maxCount;
  }

private:
  size_t _objectSize{0};
  float _objectSizeReciprocal{0.0};
  uintptr_t _start{0};
  uintptr_t _end{0};
  MiniHeap *_attachedMiniheap{nullptr};
  MWC _prng;
  uint16_t _maxCount{0};
  uint16_t _off{0};
  volatile uint8_t _lastOff{0};
  uint8_t __padding[7];
  uint8_t _list[kMaxFreelistLength] CACHELINE_ALIGNED;
};

static_assert(HL::gcd<sizeof(Freelist), CACHELINE_SIZE>::value == CACHELINE_SIZE,
              "Freelist not multiple of cacheline size!");
static_assert(sizeof(Freelist) == 320, "Freelist not expected size!");
}  // namespace mesh

#endif  // MESH__FREELIST_H
