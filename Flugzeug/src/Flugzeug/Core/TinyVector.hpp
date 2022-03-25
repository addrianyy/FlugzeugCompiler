#pragma once
#include "Error.hpp"

#include <span>

namespace flugzeug {

template <typename T, size_t Capacity> class TinyVector {
  T data[Capacity];
  size_t size_ = 0;

public:
  TinyVector() = default;

  void push_back(T value) {
    verify(size_ < Capacity, "TinyVector is full");
    data[size_++] = value;
  }

  T& operator[](size_t index) { return data[index]; }
  const T& operator[](size_t index) const { return data[index]; }

  T* begin() { return data; }
  T* end() { return data + size_; }

  const T* begin() const { return data; }
  const T* end() const { return data + size_; }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  operator std::span<T>() { return std::span<T>(data, size_); }
  operator std::span<T>() const { return std::span<T>(data, size_); }

  template <size_t OtherCapacity> bool operator==(const TinyVector<T, OtherCapacity>& other) const {
    if (other.size() != size()) {
      return false;
    }

    for (size_t i = 0; i < size(); ++i) {
      if (data[i] != other[i]) {
        return false;
      }
    }

    return true;
  }
};

} // namespace flugzeug