//
// Created by dmarino on 05/05/18.
//

#ifndef KLEE_ARITHMETICNORMALIZER_H
#define KLEE_ARITHMETICNORMALIZER_H

#include <klee/Expr.h>


/**
 * Basic arithmetic normalizer, tries to reduce every comparison to <=.
 * Partially ignores unsigned >= and > as the transformations don't make sense without negative numbers
 */
namespace klee {
    class AlgebraComparisonNormalizer {
    public:
        static ref<Expr> normalizeExpr(const ref<Expr>& expr);
        static std::set<ref<Expr>> normalizeExpressions(const std::set<ref<Expr>> &exprs);
        static std::set<ref<Expr>> normalizeEqExpr(const ref<Expr> &expr);
    };
}


#endif //KLEE_ARITHMETICNORMALIZER_H
