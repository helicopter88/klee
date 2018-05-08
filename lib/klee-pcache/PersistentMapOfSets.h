//
// Created by dmarino on 18/02/18.
//

#ifndef KLEE_PERSISTENTMAPOFSETS_H
#define KLEE_PERSISTENTMAPOFSETS_H


#include "Storage.h"

#include <klee/util/Assignment.h>
#include <klee/Internal/ADT/MapOfSets.h>
#include "klee/Expr.h"
namespace klee {
    class PersistentMapOfSets : public Storage<std::set<ref<Expr>>, Assignment **> {
    private:
        unsigned size = 0;
        const std::string path;
        MapOfSets<unsigned, Assignment *> cache;
    public:
        explicit PersistentMapOfSets(const std::string &_path);

        Assignment** get(std::set<ref<Expr>> &key) final;
        Assignment** tryAll_get(std::set<ref<Expr>>& key);

        void set(std::set<ref<Expr>> &key, Assignment **const& value) final;

        void store() final;

    };
}

#endif //KLEE_PERSISTENTMAPOFSETS_H
