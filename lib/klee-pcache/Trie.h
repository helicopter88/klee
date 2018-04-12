/**Implementation of set-trie: "Index Data Structure for Fast Subset
    and Superset Queries" - Iztok Savnik (http://osebje.famnit.upr.si/~savnik/papers/cdares13.pdf)
**/
#ifndef KLEE_TRIE_H
#define KLEE_TRIE_H

#include <klee/Expr.h>
#include <klee/util/Assignment.h>

namespace klee {
    class Trie {
        struct ExprComparator {
        public:
            bool operator()(const ref<Expr> &lhs, const ref<Expr> &rhs) const {
                return lhs->hash() < rhs->hash();
            }
        };

    private:
        struct TrieNode {
            std::map<ref<Expr>, TrieNode *, ExprComparator> children;
            Assignment **value;
            bool last;
        public:
            TrieNode() : value(nullptr), last(false) {}

            explicit TrieNode(Assignment **pAssignment) : value(pAssignment), last(false) {}
        };

        static TrieNode *createTrieNode() {
            auto *newNode = new TrieNode;
            return newNode;
        }

        static TrieNode *createTrieNode(Assignment **pAssignment) {
            auto *newNode = new TrieNode(pAssignment);
            return newNode;
        }

        TrieNode *root;

        void insertInternal(TrieNode *node,
                            const std::set<ref<Expr>> &, std::set<ref<Expr>>::iterator expr, Assignment **);

        Assignment **searchInternal(TrieNode *node,
                                    const std::set<ref<Expr>> &, std::set<ref<Expr>>::iterator expr);

        Assignment **existsSubsetInternal(TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                          std::set<ref<Expr>>::iterator expr);

        Assignment **existsSupersetInternal(TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                            std::set<ref<Expr>>::iterator expr);

        void dumpNode(const TrieNode *node);

    public:
        explicit Trie() : root(createTrieNode()) {

        }

        void insert(const std::set<ref<Expr>> &exprs, Assignment *pAssignment);

        Assignment **search(const std::set<ref<Expr>> &exprs);

        Assignment **existsSubset(const std::set<ref<Expr>> &exprs);

        Assignment **existsSuperset(const std::set<ref<Expr>> &exprs);

        void dump();
    };
}

#endif //KLEE_TRIE_H
