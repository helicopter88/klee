//
// Created by dmarino on 12/04/18.
//

#include <klee/util/Assignment.h>
#include "TrieFinder.h"
#include "NameNormalizer.h"

namespace klee {
    Assignment **TrieFinder::find(std::set<ref<Expr>> &exprs) {
        NameNormalizer nn;
        Assignment **ret = trie.search(exprs);
        if (ret) {
            return ret;
        }
        ret = trie.existsSubset(exprs);
        if (ret && !*ret) {
            *ret = nullptr;
            return ret;
        }
        return nullptr;
    }

    void TrieFinder::insert(std::set<ref<Expr>> &exprs, Assignment *assignment) {
        NameNormalizer nn;
        trie.insert(exprs, assignment);
    }

    void TrieFinder::close() {
        /** TODO: STORE**/
    }
};