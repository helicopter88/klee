//
// Created by dmarino on 16/04/18.
//

#ifndef KLEE_PREDICATES_H
#define KLEE_PREDICATES_H

#include <klee/Expr.h>
#include <klee/util/Assignment.h>

namespace klee {
    struct NullOrNotSatisfyingAssignment {
        bool operator()(Assignment * a) const {
            return !a;
        }
    };
    struct SatisfyingAssignment {
        std::set <ref<Expr>> &key;

        explicit SatisfyingAssignment(std::set <ref<Expr>> &_key) : key(_key) {};

        bool operator()(Assignment *a) const {
            return a && a->satisfies(key.cbegin(), key.cend());
        }
    };
}

#endif //KLEE_PREDICATES_H
