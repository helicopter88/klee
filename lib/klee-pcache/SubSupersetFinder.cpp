//
// Created by dmarino on 18/02/18.
//

#include "SubSupersetFinder.h"

namespace klee {
    Assignment **SubSupersetFinder::find(std::set<ref<Expr>> &exprs) {
        return persistentCache.get(exprs);
    }

    void SubSupersetFinder::insert(std::set<ref<klee::Expr>> &exprs, Assignment *assignment) {
        persistentCache.set(exprs, &assignment);
    }

    void SubSupersetFinder::close() {
        persistentCache.store();
    }
}
