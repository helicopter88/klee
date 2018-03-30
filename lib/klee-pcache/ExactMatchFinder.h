//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_EXACTMATCHFINDER_H
#define KLEE_PCACHE_EXACTMATCHFINDER_H


#include "klee/Solver.h"
#include "klee/Expr.h"
#include "RedisInstance.h"
#include "Finder.h"
#include <capnp/serialize.h>

namespace klee {
    class ExactMatchFinder : Finder<Assignment*>{
    private:

        std::map<std::set<ref<Expr>>, std::string> nameCache;
        RedisInstance instance;
        ::capnp::MallocMessageBuilder mmb;
        std::string serializeToString(std::set<ref<Expr>>& expressions);

    public:
        ExactMatchFinder();
        Assignment** find(std::set<ref<Expr>>& expressions);

        void insert(std::set<ref<Expr>>& expressions, Assignment *value);

        void close();

        std::future<cpp_redis::reply> future_find(std::set<ref<Expr>>& expressions);


        Assignment **processResponse(std::future<cpp_redis::reply>&& reply) const;
    };
}


#endif //KLEE_PCACHE_EXACTMATCHFINDER_H
