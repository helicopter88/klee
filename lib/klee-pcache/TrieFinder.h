#ifndef KLEE_TRIEFINDER_H
#define KLEE_TRIEFINDER_H

#include <klee/util/Assignment.h>
#include "Finder.h"
#include "Trie.h"

namespace klee {
    class TrieFinder : public Finder<Assignment*> {
    private:
        Trie trie;
        std::string path;
    public:
        Assignment **find(std::set<ref<Expr>> &exprs) override;

        void insert(std::set<ref<Expr>> &exprs, Assignment *assignment) override;

        Assignment **findSpecial(std::set<ref<Expr>>& exprs) override;

        void storeFinder() override;

        explicit TrieFinder(const std::string& opt);
    };
}
#endif //KLEE_TRIEFINDER_H
