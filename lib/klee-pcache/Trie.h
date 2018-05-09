/**Implementation of set-trie: "Index Data Structure for Fast Subset
    and Superset Queries" - Iztok Savnik (http://osebje.famnit.upr.si/~savnik/papers/cdares13.pdf)
**/
#ifndef KLEE_TRIE_H
#define KLEE_TRIE_H

#include <klee/Expr.h>
#include <klee/util/Assignment.h>
#include "Storage.h"

namespace klee {
    typedef std::set<ref<Expr>>::const_iterator constIterator;

    typedef const std::set<ref<Expr>> Key;

    class Trie : public Storage<Key, Assignment **> {

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

        void insertInternal(TrieNode *node, const Key &, constIterator expr, Assignment **);

        Assignment **searchInternal(TrieNode *node, const Key &exprs, constIterator expr) const;

        Assignment **existsSubsetInternal(const TrieNode *pNode, const Key &exprs,
                                          constIterator expr) const;

        Assignment **existsSupersetInternal(const TrieNode *pNode, const Key &exprs,
                                            constIterator expr, bool &hasResult) const;

        void dumpNode(const TrieNode *node) const;

        TrieNode *createTrieNode(const CacheTrieNode::Reader &&node);

    public:
        explicit Trie() : root(createTrieNode()) {
        }

        explicit Trie(CacheTrie::Reader &&builder) :
                root(createTrieNode(builder.getRoot())) {
        }


        Assignment **existsSubset(const Key &exprs) const;

        Assignment **existsSuperset(const Key &exprs) const;

        void dump() const;

        void store(CacheTrie::Builder &&node) const;

        /** From Storage**/
        Assignment **get(Key &key) override;

        void set(Key &key, Assignment **const &value) override;


        void store() override {
            assert(false && "Do not use this method, use store(CacheTrie::Builder) instead");
        };

    };
}

#endif //KLEE_TRIE_H
