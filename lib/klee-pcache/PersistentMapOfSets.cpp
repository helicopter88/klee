//
// Created by dmarino on 18/02/18.
//

#include <fstream>
#include <klee/util/Cache.pb.h>
#include <llvm/Support/Path.h>
#include "PersistentMapOfSets.h"
#include "klee/Expr.h"
#include "klee/util/Cache.pb.h"

using namespace llvm;
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
        for (int i = 0; i <= INT_MAX; i++) {
            SmallString<128> s(_path);
            llvm::sys::path::append(s, "cache" + std::to_string(i) + ".bin");
            std::ifstream ifs(s.c_str());
            if (ifs.fail()) {
                break;
            }
            ProtoCache *pc = new ProtoCache;
            if (!pc->ParseFromIstream(&ifs)) {
                //llvm::errs() << pc->ShortDebugString();
                llvm::errs() << pc->InitializationErrorString() << "\n";
                llvm::errs() << s.c_str() << "\n";
            }

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
                if (exprs.empty()) {
                    continue;
                };
                Assignment *a = Assignment::deserialize(e.assignment());
                set(exprs, &a);
            }
            delete pc;
        }
        std::cout << "Cache size: " << size << std::endl;
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
        size++;
        cache.insert(key, *value);
    }

    const char *nextFile(SmallString<128> &p, int &i, const std::string &thing) {
        llvm::sys::path::remove_filename(p);
        llvm::sys::path::append(p, thing + std::to_string(++i) + ".bin");
        std::remove(p.c_str());
        return p.c_str();
    }

    void PersistentMapOfSets::store() {
        int i = -1;
        SmallString<128> p(path);
        llvm::sys::fs::create_directory(p.c_str());
        p.append("/placeholder");
        auto *pc = new ProtoCache;
        for (auto c : cache) {
            auto *protoCacheElem = pc->add_elem();
            for (const auto &expr : c.first) {
                protoCacheElem->mutable_key()->AddAllocated(expr->serialize());
            }
            ProtoAssignment *protoAssignment;
            if (!c.second) {
                protoAssignment = new ProtoAssignment;
                protoAssignment->set_nobinding(true);
            } else {
                protoAssignment = c.second->serialize();
                protoAssignment->set_nobinding(false);
            }
            protoCacheElem->set_allocated_assignment(protoAssignment);
        }
        std::ofstream keyO(nextFile(p, i, "cache"), std::ios::binary | std::ios::out);
        pc->SerializeToOstream(&keyO);
        std::cout << "Cache size: " << size << std::endl;
    }
}