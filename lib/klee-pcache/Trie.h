/**Implementation of set-trie: "Index Data Structure for Fast Subset
    and Superset Queries" - Iztok Savnik (http://osebje.famnit.upr.si/~savnik/papers/cdares13.pdf)
**/
#ifndef KLEE_TRIE_H
#define KLEE_TRIE_H


#include <klee/Expr.h>
#include <klee/util/Assignment.h>
#include <functional>
#include "Storage.h"

namespace klee {
    typedef std::set<ref<Expr>>::const_iterator constIterator;

    typedef const std::set<ref<Expr>> Key;


    typedef const std::function<bool(Assignment *)>& Predicate;

    class Trie : public Storage<Key, Assignment **> {

    private:

        struct TrieNode {
            std::unordered_map<unsigned, std::shared_ptr<TrieNode>> children;
            Assignment **value;
            bool last;
        public:
            TrieNode() : value(nullptr), last(false) {}

            void storeNode(CacheTrieNode::Builder &&nodeBuilder) const;

            explicit TrieNode(Assignment **pAssignment) : value(pAssignment), last(false) {}

            explicit TrieNode(std::unordered_map<unsigned, std::shared_ptr<TrieNode>> &&_children, Assignment **_value,
                              bool _last) : children(std::move(_children)), value(_value), last(_last) {}

            ~TrieNode() {
                if (value)
                    delete *value;
                delete value;

                children.clear();
            }
        };

        static std::shared_ptr<TrieNode> createTrieNode() {
            return std::make_shared<TrieNode>();
        }

        static std::shared_ptr<TrieNode> createTrieNode(Assignment **pAssignment) {
            return std::make_shared<TrieNode>(pAssignment);
        }

        typedef std::shared_ptr<TrieNode> tnodePtr;

        size_t size = 0;

        tnodePtr root;

        void insertInternal(tnodePtr node, const Key &, constIterator expr, Assignment **);

        Assignment **searchInternal(const tnodePtr &node, const Key &exprs, constIterator expr) const;

        Assignment **existsSubsetInternal(const tnodePtr &pNode, const Key &exprs,
                                          constIterator expr) const;

        Assignment **existsSupersetInternal(const tnodePtr &pNode, const Key &exprs,
                                            constIterator expr, bool &hasResult) const;

        void dumpNode(const tnodePtr &node) const;

        tnodePtr createTrieNode(const CacheTrieNode::Reader &&node);

        bool getSubsetsInternal(const tnodePtr &pNode, const Key &exprs, constIterator expr,
                                std::vector<Assignment **> &assignments) const;

        Assignment **getSubsetsInternalPredicate(const tnodePtr &pNode, const Key &exprs, constIterator expr,
                                          Predicate predicate) const;

        bool getSupersetsInternal(const tnodePtr &pNode, const Key &exprs,
                                  constIterator expr, std::vector<Assignment **> &assignments) const;

        void findAssignments(const tnodePtr &shared_ptr, std::vector<Assignment **> &set) const;

        Assignment** getSupersetsInternalPredicate(const tnodePtr &pNode, const Key &exprs,
                                                         constIterator expr, Predicate predicate) const;

        Assignment **findAssignment(const tnodePtr &shared_ptr, Predicate function, size_t counter, size_t limit) const;
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

        /**
         * Finds all subsets of a constraint set
         * @param exprs The constraint set to be used for lookup
         * @return A vector (possibly empty) containing all the stored subsets inside the trie
         */
        std::vector<Assignment **> getSubsets(const Key &exprs) const;

        /**
         * Finds all supersets of a constraint set
         * @param exprs The constraint set to be used for lookup
         * @return A vector (possibly empty) containing all the stored supersets inside the trie
         */
        std::vector<Assignment **> getSupersets(const Key &exprs) const;

        Assignment **findFirstSubsetMatching(const Key &exprs, Predicate predicate) const;

        Assignment **findFirstSupersetMatching(const Key &exprs, Predicate predicate) const;

    };
}

#endif //KLEE_TRIE_H
