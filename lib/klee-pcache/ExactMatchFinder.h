//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_EXACTMATCHFINDER_H
#define KLEE_PCACHE_EXACTMATCHFINDER_H


#include "klee/Solver.h"
#include "klee/Expr.h"
#include "RedisInstance.h"
#include "Finder.h"

namespace klee {
    class ExactMatchFinder : Finder {
    private:
        RedisInstance instance;
    public:
        ProtoAssignment *find(const std::set<ref<Expr>>& expressions) final;

        void insert(const std::set<ref<Expr>>& expressions, const Assignment *value) final;
    };
}


#endif //KLEE_PCACHE_EXACTMATCHFINDER_H
