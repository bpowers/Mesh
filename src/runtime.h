// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2017 University of Massachusetts, Amherst

#pragma once
#ifndef MESH__RUNTIME_H
#define MESH__RUNTIME_H

#include <pthread.h>
#include <signal.h>  // for stack_t

#include "internal.h"

#include "real.h"

#include "globalmeshingheap.h"
#include "lockedheap.h"
#include "mmapheap.h"
#include "semiansiheap.h"

#include "heaplayers.h"

namespace mesh {

// function passed to pthread_create
typedef void *(*PthreadFn)(void *);

class Runtime {
private:
  DISALLOW_COPY_AND_ASSIGN(Runtime);

  // ensure we don't mistakenly create additional runtime instances
  explicit Runtime();

public:
  void lock();
  void unlock();

  inline GlobalHeap &heap() {
    return _heap;
  }

  void startBgThread();

  // we need to wrap pthread_create so that we can safely implement a
  // stop-the-world quiescent period for the copy/mremap phase of
  // meshing
  int createThread(pthread_t *thread, const pthread_attr_t *attr, mesh::PthreadFn startRoutine, void *arg);

  void setMeshPeriodSecs(double period) {
    _heap.setMeshPeriodSecs(period);
  }

  int epollWait(int __epfd, struct epoll_event *__events, int __maxevents, int __timeout);
  int epollPwait(int __epfd, struct epoll_event *__events, int __maxevents, int __timeout, const __sigset_t *__ss);
  int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
  int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

  struct StartThreadArgs {
    explicit StartThreadArgs(Runtime *runtime_, PthreadFn startRoutine_, void *arg_)
        : runtime(runtime_), startRoutine(startRoutine_), arg(arg_) {
    }

    Runtime *runtime;
    PthreadFn startRoutine;
    void *arg;
  };

  static void *startThread(StartThreadArgs *threadArgs);

  // so we can call from the libmesh init function
  void createSignalFd();
  void installSegfaultHandler();

private:
  // initialize our pointer to libc's pthread_create, etc.  This
  // happens lazily, as the dynamic linker's dlopen calls into malloc
  // for memory allocation, so if we try to do this in MeshHeaps's
  // constructor we deadlock before main even runs.
  void initThreads();

  static void segfaultHandler(int sig, siginfo_t *siginfo, void *context);

  static void *bgThread(void *arg);

  friend Runtime &runtime();

  GlobalHeap _heap{};
  mutex _mutex{};
  int _signalFd{-2};
};

// get a reference to the Runtime singleton
inline Runtime &runtime() {
  // force alignment by using a buffer of doubles.
  static double buf[(sizeof(Runtime) + sizeof(double) - 1) / sizeof(double)];
  static Runtime *runtimePtr = new (buf) Runtime{};
  return *runtimePtr;
}
}  // namespace mesh

#endif  // MESH__RUNTIME_H
