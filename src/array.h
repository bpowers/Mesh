// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright 2017 University of Massachusetts, Amherst

#pragma once
#ifndef MESH__ARRAY_H
#define MESH__ARRAY_H

#include <iterator>

namespace mesh {

// enables iteration through the miniheaps in an array
template <typename Array>
class ArrayIter : public std::iterator<std::forward_iterator_tag, typename Array::value_type> {
public:
  ArrayIter(const Array &a, const size_t i) : _i(i), _array(a) {
  }
  ArrayIter &operator++() {
    if (unlikely(_i + 1 >= _array.size())) {
      _i = _array.size();
      return *this;
    }

    _i++;
    return *this;
  }
  bool operator==(const ArrayIter &rhs) const {
    return &_array == &rhs._array && _i == rhs._i;
  }
  bool operator!=(const ArrayIter &rhs) const {
    return &_array != &rhs._array || _i != rhs._i;
  }
  typename Array::value_type operator*() {
    return _array[_i];
  }

private:
  size_t _i;
  const Array &_array;
};

template <typename T, size_t Capacity>
class Array {
private:
  DISALLOW_COPY_AND_ASSIGN(Array);

public:
  typedef T *value_type;
  typedef ArrayIter<Array<T, Capacity>> iterator;
  typedef ArrayIter<Array<T, Capacity>> const const_iterator;

  Array() {
    d_assert(_size == 0);
#ifndef NDEBUG
    for (size_t i = 0; i < Capacity; i++) {
      d_assert(_objects[i] == nullptr);
    }
#endif
  }

  ~Array() {
    clear();
  }

  size_t size() const {
    return _size;
  }

  void clear() {
    memset(_objects, 0, Capacity * sizeof(T *));
    _size = 0;
  }

  void append(T *obj) {
    hard_assert(_size < Capacity);
    _objects[_size] = obj;
    _size++;
  }

  T *operator[](size_t i) const {
    d_assert(i < _size);
    return _objects[i];
  }

  iterator begin() {
    return iterator(*this, 0);
  }
  iterator end() {
    return iterator(*this, size());
  }
  const_iterator begin() const {
    return iterator(*this, 0);
  }
  const_iterator end() const {
    return iterator(*this, size());
  }
  const_iterator cbegin() const {
    return iterator(*this, 0);
  }
  const_iterator cend() const {
    return iterator(*this, size());
  }

private:
  size_t _size{0};
  T *_objects[Capacity]{};
};

}  // namespace mesh

#endif  // MESH__ARRAY_H
