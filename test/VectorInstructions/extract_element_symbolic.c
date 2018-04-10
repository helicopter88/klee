// REQUIRES: geq-llvm-3.4
// RUN: %llvmgcc %s -emit-llvm -O0 -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: not %klee --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out --exit-on-error %t1.bc > %t.log 2>&1
// RUN: FileCheck -input-file=%t.log %s
#include "klee/klee.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

typedef uint32_t v4ui __attribute__ ((vector_size (16)));
int main() {
  v4ui f = { 0, 1, 2, 3 };
  unsigned index = 0;
  klee_make_symbolic(&index, sizeof(unsigned), "index");
  // Performing read should be ExtractElement instruction.
  // For now this is an expected limitation.
  // CHECK: extract_element_symbolic.c:[[@LINE+1]]: ExtractElement, support for symbolic index not implemented
  uint32_t readValue = f[index];
  return readValue % 255;
}
