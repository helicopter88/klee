// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t1.out %t1.replay
// RUN: %klee --pcache-path=%t.klee-out/cache --output-dir=%t1.out %t1.bc
// RUN: %klee --pcache-path=%t.klee-out/cache --output-dir=%t1.replay --replay-ktest-dir=%t1.out %t1.bc

int main() {
  int i;
  klee_make_symbolic(&i, sizeof i, "i");
  klee_print_range("i", i);
  return 0;
}
