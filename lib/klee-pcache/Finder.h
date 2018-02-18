//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_FINDER_H
#define KLEE_PCACHE_FINDER_H

#include <klee/util/Cache.pb.h>
#include <klee/util/Assignment.h>
#include "klee/Expr.h"
namespace klee {
    class Finder {
    public:
        virtual ProtoAssignment* find(const std::set<ref<Expr>>& exprs) = 0;
        virtual void insert(const std::set<ref<Expr>>& exprs, const Assignment* assignment) = 0;
    };
}

#endif //KLEE_PCACHE_FINDER_H
