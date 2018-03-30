//
// Created by dmarino on 18/02/18.
//

#ifndef KLEE_SUBSUPERSETFINDER_H
#define KLEE_SUBSUPERSETFINDER_H

#include "Finder.h"
#include "PersistentMapOfSets.h"

namespace klee {
    class SubSupersetFinder : public Finder<Assignment*> {
    private:
        PersistentMapOfSets persistentCache;
    public:
        Assignment** find(std::set<ref<Expr>>& exprs) override;

        Assignment** findSubSuperSet(std::set<ref<Expr>>& expr);

        void insert(std::set<ref<Expr>>& exprs, Assignment* assignment) override;

        void close() override;
    };
}


#endif //KLEE_SUBSUPERSETFINDER_H
