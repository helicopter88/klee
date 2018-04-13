//
// Created by dmarino on 13/04/18.
//

#include <gtest/gtest.h>
#include <klee/util/ArrayCache.h>
#include "../../lib/klee-pcache/Trie.h"

using namespace klee;
namespace {
    TEST(TrieTest, SimpleGet) {
        Trie trie;
        std::set<ref<Expr>> set1, set2, set3;

        auto *assignment = new Assignment();
        ArrayCache ac;

        const Array *array = ac.CreateArray("arr2", 256);
        ref<Expr> read64 = Expr::createTempRead(array, 64);
        set1.insert(read64);

        const Array *array2 = ac.CreateArray("arr3", 256);
        ref<Expr> read8_2 = Expr::createTempRead(array2, 8);
        set1.insert(read8_2);

        trie.insert(set1, assignment);
        EXPECT_TRUE(trie.search(set1));

        ref<Expr> extract1 = ExtractExpr::create(read64, 36, 4);
        set2.insert(extract1);

        ref<Expr> extract2 = ExtractExpr::create(read64, 32, 4);
        set2.insert(extract2);

        EXPECT_FALSE(trie.search(set2));
        EXPECT_FALSE(trie.search(set3));

        delete assignment;
    }

    TEST(TrieTest, SubsetTest) {
        Trie trie;
        std::set<ref<Expr>> set1, set2, emptySet;

        ArrayCache ac;
        auto *assignment = new Assignment();
        const Array *array = ac.CreateArray("arr2", 256);
        ref<Expr> read64 = Expr::createTempRead(array, 64);

        ref<Expr> extract1 = ExtractExpr::create(read64, 36, 4);
        set1.insert(extract1);
        set2.insert(extract1);

        ref<Expr> extract2 = ExtractExpr::create(read64, 32, 4);
        set1.insert(extract2);

        ref<Expr> extract3 = ExtractExpr::create(read64, 12, 3);
        set1.insert(extract3);

        ref<Expr> extract4 = ExtractExpr::create(read64, 10, 2);
        set1.insert(extract4);
        set2.insert(extract4);

        ref<Expr> extract5 = ExtractExpr::create(read64, 2, 8);
        set1.insert(extract5);

        trie.insert(set2, assignment);

        EXPECT_FALSE(trie.search(set1));
        EXPECT_TRUE(trie.search(set2));
        EXPECT_TRUE(trie.existsSubset(set1));
        EXPECT_FALSE(trie.existsSubset(emptySet));

        delete assignment;
    }

    TEST(TrieTest, SupersetTest) {
        Trie trie;
        std::set<ref<Expr>> set1, set2, set3, emptySet;

        ArrayCache ac;
        auto *assignment = new Assignment();


        const Array *array = ac.CreateArray("arr2", 256);
        ref<Expr> read64 = Expr::createTempRead(array, 64);
        set3.insert(read64);

        ref<Expr> extract1 = ExtractExpr::create(read64, 36, 4);
        set1.insert(extract1);
        set2.insert(extract1);

        ref<Expr> extract2 = ExtractExpr::create(read64, 32, 4);
        set1.insert(extract2);

        ref<Expr> extract3 = ExtractExpr::create(read64, 12, 3);
        set1.insert(extract3);

        ref<Expr> extract4 = ExtractExpr::create(read64, 10, 2);
        set1.insert(extract4);
        set2.insert(extract4);

        ref<Expr> extract5 = ExtractExpr::create(read64, 2, 8);
        set1.insert(extract5);

        trie.insert(set1, assignment);

        EXPECT_TRUE(trie.search(set1));
        EXPECT_TRUE(trie.existsSuperset(set1));
        EXPECT_TRUE(trie.existsSuperset(set2));
        EXPECT_FALSE(trie.existsSuperset(set3));
        EXPECT_FALSE(trie.existsSuperset(emptySet));
        delete assignment;
    }
}
