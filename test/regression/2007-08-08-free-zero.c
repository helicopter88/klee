// RUN: rm -rf %klee_pcache_dir
// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out %t1.bc
// RUN: ls %t.klee-out | not grep *.err

#include <stdlib.h>

int main() {
  free(0);
  return 0;
}
