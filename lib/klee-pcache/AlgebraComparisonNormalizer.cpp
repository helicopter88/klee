//
// Created by dmarino on 05/05/18.
//

#include <klee/util/ExprVisitor.h>
#include "AlgebraComparisonNormalizer.h"

using namespace klee;

class ArithmeticVisitor : public ExprVisitor {
protected:

    // x = k -> x < k + 1 && k < x + 1
    // TODO: this should become a std::set<ref<Expr>> with the two expressions to improve reusability.
    Action visitEq(const EqExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        const ref<ConstantExpr> &one = ConstantExpr::create(1, child1->getWidth());
        if (isa<ConstantExpr>(child1)) {
            ConstantExpr *constantExpr = cast<ConstantExpr>(child1);
            uint64_t value = constantExpr->getAPValue().getZExtValue();
            switch (constantExpr->getWidth()) {
#define SIZE_CHECK(S) \
        case Expr::Int ## S: \
            if(value >= INT##S##_MAX) { \
                    llvm::errs() << "Ignored: "<< value << "\n";\
                    return Action::skipChildren(); \
            } \
            break;

                SIZE_CHECK(8);
                SIZE_CHECK(16);
                SIZE_CHECK(32);
                SIZE_CHECK(64);
                default:
                    return Action::skipChildren();
            }
        }

        if (isa<ConstantExpr>(child2)) {
            ConstantExpr *constantExpr = cast<ConstantExpr>(child2);
            uint64_t value = constantExpr->getAPValue().getZExtValue();
            switch (constantExpr->getWidth()) {
                SIZE_CHECK(8);
                SIZE_CHECK(16);
                SIZE_CHECK(32);
                SIZE_CHECK(64);
                default:
                    return Action::skipChildren();
            }
        }

        return Action::changeTo(
                AndExpr::create(SltExpr::create(child1, AddExpr::create(child2, one)),
                                SltExpr::create(child2, AddExpr::create(child1, one)))
        );

    }

    Action visitSle(const SleExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        return Action::changeTo(
                SltExpr::create(child1, AddExpr::create(child2, ConstantExpr::create(1, child2->getWidth()))));

    }

    // x > k -> -x < -k
    Action visitSgt(const SgtExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        const ref<ConstantExpr> &one = ConstantExpr::create(1, child2->getWidth());
        return Action::changeTo(
                SltExpr::create(MulExpr::create(child1, one->Neg()),
                                (MulExpr::create(child2, one->Neg()))));
    }

    // x >= k -> -x < -k + 1
    Action visitSge(const SgeExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        const ref<ConstantExpr> &one = ConstantExpr::create(1, child2->getWidth());
        return Action::changeTo(
                SltExpr::create(MulExpr::create(child1, one->Neg()),
                                AddExpr::create(MulExpr::create(child2, one->Neg()), one)));
    }
};

ref<Expr> AlgebraComparisonNormalizer::normalizeExpr(const ref<Expr> &expr) {
    if (expr.isNull()) {
        return expr;
    }
    ArithmeticVisitor av;
    return av.visit(expr);
}


std::set<ref<Expr>> AlgebraComparisonNormalizer::normalizeExpressions(const std::set<ref<Expr>> &exprs) {
    std::set<ref<Expr>> out;
    for (const ref<Expr> &expr: exprs) {
        out.insert(normalizeExpr(expr));
    }
    return out;
}