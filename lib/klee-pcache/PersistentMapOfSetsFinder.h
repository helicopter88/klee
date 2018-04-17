//
// Created by dmarino on 18/02/18.
//

#ifndef KLEE_SUBSUPERSETFINDER_H
#define KLEE_SUBSUPERSETFINDER_H

#include "Finder.h"
#include "PersistentMapOfSets.h"

namespace klee {
    class PersistentMapOfSetsFinder : public Finder<Assignment*> {
    private:
        PersistentMapOfSets persistentCache;
    public:
        Assignment** find(std::set<ref<Expr>>& exprs) override;

        Assignment** findSpecial(std::set<ref<Expr>>& expr) override;

        void insert(std::set<ref<Expr>>& exprs, Assignment* assignment) override;

        void storeFinder() override;

        explicit PersistentMapOfSetsFinder(const std::string& opt);
    };
}


#endif //KLEE_SUBSUPERSETFINDER_H
