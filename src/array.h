// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2017 University of Massachusetts, Amherst

#pragma once
#ifndef MESH__ARRAY_H
#define MESH__ARRAY_H

#include <iterator>

namespace mesh {

template <typename T, size_t Capacity>
class Array {
private:
  DISALLOW_COPY_AND_ASSIGN(Array);

public:
  Array() {
  }

  ~Array() {
    memset(_objects, 0, Capacity * sizeof(T));
  }

private:
  size_t _length{0};
  T *_objects[Capacity]{};
};

}  // namespace mesh

#endif  // MESH__ARRAY_H
