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
    class ExactMatchFinder : Finder<Assignment*> {
    private:
        RedisInstance instance;
    public:
        Assignment** find(std::set<ref<Expr>>& expressions) final;

        void insert(std::set<ref<Expr>>& expressions, Assignment *value) final;

        void close() final;
    };
}


#endif //KLEE_PCACHE_EXACTMATCHFINDER_H
