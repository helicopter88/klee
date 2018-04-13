add_klee_unit_test(PCacheTest
        TrieTest.cpp
        NameNormalizerTest.cpp)
target_link_libraries(PCacheTest PRIVATE klee_pcache kleaverExpr)
