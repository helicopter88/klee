//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_EXACTMATCHFINDER_H
#define KLEE_PCACHE_EXACTMATCHFINDER_H


#include "klee/Solver.h"
#include "klee/Expr.h"
#include "RedisInstance.h"
#include "Finder.h"
#include "Trie.h"
#include <capnp/serialize.h>

namespace klee {
    class RedisFinder : public Finder<Assignment *> {
    private:

        std::map<std::set<ref<Expr>>, std::string> nameCache;
        RedisInstance instance;
        ::capnp::MallocMessageBuilder lookupMessageBuilder;

        std::string serializeToString(const std::set<ref<Expr>> &expressions);

    public:
        RedisFinder(const std::string& url = "127.0.0.1", size_t port = 6379, int dbNum = 0);

        Assignment **find(std::set<ref<Expr>> &expressions) final;

        void insert(std::set<ref<Expr>> &expressions, Assignment *value) final;

        void storeFinder() final;

        std::future<cpp_redis::reply> future_find(std::set<ref<Expr>> &expressions);


        Assignment **processResponse(std::future<cpp_redis::reply> &&reply) const;
    };
}


#endif //KLEE_PCACHE_EXACTMATCHFINDER_H
