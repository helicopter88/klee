//
// Created by dmarino on 05/05/18.
//

#include <klee/util/ExprVisitor.h>
#include "AlgebraComparisonNormalizer.h"

using namespace klee;

class ArithmeticVisitor : public ExprVisitor {
protected:

    // x = k -> x <= k && -x <= -k
    // TODO: this should become a std::set<ref<Expr>> with the two expressions to improve reusability.
    Action visitEq(const EqExpr &expr) override {
        if(expr.getWidth() == Expr::Bool) {
            return Action::doChildren();
        }
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        const ref<ConstantExpr> &minusOne = ConstantExpr::create(1, child1->getWidth())->Neg();
        return Action::changeTo(
                AndExpr::create(SleExpr::create(child1, child2),
                                SleExpr::create(
                                        MulExpr::create(minusOne, child1),
                                        MulExpr::create(minusOne, child2)))
        );

    }

    // x < k -> x <= k - 1
    Action visitUlt(const UltExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        // Don't do anything if it's 0 as it would become MAX_INT
        if (isa<ConstantExpr>(child2)) {
            if (cast<ConstantExpr>(child2)->isZero()) {
                return Action::skipChildren();
            }
        }
        return Action::changeTo(
                UleExpr::create(child1, SubExpr::create(child2, ConstantExpr::create(1, child2->getWidth()))));
    }


    // x < k -> x <= k - 1
    Action visitSlt(const SltExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        return Action::changeTo(
                SleExpr::create(child1, SubExpr::create(child2, ConstantExpr::create(1, child2->getWidth()))));

    }

    // x > k -> -x <= -k -1
    Action visitSgt(const SgtExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        const ref<ConstantExpr> &one = ConstantExpr::create(1, child2->getWidth());
        return Action::changeTo(
                SleExpr::create(MulExpr::create(child1, one->Neg()),
                                SubExpr::create(MulExpr::create(child2, one->Neg()), one)));
    }

    // x >= k -> -x <= -k
    Action visitSge(const SgeExpr &expr) override {
        ref<Expr> child1 = expr.getKid(0);
        ref<Expr> child2 = expr.getKid(1);
        const ref<ConstantExpr> &one = ConstantExpr::create(1, child2->getWidth());
        return Action::changeTo(
                SleExpr::create(MulExpr::create(child1, one->Neg()),
                                MulExpr::create(child2, one->Neg())));
    }
};

ref<Expr> AlgebraComparisonNormalizer::normalizeExpr(const ref<Expr> &expr) {
    if(expr.isNull()) {
        return expr;
    }
    ArithmeticVisitor av;
    return av.visit(expr);
}

std::set<ref<Expr>> AlgebraComparisonNormalizer::normalizeEqExpr(const ref<Expr> &expr) {
    if(expr->getWidth() == Expr::Bool)
        return {expr};
    EqExpr *eq = cast<EqExpr>(expr);
    ref<Expr> child1 = eq->getKid(0);
    ref<Expr> child2 = eq->getKid(1);
    const ref<ConstantExpr> &minusOne = ConstantExpr::create(1, child1->getWidth())->Neg();
    return {
            SleExpr::create(child1, child2),
            SleExpr::create(
                    MulExpr::create(minusOne, child1),
                    MulExpr::create(minusOne, child2))
    };

}

std::set<ref<Expr>> AlgebraComparisonNormalizer::normalizeExpressions(const std::set<ref<Expr>> &exprs) {
    std::set<ref<Expr>> out;
    for (const ref<Expr> &expr: exprs) {
        //if (expr->getKind() == Expr::Eq) {
        //    const std::set<ref<Expr>> &cstraints = normalizeEqExpr(expr);
        //    out.insert(cstraints.cbegin(), cstraints.cend());
        //} else {
            out.insert(normalizeExpr(expr));
        //}
    }
    return out;
}