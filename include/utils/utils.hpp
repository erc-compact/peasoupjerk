#pragma once

#include <cmath>
#include "cuda.h"
#include <string>
#include <utils/exceptions.hpp>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iostream>

class Utils {
public:
  static unsigned int prev_power_of_two(unsigned int val) {
    unsigned int n = 1;
    while (n * 2 <= val) n *= 2;
    return n;
  }

  template <class T>
  static void device_malloc(T** ptr, unsigned int units) {
    cudaMalloc((void**)ptr, sizeof(T) * units);
    ErrorChecker::check_cuda_error("Error from device_malloc");
  }

  template <class T>
  static void host_malloc(T** ptr, unsigned int units) {
    cudaMallocHost((void**)ptr, sizeof(T) * units);
    ErrorChecker::check_cuda_error("Error from host_malloc");
  }

  template <class T>
  static void device_free(T* ptr) {
    cudaFree(ptr);
    ErrorChecker::check_cuda_error("Error from device_free");
  }

  template <class T>
  static void host_free(T* ptr) {
    cudaFreeHost((void*)ptr);
    ErrorChecker::check_cuda_error("Error from host_free");
  }

  template <class T>
  static void h2dcpy(T* d_ptr, T* h_ptr, unsigned int units) {
    cudaMemcpy((void*)d_ptr, (void*)h_ptr, sizeof(T)*units, cudaMemcpyHostToDevice);
    ErrorChecker::check_cuda_error("Error from h2dcpy");
  }

  template <class T>
  static void d2hcpy(T* h_ptr, T* d_ptr, unsigned int units) {
    cudaMemcpy((void*)h_ptr, (void*)d_ptr, sizeof(T)*units, cudaMemcpyDeviceToHost);
    ErrorChecker::check_cuda_error("Error from d2hcpy");
  }

  template <class T>
  static void d2dcpy(T* dst, T* src, unsigned int units) {
    cudaMemcpy(dst, src, sizeof(T)*units, cudaMemcpyDeviceToDevice);
    ErrorChecker::check_cuda_error("Error from d2dcpy");
  }

  template <class T>
  static void dump_device_buffer(T* buffer, size_t size, const std::string& filename) {
    T* host_ptr;
    host_malloc<T>(&host_ptr, size);
    d2hcpy(host_ptr, buffer, size);
    std::ofstream out(filename, std::ofstream::binary);
    out.write(reinterpret_cast<char*>(host_ptr), size * sizeof(T));
    out.close();
    host_free(host_ptr);
  }

  template <class T>
  static void dump_host_buffer(T* buffer, size_t size, const std::string& filename) {
    std::ofstream out(filename, std::ofstream::binary);
    out.write(reinterpret_cast<char*>(buffer), size * sizeof(T));
    out.close();
  }

  static int gpu_count() {
    int count;
    cudaGetDeviceCount(&count);
    return count;
  }
};

class Block {
public:
  unsigned int blocks;
  size_t data_idx;
  size_t gulp_size;
  Block(unsigned int b, size_t idx, size_t gs)
    : blocks(b), data_idx(idx), gulp_size(gs) {}
};

class BlockCalculator {
  std::vector<Block> output;
public:
  BlockCalculator(size_t total_size,
                  unsigned int max_blocks,
                  unsigned int max_threads) {
    size_t units = max_blocks * max_threads;
    size_t ngulps = total_size / units + 1;
    for (size_t g = 0; g < ngulps; ++g) {
      size_t offset = g * units;
      size_t rem = (g < ngulps - 1) ? units
                                    : (total_size > offset ? total_size - offset : 0);
      unsigned int b = (rem + max_threads - 1) / max_threads;
      if (b > max_blocks) b = max_blocks;
      output.emplace_back(b, offset, rem);
    }
  }
  unsigned int size() const { return (unsigned int)output.size(); }
  const Block& operator[](size_t i) const { return output[i]; }
};
