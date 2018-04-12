/** TODO: ADD PERSISTENCE **/
#include "Trie.h"

namespace klee {
    void Trie::dumpNode(const TrieNode *node) {
        for (const auto &m : node->children) {
            m.first->dump();
            llvm::errs() << "\t";
            dumpNode(m.second);
            llvm::errs() << " | ";
        }
    }

    void Trie::dump() {
        for (const auto &m : root->children) {
            m.first->dump();
            llvm::errs() << "\n\t";
            dumpNode(m.second);
        }
    }

    void Trie::insertInternal(TrieNode *node, const std::set<ref<Expr>> &exprs, std::set<ref<Expr>>::iterator expr, Assignment** ass) {
        if (expr == exprs.end()) {
            node->last = true;
            node->value = ass;
            return;
        }
        TrieNode *next;
        auto iter = node->children.find(*expr);
        if (iter != node->children.end()) {
            next = iter->second;
        } else {
            next = createTrieNode(ass);
            node->children.insert(std::make_pair(*expr, next));
        }
        insertInternal(next, exprs, ++expr, ass);
    }

    void Trie::insert(const std::set<ref<Expr>> &exprs, Assignment* ass) {
        Assignment** pAssignment = new Assignment*;
        *pAssignment = ass;
        insertInternal(root, exprs, exprs.begin(), pAssignment);
    }


    Assignment** Trie::searchInternal(TrieNode *node, const std::set<ref<Expr>> &exprs, std::set<ref<Expr>>::iterator expr) {
        if (expr == exprs.end()) {
            return node->value;
        }

        auto iter = node->children.find(*expr);
        if (iter == node->children.end()) {
            return nullptr;
        }
        return searchInternal(iter->second, exprs, ++expr);

    }

    Assignment** Trie::search(const std::set<ref<Expr>> &exprs) {
        return searchInternal(root, exprs, exprs.begin());
    }

    Assignment** Trie::existsSubsetInternal(TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                    std::set<ref<Expr>>::iterator expr) {
        if (pNode->last) {
            return pNode->value;
        }
        if (expr == exprs.end()) {
            return nullptr;
        }
        Assignment** found = nullptr;
        auto iter = pNode->children.find(*expr);
        if (iter != pNode->children.end()) {
            found = existsSubsetInternal(iter->second, exprs, ++expr);
        }
        if (!found) {
            return existsSubsetInternal(pNode, exprs, ++expr);
        }
        return found;
    }

    Assignment** Trie::existsSupersetInternal(TrieNode *pNode, const std::set<ref<Expr>> &exprs,
                                      std::set<ref<Expr>>::iterator expr) {

        if(expr == exprs.end()) {
            return pNode->value;
        }
        Assignment** found = nullptr;
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

    Assignment** Trie::existsSuperset(const std::set<ref<Expr>> &exprs) {
        return existsSupersetInternal(root, exprs, exprs.begin());
    }

    Assignment** Trie::existsSubset(const std::set<ref<Expr>> &exprs) {
        return existsSubsetInternal(root, exprs, exprs.begin());
    }
}