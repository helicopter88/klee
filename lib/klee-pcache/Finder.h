//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_FINDER_H
#define KLEE_PCACHE_FINDER_H

#include <klee/util/Assignment.h>
#include <klee/Expr.h>

namespace klee {
    class Finder {
    private:
        size_t hits = 0;
        size_t misses = 0;
        size_t specialHits = 0, specialMisses = 0;
    public:
        virtual Assignment **find(std::set<ref<Expr>> &exprs) = 0;

        virtual Assignment **findSpecial(std::set<ref<Expr>> &exprs) {
            return find(exprs);
        }

        virtual void insert(std::set<ref<Expr>> &exprs, Assignment *assignment) = 0;

        virtual void storeFinder() = 0;

        virtual ~Finder() {};

        void incrementSpecialHits() {
            specialHits++;
        }

        void incrementSpecialMisses() {
            specialMisses++;
        }

        void incrementHits() {
            hits++;
        }

        void incrementMisses() {
            misses++;
        }

        virtual std::string name() const { return ""; };

        virtual void printStats() const {
            llvm::errs() << name() << " H: " << hits << " M: " << misses << "\n";
            llvm::errs() << name() << " SH: " << specialHits << " SM: " << specialMisses << "\n";
        }
    };
}

#endif //KLEE_PCACHE_FINDER_H
