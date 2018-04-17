//
// Created by dmarino on 13/04/18.
//

#ifndef KLEE_NAMENORMALIZERFINDER_H
#define KLEE_NAMENORMALIZERFINDER_H

#include "Finder.h"

namespace klee {
    class NameNormalizerFinder : public Finder<Assignment *> {
    private:
        Finder &f;
    public:
        Assignment **find(std::set<ref<Expr>> &exprs) override;

        void insert(std::set<ref<Expr>> &exprs, Assignment *assignment) override;

        void storeFinder() final { f.storeFinder(); };

        Assignment **findSpecial(std::set<ref<Expr>>& exprs) override;

        explicit NameNormalizerFinder(Finder &_f) : f(_f) {};
    };
}


#endif //KLEE_NAMENORMALIZERFINDER_H
