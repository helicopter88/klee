project(klee_pcache)

klee_add_component(klee_pcache RedisInstance.cpp PersistentMapOfSets.cpp RedisFinder.cpp PersistentMapOfSetsFinder.cpp NameNormalizer.cpp Trie.cpp TrieFinder.h TrieFinder.cpp NameNormalizerFinder.cpp AlgebraComparisonNormalizer.cpp Predicates.h)
set(LLVM_COMPONENTS
        support
        )
klee_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
target_link_libraries(klee_pcache PUBLIC ${LLVM_LIBS})
target_link_libraries(klee_pcache PUBLIC protobuf capnp kj kleaverExpr)
target_link_libraries(klee_pcache PUBLIC tacopie)
target_link_libraries(klee_pcache PUBLIC cpp_redis)
