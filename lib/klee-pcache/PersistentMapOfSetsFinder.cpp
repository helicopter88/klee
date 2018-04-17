//
// Created by dmarino on 18/02/18.
//

#include <klee/TimerStatIncrementer.h>
#include <klee/SolverStats.h>
#include "PersistentMapOfSetsFinder.h"

namespace klee {
    Assignment **PersistentMapOfSetsFinder::find(std::set<ref<Expr>> &exprs) {
        return persistentCache.get(exprs);
    }

    Assignment **PersistentMapOfSetsFinder::findSpecial(std::set<ref<Expr>> &expr) {
        return persistentCache.tryAll_get(expr);
    }
    void PersistentMapOfSetsFinder::insert(std::set<ref<klee::Expr>> &exprs, Assignment *assignment) {
        TimerStatIncrementer t(stats::pcacheInsertionTime);
        persistentCache.set(exprs, &assignment);
    }

    void PersistentMapOfSetsFinder::storeFinder() {
        persistentCache.store();
    }

    PersistentMapOfSetsFinder::PersistentMapOfSetsFinder(const std::string& path) : persistentCache(path) {

    }
}
