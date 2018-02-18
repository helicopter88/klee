//
// Created by dmarino on 18/02/18.
//

#include <fstream>
#include "PersistentMapOfSets.h"
#include "klee/Expr.h"
#include "klee/util/Cache.pb.h"

namespace klee {
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

    Assignment *const PersistentMapOfSets::get(const std::set<ref<Expr>> &&key) {
        Assignment **const value = cache.lookup(key);
        if (value) {
            return *value;
        }
        // TODO: perform subset/superset
        return nullptr;
    }

    void PersistentMapOfSets::set(const std::set<ref<Expr>> &&key, Assignment *const &&value) {
        cache.insert(key, value);
    }

    void PersistentMapOfSets::store() {
        int rem = std::remove(path.c_str());
        if (rem)
            std::cout << "File deleted!" << std::endl;
        std::ofstream ofs(path);
        auto *protoCache = new ProtoCache;
        for (const auto &c : cache) {
            if (c.second) {
                auto *pc = new ProtoCacheElem;
                for (const auto &expr : c.first) {
                    pc->mutable_key()->AddAllocated(expr->serialize());
                }
                pc->set_allocated_assignment(c.second->serialize());
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
        for(auto binding : cache) {
            delete binding.second;
        }
    }
}