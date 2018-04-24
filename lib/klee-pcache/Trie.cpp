/** TODO: ADD PERSISTENCE **/
#include <capnp/message.h>
#include <klee/SolverStats.h>
#include "Trie.h"

using namespace llvm;
namespace klee {
    void Trie::dumpNode(const TrieNode *node) const {
        llvm::errs() << " Is last: " << node->last << "\n";
        for (const auto &m : node->children) {
            llvm::errs() << "\t";
            dumpNode(m.second);
            llvm::errs() << " | ";
        }
    }

    void Trie::dump() const {
        for (const auto &m : root->children) {
            llvm::errs() << "\n\t";
            dumpNode(m.second);
        }
    }

    void
    Trie::insertInternal(TrieNode *node, const std::set<ref<Expr>> &exprs, std::set<ref<Expr>>::const_iterator expr,
                         Assignment **ass) {
        if (expr == exprs.cend()) {
            node->last = true;
            node->value = ass;
            return;
        }
        TrieNode *next;
        auto iter = node->children.find((*expr)->hash());
        if (iter != node->children.cend()) {
            next = iter->second;
        } else {
            next = createTrieNode();
            node->children.insert(std::make_pair((*expr)->hash(), next));
        }
        insertInternal(next, exprs, ++expr, ass);
    }

    void Trie::insert(const std::set<ref<Expr>> &exprs, Assignment *ass) {
        if (exprs.empty()) {
            return;
        }
        ++stats::pcacheTrieSize;
        ++size;
        Assignment **pAssignment = new Assignment *;
        *pAssignment = ass;
        insertInternal(root, exprs, exprs.cbegin(), pAssignment);
    }


    Assignment **Trie::searchInternal(TrieNode *node, const Key &exprs, constIterator expr) const {
        if (expr == exprs.cend()) {
            return node->value;
        }

        const auto& iter = node->children.find((*expr)->hash());
        if (iter == node->children.cend()) {
            return nullptr;
        }
        return searchInternal(iter->second, exprs, ++expr);

    }

    Assignment **Trie::search(const Key &exprs) const {
        assert(!this->root->last && "LAST SET FOR ROOT");
        assert(!this->root->value && "VALUE SET FOR ROOT");
        if (!size) {
            return nullptr;
        }
        return searchInternal(root, exprs, exprs.cbegin());
    }

    Assignment **Trie::existsSubsetInternal(const TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                            constIterator expr) const {
        if (pNode->last) {
            return pNode->value;
        }
        if (expr == exprs.cend()) {
            return nullptr;
        }
        auto iter = pNode->children.find((*expr)->hash());
        if (iter != pNode->children.cend()) {
            return existsSubsetInternal(iter->second, exprs, ++expr);
        }
        return existsSubsetInternal(pNode, exprs, ++expr);
    }

    Assignment **Trie::existsSupersetInternal(const TrieNode *pNode, const Key &exprs,
                                              constIterator expr, bool &hasResult) const {

        if (expr == exprs.cend()) {
            hasResult = true;
            return pNode->value;
        }
        Assignment **found = nullptr;
        //const ref<Expr>& from = *expr;
        //auto upto = ++expr;
        //auto lowerBound = pNode->children.lower_bound(from);
        //std::map<ref<Expr>, TrieNode*>::iterator upperBound;
        /*if(upto != exprs.end()) {
            upperBound = pNode->children.upper_bound(*upto);
        } else {
            upperBound = pNode->children.end();
        }*/

        // TODO: figure out how to lowerbound & upperbound
        for (auto &child : pNode->children) {
            if (child.first == expr->get()->hash()) {
                found = existsSupersetInternal(child.second, exprs, ++expr, hasResult);
            } else {
                found = existsSupersetInternal(child.second, exprs, expr, hasResult);
            }
            if (hasResult) {
                return found;
            }
        }
        hasResult = false;
        return found;
    }

    Assignment **Trie::existsSuperset(const Key &exprs) const {
        bool hasResult = false;
        return existsSupersetInternal(root, exprs, exprs.cbegin(), hasResult);
    }

    Assignment **Trie::existsSubset(const Key &exprs) const {
        return existsSubsetInternal(root, exprs, exprs.cbegin());
    }

    void Trie::store(CacheTrie::Builder &&builder) const {
        root->storeNode(std::forward<CacheTrieNode::Builder>(builder.initRoot()));
    }

    Trie::TrieNode *Trie::createTrieNode(CacheTrieNode::Reader &&node) {
        Assignment **pAssignment = nullptr;
        size++;
        if (node.hasValue()) {
            ++klee::stats::pcacheTrieSize;
            pAssignment = new Assignment *;
            *pAssignment = Assignment::deserialize(node.getValue());
        }
        std::map<unsigned, TrieNode *> children;
        for (const CacheTrieNode::Child::Reader &child : node.getChildren()) {
            children.insert(std::make_pair(child.getExpr(), createTrieNode(child.getNode())));
        }
        return new TrieNode(std::move(children), pAssignment, (bool) node.getLast());
    }


    void Trie::TrieNode::storeNode(CacheTrieNode::Builder &&nodeBuilder) const {
        nodeBuilder.setLast(this->last);
        if (value && *value) {
            (*value)->serialize(nodeBuilder.initValue());
        }
        unsigned i = 0;
        auto map = nodeBuilder.initChildren(children.size());
        for (const auto &child : children) {
            map[i].setExpr(child.first);
            child.second->storeNode(map[i].initNode());
            i++;
        }
    }
}