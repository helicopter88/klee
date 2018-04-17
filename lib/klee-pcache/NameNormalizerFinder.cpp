//
// Created by dmarino on 13/04/18.
//

#include "NameNormalizerFinder.h"
#include "NameNormalizer.h"

namespace klee {

    Assignment **NameNormalizerFinder::find(std::set<ref<Expr>> &exprs) {
        NameNormalizer nn;
        std::set<ref<Expr>> nExprs = nn.normalizeExpressions(exprs);
        Assignment **ret = f.find(nExprs);
        if (ret) {
            Assignment* r = nn.denormalizeAssignment(*ret);
            *ret = r;
        }
        return ret;
    }

    void NameNormalizerFinder::insert(std::set<ref<Expr>> &exprs, Assignment *assignment) {
        NameNormalizer nn;
        std::set<ref<Expr>> nExprs = nn.normalizeExpressions(exprs);
        Assignment *nAssignment = nn.normalizeAssignment(assignment);
        f.insert(nExprs, nAssignment);
    }

    Assignment **NameNormalizerFinder::findSpecial(std::set<ref<Expr>> &exprs) {
        NameNormalizer nn;
        std::set<ref<Expr>> nExprs = nn.normalizeExpressions(exprs);
        Assignment **ret = f.findSpecial(nExprs);
        if (ret) {
            Assignment* r = nn.denormalizeAssignment(*ret);
            *ret = r;
        }
        return ret;
    }

}