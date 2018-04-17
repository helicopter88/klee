//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_FINDER_H
#define KLEE_PCACHE_FINDER_H

#include <klee/util/Cache.pb.h>
#include <klee/util/Assignment.h>
#include "klee/Expr.h"
namespace klee {
    template <typename Result> class Finder {
    public:
        virtual Result* find(std::set<ref<Expr>>& exprs) = 0;
        virtual Result* findSpecial(std::set<ref<Expr>>& exprs) {
            return find(exprs);
        }
        virtual void insert(std::set<ref<Expr>>& exprs, Assignment* assignment) = 0;
        virtual void storeFinder() = 0;
    };
}

#endif //KLEE_PCACHE_FINDER_H
