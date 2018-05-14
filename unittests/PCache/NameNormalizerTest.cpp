#include <klee/Expr.h>
#include <gtest/gtest.h>
#include <klee/util/ArrayCache.h>
#include "../../lib/klee-pcache/NameNormalizer.h"

using namespace klee;
namespace {
    TEST(NameNormalizerTest, ExprNormalization) {
        NameNormalizer nn;
        std::set<ref<Expr>> exprs;

        ArrayCache ac;

        const Array *array = ac.CreateArray("arr1", 256);
        ref<Expr> read64 = Expr::createTempRead(array, 64);

        const Array *array2 = ac.CreateArray("arr2", 256);
        ref<Expr> read8_2 = Expr::createTempRead(array2, 64);

        ref<Expr> addExpr = AddExpr::create(read64, read8_2);
        exprs.insert(addExpr);
        const std::set<ref<Expr>> &nExprs = nn.normalizeExpressions(exprs);
        EXPECT_NE(exprs, nExprs);

        const std::map<std::string, std::string> &mappings = nn.getMappings();
        EXPECT_EQ(mappings.size(), 2);

        EXPECT_TRUE(mappings.find("arr1") != mappings.cend());

        EXPECT_TRUE(mappings.find("arr2") != mappings.cend());

        EXPECT_TRUE(mappings.find("shouldnotbehere") == mappings.cend());

    }

    TEST(NameNormalizerTest, AssignmentNormalization) {
        NameNormalizer nn;

        std::vector<const Array *> arrays;
        std::vector<std::vector<unsigned char>> bvs;
        std::set<ref<Expr>> exprs;

        ArrayCache ac;

        const Array *array = ac.CreateArray("arr1", 256);
        ref<Expr> read64 = Expr::createTempRead(array, 64);

        const Array *array2 = ac.CreateArray("arr2", 256);
        ref<Expr> read64_2 = Expr::createTempRead(array2, 64);

        const Array *array3 = ac.CreateArray("arr3", 256);
        ref<Expr> read64_3 = Expr::createTempRead(array3, 64);

        ref<Expr> selExpr = AddExpr::create(read64, AddExpr::create(read64_2, read64_3));
        exprs.insert(selExpr);

        const std::set<ref<Expr>> &nExprs = nn.normalizeExpressions(exprs);

        arrays.push_back(array);
        arrays.push_back(array2);

        for (int i = 0; i < 3; i++) {
            bvs.emplace_back(0, 64);
        }
        Assignment assignment(arrays, bvs);
        Assignment *nAssignment = nn.normalizeAssignment(&assignment);
        EXPECT_NE(assignment.bindings, nAssignment->bindings);

        Assignment *dAssignment = nn.denormalizeAssignment(nAssignment);
        auto dIter = dAssignment->bindings.cbegin();
        for (auto iter = assignment.bindings.cbegin(); iter != assignment.bindings.cend(); iter++) {
            EXPECT_EQ(*(iter->first), *(dIter->first));
            EXPECT_EQ(iter->second, iter->second);
            dIter++;
        }
    }
}
