//
// Created by dmarino on 16/04/18.
//

#ifndef KLEE_PREDICATES_H
#define KLEE_PREDICATES_H

#include <klee/Expr.h>
#include <klee/util/Assignment.h>

using namespace llvm;
using namespace klee;
namespace klee {
    struct NullOrNotSatisfyingAssignment {
        std::set<ref<Expr>> &key;

        explicit NullOrNotSatisfyingAssignment(std::set<ref<Expr>> &_key) : key(_key) {};

        bool operator()(Assignment *a) const {
            return !a || a->satisfies(key.cbegin(), key.cend());
        }
    };

    struct SatisfyingAssignment {

        bool operator()(Assignment *a) const {
            return a;
        }
    };
}

#endif //KLEE_PREDICATES_H
