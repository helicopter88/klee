/**Implementation of set-trie: "Index Data Structure for Fast Subset
    and Superset Queries" - Iztok Savnik (http://osebje.famnit.upr.si/~savnik/papers/cdares13.pdf)
**/
#ifndef KLEE_TRIE_H
#define KLEE_TRIE_H

#include <klee/Expr.h>
#include <klee/util/Assignment.h>

namespace klee {
    struct ExprComparator {
    public:
        bool operator()(const ref<Expr> &lhs, const ref<Expr> &rhs) const {
            return lhs->hash() < rhs->hash();
        }
    };

    class Trie {

    private:

        struct TrieNode {
            std::map<unsigned, TrieNode *> children;
            Assignment **value;
            bool last;
        public:
            TrieNode() : value(nullptr), last(false) {}

            void storeNode(CacheTrieNode::Builder &&nodeBuilder) const;

            explicit TrieNode(Assignment **pAssignment) : value(pAssignment), last(false) {}

            explicit TrieNode(std::map<unsigned, TrieNode *> &&_children, Assignment **_value,
                              bool _last) : children(_children), value(_value), last(_last) {}
        };

        static TrieNode *createTrieNode() {
            auto *newNode = new TrieNode;
            return newNode;
        }

        static TrieNode *createTrieNode(Assignment **pAssignment) {
            auto *newNode = new TrieNode(pAssignment);
            return newNode;
        }


        size_t size = 0;

        TrieNode *root;

        void insertInternal(TrieNode *node,
                            const std::set<ref<Expr>> &, std::set<ref<Expr>>::const_iterator expr, Assignment **);

        Assignment **searchInternal(TrieNode *node,
                                    const std::set<ref<Expr>> &, std::set<ref<Expr>>::const_iterator expr,
                                    bool &hasSolution) const;

        Assignment **existsSubsetInternal(const TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                          std::set<ref<Expr>>::const_iterator expr) const;

        Assignment **existsSupersetInternal(const TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                            std::set<ref<Expr>>::const_iterator expr, bool &hasResult) const;

        void dumpNode(const TrieNode *node) const;

        TrieNode *createTrieNode(CacheTrieNode::Reader &&node);

    public:
        explicit Trie() : root(createTrieNode()) {
        }

        explicit Trie(CacheTrie::Reader &&builder) :
                root(createTrieNode(builder.getRoot())) {
        }

        void insert(const std::set<ref<Expr>> &exprs, Assignment *pAssignment);

        Assignment **search(const std::set<ref<Expr>> &exprs, bool &hasSolution) const;

        Assignment **existsSubset(const std::set<ref<Expr>> &exprs) const;

        Assignment **existsSuperset(const std::set<ref<Expr>> &exprs) const;

        void dump() const;

        void store(CacheTrie::Builder &&node) const;
    };
}

#endif //KLEE_TRIE_H
