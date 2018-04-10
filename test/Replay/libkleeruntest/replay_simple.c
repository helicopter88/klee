// RUN: %llvmgcc %s -emit-llvm -g -O0 -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out --search=dfs %t.bc
// RUN: test -f %t.klee-out/test000001.ktest
// RUN: test -f %t.klee-out/test000002.ktest

// Now try to replay with libkleeRuntest
// RUN: %cc %s %libkleeruntest -Wl,-rpath %libkleeruntestdir -o %t_runner
// RUN: env KTEST_FILE=%t.klee-out/test000001.ktest %t_runner | FileCheck -check-prefix=TESTONE %s
// RUN: env KTEST_FILE=%t.klee-out/test000002.ktest %t_runner | FileCheck -check-prefix=TESTTWO %s

#include "klee/klee.h"
#include <stdio.h>

int main(int argc, char** argv) {
  int x = 0;
  klee_make_symbolic(&x, sizeof(x), "x");

  if (x == 0) {
    printf("x is 0\n");
  } else {
    printf("x is not 0\n");
  }
  return 0;
}

// TESTONE: x is not 0
// TESTTWO: x is 0
