//
// Created by dmarino on 13/04/18.
//

#ifndef KLEE_NAMENORMALIZERFINDER_H
#define KLEE_NAMENORMALIZERFINDER_H

#include "Finder.h"

namespace klee {
    class NameNormalizerFinder : public Finder {
    private:
        std::vector<Finder *> finders;
    public:
        Assignment **find(std::set<ref<Expr>> &exprs) override;

        void insert(std::set<ref<Expr>> &exprs, Assignment *assignment) override;

        void storeFinder() final {
            for (Finder *finder : finders) {
                finder->storeFinder();
            }
        };

        Assignment **findSpecial(std::set<ref<Expr>> &exprs) override;

        NameNormalizerFinder(std::initializer_list<Finder *> _f) : finders(_f) {};

        ~NameNormalizerFinder() override {
            for (Finder *finder : finders) {
                delete finder;
            }
        }

        void printStats() const override {
            llvm::errs() << "NameNormalized: \n";
            for (Finder *finder : finders) {
                finder->printStats();
            }
            llvm::errs() << "NameNormalized end\n";
        }
    };
}


#endif //KLEE_NAMENORMALIZERFINDER_H
