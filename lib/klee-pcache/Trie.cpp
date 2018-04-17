/** TODO: ADD PERSISTENCE **/
#include <capnp/message.h>
#include "Trie.h"
#include "../Solver/STPBuilder.h"

namespace klee {
    void Trie::dumpNode(const TrieNode *node) const {
        //if(node->value) node->value-dump(); else llvm::errs() << "No assignment";
        llvm::errs() << " Is last: " << node->last << "\n";
        for (const auto &m : node->children) {
            m.first->dump();
            llvm::errs() << "\t";
            dumpNode(m.second);
            llvm::errs() << " | ";
        }
    }

    void Trie::dump() const {
        for (const auto &m : root->children) {
            m.first->dump();
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
        auto iter = node->children.find(*expr);
        if (iter != node->children.cend()) {
            next = iter->second;
        } else {
            next = createTrieNode();
            node->children.insert(std::make_pair(*expr, next));
        }
        insertInternal(next, exprs, ++expr, ass);
    }

    void Trie::insert(const std::set<ref<Expr>> &exprs, Assignment *ass) {
        if(exprs.empty()) {
            return;
        }
        size++;
        Assignment **pAssignment = new Assignment*;
        *pAssignment = ass;
        insertInternal(root, exprs, exprs.cbegin(), pAssignment);
    }


    Assignment **
    Trie::searchInternal(TrieNode *node, const std::set<ref<Expr>> &exprs, std::set<ref<Expr>>::const_iterator expr,
                         bool &hasSolution) const {
        if (expr == exprs.cend()) {
            hasSolution = true;
            return node->value;
        }

        auto iter = node->children.find(*expr);
        if (iter == node->children.end()) {
            hasSolution = false;
            return nullptr;
        }
        return searchInternal(iter->second, exprs, ++expr, hasSolution);

    }

    Assignment **Trie::search(const std::set<ref<Expr>> &exprs, bool &hasSolution) const {
        assert(!this->root->last && "LAST SET FOR ROOT");
        assert(!this->root->value && "VALUE SET FOR ROOT");
        if (!size) {
            hasSolution = false;
            return nullptr;
        }
        return searchInternal(root, exprs, exprs.cbegin(), hasSolution);
    }

    Assignment **Trie::existsSubsetInternal(TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                           std::set<ref<Expr>>::iterator expr) const {
        if (pNode->last) {
            return pNode->value;
        }
        if (expr == exprs.end()) {
            return nullptr;
        }
        Assignment **found = nullptr;
        auto iter = pNode->children.find(*expr);
        if (iter != pNode->children.end()) {
            found = existsSubsetInternal(iter->second, exprs, ++expr);
        }
        if (!found) {
            return existsSubsetInternal(pNode, exprs, ++expr);
        }
        return found;
    }

    Assignment **Trie::existsSupersetInternal(TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                             std::set<ref<Expr>>::iterator expr) const {

        if (expr == exprs.end()) {
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
            if (found) {
                return found;
            }

            if (child.first == expr->get()) {
                found = existsSupersetInternal(child.second, exprs, ++expr);
            } else {
                found = existsSupersetInternal(child.second, exprs, expr);
            }
        }
        return found;
    }

    Assignment **Trie::existsSuperset(const std::set<ref<Expr>> &exprs) const {
        return existsSupersetInternal(root, exprs, exprs.cbegin());
    }

    Assignment **Trie::existsSubset(const std::set<ref<Expr>> &exprs) const {
        return existsSubsetInternal(root, exprs, exprs.cbegin());
    }

    void Trie::store(CacheTrie::Builder &&builder) const {
        root->storeNode(std::forward<CacheTrieNode::Builder>(builder.initRoot()));
    }

    Trie::TrieNode* Trie::createTrieNode(CacheTrieNode::Reader &&node) {
        Assignment **pAssignment = nullptr;
        size++;
        if (node.hasValue()) {
            pAssignment = new Assignment*;
            *pAssignment = Assignment::deserialize(node.getValue());
        }
        std::map<ref<Expr>, TrieNode *> children;
        for (const CacheTrieNode::Child::Reader &child : node.getChildren()) {
            children.insert(std::make_pair(Expr::deserialize(child.getExpr()), createTrieNode(child.getNode())));
        }
        return new TrieNode(std::move(children), pAssignment, (bool) node.getLast());
    }


    void Trie::TrieNode::storeNode(CacheTrieNode::Builder &&nodeBuilder) const {
        nodeBuilder.setLast(static_cast<uint8_t>(this->last));
        if (value && *value) {
            (*value)->serialize(nodeBuilder.initValue());
        }
        unsigned i = 0;
        auto map = nodeBuilder.initChildren(children.size());
        for (const auto &child : children) {
            child.first->serialize(map[i].initExpr());
            child.second->storeNode(map[i].initNode());
            i++;
        }
    }
}