// RUN: rm -rf %klee_pcache_dir
// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out %t1.bc

#include <assert.h>
#include <stdio.h>

int main() {  
  unsigned char x;

  klee_make_symbolic(&x, sizeof x);
  if (x >= 2) klee_silent_exit(0);

  char delete[2] = {0,1};

  char tmp = delete[ x ];
  char tmp2 = delete[0];
  delete[ x ] = tmp2;

  if (x==1) {
    assert(delete[1] == 0);
    return 0;
  }

  return 0;
}
