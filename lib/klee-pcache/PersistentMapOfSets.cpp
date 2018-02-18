//
// Created by dmarino on 18/02/18.
//

#include <fstream>
#include "PersistentMapOfSets.h"
#include "klee/Expr.h"
#include "klee/util/Cache.pb.h"

namespace klee {

    struct NullAssignment {
        bool operator()(Assignment *a) const { return !a; }
    };

    struct NonNullAssignment {
        bool operator()(Assignment *a) const { return a != 0; }
    };

    struct NullOrSatisfyingAssignment {
        std::set<ref<Expr>> &key;

        explicit NullOrSatisfyingAssignment(std::set<ref<Expr>> &_key) : key(_key) {}

        bool operator()(Assignment *a) const {
            return !a || a->satisfies(key.begin(), key.end());
        }
    };

    PersistentMapOfSets::PersistentMapOfSets(const std::string &_path) : path(_path) {
        std::ifstream ifs(path);
        auto *pc = new ProtoCache();
        pc->ParseFromIstream(&ifs);
        for (const ProtoCacheElem &e : pc->elem()) {
            std::set<ref<Expr>> exprs;
            for (const ProtoExpr &expr : e.key()) {
                ref<Expr> ex = Expr::deserialize(expr);
                // We couldn't deserialize this expression, bail out from this assignment
                if (!ex.get()) {
                    std::cout << "Deserialization failed!" << std::endl;
                    expr.PrintDebugString();
                    exprs.clear();
                    break;
                }
                exprs.insert(ex);
            }
            if (exprs.empty()) continue;
            Assignment *a = Assignment::deserialize(e.assignment());
            cache.insert(exprs, a);
        }
        ifs.close();
        delete pc;
    }

    Assignment **PersistentMapOfSets::get(std::set<ref<Expr>> &key) {
        Assignment **value = cache.lookup(key);
        if (value) {
            return value;
        }
        // Find a non satisfying subset, if it exists then this expression is unsatisfiable.
        value = cache.findSubset(key, NullAssignment());
        if (!value) {
            // If a superset is satisfiable then this subset must be satisfiable.
            value = cache.findSuperset(key, NonNullAssignment());
        }
        if (!value)
            value = cache.findSubset(key, NullOrSatisfyingAssignment(key));


        return value;
    }

    void PersistentMapOfSets::set(std::set<ref<Expr>> &key, Assignment **const &value) {
        cache.insert(key, *value);
    }

    void PersistentMapOfSets::store() {
        int rem = std::remove(path.c_str());
        if (rem)
            std::cout << "File deleted!" << std::endl;
        std::ofstream ofs(path);
        auto *protoCache = new ProtoCache;
        for (auto c : cache) {
            if (!c.second) {
                auto *pc = new ProtoCacheElem;
                for (const auto &expr : c.first) {
                    pc->mutable_key()->AddAllocated(expr->serialize());
                }
                ProtoAssignment *protoAssignment;
                if(!c.second) {
                    protoAssignment = new ProtoAssignment;
                    protoAssignment->set_nobinding(true);
                } else {
                    protoAssignment = c.second->serialize();
                }
                pc->set_allocated_assignment(protoAssignment);
                protoCache->mutable_elem()->AddAllocated(pc);
            }
        }
        protoCache->SerializeToOstream(&ofs);
        ofs.flush();
        ofs.close();
        delete protoCache;
    }

    PersistentMapOfSets::~PersistentMapOfSets() {
        store();
        cache.clear();
    }
}