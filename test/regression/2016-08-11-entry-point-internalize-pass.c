// RUN: %llvmgcc %s -emit-llvm -g -O0 -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --pcache-path=%t.klee-out/cache --output-dir=%t.klee-out --entry-point=entry %t.bc

int entry() {
  return 0;
}
