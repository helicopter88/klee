// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --optimize=false --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out %t1.bc
// RUN: grep "total queries = 2" %t.klee-out/info
// RUN: %klee --optimize=false --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out-1 %t1.bc
// RUN: grep "total queries = 0" %t.klee-out-1/info

#include <assert.h>

int main() {
    unsigned char x, y;

    klee_make_symbolic(&x, sizeof x, "x");
    klee_make_symbolic(&y, sizeof y, "y");

    if (x==10) {
        assert(y==10);
    }

    klee_silent_exit(0);
    return 0;
}
