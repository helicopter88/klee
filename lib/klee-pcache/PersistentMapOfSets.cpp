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
        auto *pc = new ProtoCache();
        for (int i = 0; i <= INT_MAX; i++) {
            SmallString<128> s(_path);
            SmallString<128> p(_path);
            llvm::sys::path::append(s, "cache" + std::to_string(i) + ".bin");
            llvm::sys::path::append(p, "ass" + std::to_string(i) + ".bin");
            std::ifstream ifs(s.c_str());
            std::ifstream assIfs(p.c_str());
            if (ifs.fail()) {
                break;
            }
            ProtoCacheElem* e = new ProtoCacheElem;
            if (!e->ParseFromIstream(&ifs)) {
                //llvm::errs() << pc->ShortDebugString();
                llvm::errs() << pc->InitializationErrorString() << "\n";
                llvm::errs() << s.c_str() << "\n";
            }

            std::set<ref<Expr>> exprs;
            for (const ProtoExpr &expr : e->key()) {
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
            if (exprs.empty()) { errs() << "Hello!";
                continue;};
            ProtoAssignment* pA = new ProtoAssignment;
            pA->ParseFromIstream(&assIfs);
            Assignment *a = Assignment::deserialize(*pA);
            set(exprs, &a);
            ifs.close();
            assIfs.close();
            delete e;
            delete pA;
        }
        std::cout << "Cache size: " << size << std::endl;
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
        size++;
        cache.insert(key, *value);
    }

    const char *nextFile(SmallString<128> &p, int &i, const std::string& thing) {
        llvm::sys::path::remove_filename(p);
        llvm::sys::path::append(p, thing + std::to_string(++i) + ".bin");
        std::remove(p.c_str());
        return p.c_str();
    }

    void PersistentMapOfSets::store() {
        int i = -1;
        int j = -1;
        SmallString<128> p(path);
        llvm::sys::fs::create_directory(p.c_str());
        p.append("/placeholder");
        std::ofstream keyOfs(nextFile(p, i, "cache"));
        std::ofstream assOfs(nextFile(p, j, "ass"));
        for (auto c : cache) {
            auto *pc = new ProtoCacheElem;
            for (const auto &expr : c.first) {
                pc->mutable_key()->AddAllocated(expr->serialize());
            }
            ProtoAssignment *protoAssignment;
            if (!c.second) {
                protoAssignment = new ProtoAssignment;
                protoAssignment->set_nobinding(true);
            } else {
                protoAssignment = c.second->serialize();
                protoAssignment->set_nobinding(false);
            }
            //pc->set_allocated_assignment(protoAssignment);
            std::ofstream keyO(nextFile(p, i, "cache"), std::ios::binary | std::ios::out);
            //pc->SerializeToOstream(&keyO);
            for(const ProtoExpr &e : pc->key()) {
                e.SerializeToOstream(&keyO);
            }
            keyO.flush();
            keyO.close();
            std::ifstream in(p.c_str());
            auto* pc1 = new ProtoCacheElem;
            if(!pc1->ParseFromIstream(&in)) {
                errs() << pc->ShortDebugString() << "\n\n";
                errs() << pc1->ShortDebugString() << "\n\n";
                errs() << pc1->InitializationErrorString() << "\n\n";
            } else {
                errs() << "Parsed!" << "\n\n";
            }
            std::ofstream assO(nextFile(p, j, "ass"), std::ios::binary | std::ios::out);
            protoAssignment->SerializeToOstream(&assO);
            assO.flush();
            delete pc;
        }
        std::cout << "Cache size: " << size << std::endl;
    }
}