#include <capnp/message.h>
#include <klee/SolverStats.h>
#include "Trie.h"

using namespace llvm;
namespace klee {
    void Trie::dumpNode(const tnodePtr &node) const {
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

    void Trie::insertInternal(tnodePtr node, const Key &exprs, constIterator expr, Assignment **ass) {
        if (expr == exprs.cend()) {
            node->last = true;
            node->value = ass;
            return;
        }
        tnodePtr next;
        auto iter = node->children.find((*expr)->hash());
        if (iter != node->children.cend()) {
            next = iter->second;
        } else {
            next = createTrieNode();
            node->children.insert(std::make_pair((*expr)->hash(), next));
        }
        insertInternal(next, exprs, ++expr, ass);
    }

    Assignment **Trie::searchInternal(const tnodePtr &node, const Key &exprs, constIterator expr) const {
        if (expr == exprs.cend()) {
            return node->value;
        }

        const auto &iter = node->children.find((*expr)->hash());
        if (iter == node->children.cend()) {
            return nullptr;
        }
        return searchInternal(iter->second, exprs, ++expr);

    }


    Assignment **Trie::existsSubsetInternal(const tnodePtr &pNode, const std::set<ref<Expr>> &exprs,
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

    Assignment **Trie::existsSupersetInternal(const tnodePtr &pNode, const Key &exprs,
                                              constIterator expr, bool &hasResult) const {

        if (expr == exprs.cend()) {
            hasResult = true;
            return pNode->value;
        }
        Assignment **found = nullptr;

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

    void Trie::findAssignments(const tnodePtr &node, std::vector<Assignment **> &set) const {
        if (node->last)
            set.emplace_back(node->value);
        for (const auto &child : node->children) {
            findAssignments(child.second, set);
        }
    }

    bool Trie::getSupersetsInternal(const tnodePtr &pNode, const Key &exprs,
                                    constIterator expr, std::vector<Assignment **> &assignments) const {

        if (expr == exprs.cend()) {
            findAssignments(pNode, assignments);
            return true;
        }
        bool found = false;
        for (auto &child : pNode->children) {
            if (child.first == expr->get()->hash()) {
                found = getSupersetsInternal(child.second, exprs, ++expr, assignments);
            } else {
                found = getSupersetsInternal(child.second, exprs, expr, assignments);
            }
            if (found) {
                return found;
            }
        }
        return false;
    }

    Assignment **Trie::getSupersetsInternalPredicate(const tnodePtr &pNode, const Key &exprs,
                                                     constIterator expr, Predicate predicate) const {

        if (expr == exprs.cend()) {
            return findAssignment(pNode, predicate);
        }

        Assignment **found = nullptr;
        for (const auto &child : pNode->children) {
            if (expr == exprs.end()) {
                return nullptr;
            }
            if (child.first == expr->get()->hash()) {
                found = getSupersetsInternalPredicate(child.second, exprs, ++expr, predicate);
            } else {
                found = getSupersetsInternalPredicate(child.second, exprs, expr, predicate);
            }
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    Assignment **Trie::getSubsetsInternalPredicate(const tnodePtr &pNode, const Key &exprs, constIterator expr,
                                                   Predicate predicate) const {

        if (pNode->last) {
            if (predicate(*(pNode->value))) {
                return pNode->value;
            }
        }
        if (expr == exprs.cend()) {
            return nullptr;
        }
        auto iter = pNode->children.find((*expr)->hash());
        if (iter != pNode->children.cend()) {
            return getSubsetsInternalPredicate(iter->second, exprs, ++expr, predicate);
        }
        return getSubsetsInternalPredicate(pNode, exprs, ++expr, predicate);
    }

    bool Trie::getSubsetsInternal(const tnodePtr &pNode, const Key &exprs, constIterator expr,
                                  std::vector<Assignment **> &assignments) const {

        if (pNode->last) {
            assignments.emplace_back(pNode->value);
        }
        if (expr == exprs.cend()) {
            return false;
        }
        auto iter = pNode->children.find((*expr)->hash());
        if (iter != pNode->children.cend()) {
            return getSubsetsInternal(iter->second, exprs, ++expr, assignments);
        }
        return getSubsetsInternal(pNode, exprs, ++expr, assignments);
    }

    std::vector<Assignment **> Trie::getSubsets(const Key &exprs) const {
        std::vector<Assignment **> assignments;
        getSubsetsInternal(root, exprs, exprs.cbegin(), assignments);
        return assignments;
    }

    Assignment **Trie::existsSubset(const Key &exprs) const {
        return existsSubsetInternal(root, exprs, exprs.cbegin());
    }

    Assignment **Trie::existsSuperset(const Key &exprs) const {
        bool hasResult = false;
        return existsSupersetInternal(root, exprs, exprs.cbegin(), hasResult);
    }

    std::vector<Assignment **> Trie::getSupersets(const Key &exprs) const {
        std::vector<Assignment **> assignments;
        getSupersetsInternal(root, exprs, exprs.cbegin(), assignments);
        return assignments;
    }

    Assignment **Trie::findFirstSubsetMatching(const Key &exprs, Predicate predicate) const {
        return getSubsetsInternalPredicate(root, exprs, exprs.cbegin(), predicate);
    }

    void Trie::store(CacheTrie::Builder &&builder) const {
        root->storeNode(std::forward<CacheTrieNode::Builder>(builder.initRoot()));
    }

    std::shared_ptr<Trie::TrieNode> Trie::createTrieNode(const CacheTrieNode::Reader &&node) {
        Assignment **pAssignment = nullptr;
        if (node.hasValue()) {
            size++;
            ++klee::stats::pcacheTrieSize;
            pAssignment = new Assignment *;
            *pAssignment = Assignment::deserialize(node.getValue());
        }
        std::unordered_map<unsigned, std::shared_ptr<Trie::TrieNode>> children;
        for (const CacheTrieNode::Child::Reader &child : node.getChildren()) {
            children.insert(std::make_pair(child.getExpr(), createTrieNode(child.getNode())));
        }
        return std::make_shared<Trie::TrieNode>(std::move(children), pAssignment, node.getLast());
    }

    Assignment **Trie::get(Key &key) {
        if (!size) {
            return nullptr;
        }
        return searchInternal(root, key, key.cbegin());
    }

    void Trie::set(Key &key, Assignment **const &value) {
        if (key.empty()) {
            return;
        }
        ++stats::pcacheTrieSize;
        ++size;
        Assignment **pAssignment = new Assignment *;
        *pAssignment = *value;
        insertInternal(root, key, key.cbegin(), pAssignment);
    }

    Assignment **Trie::findFirstSupersetMatching(const Key &key, Predicate predicate) const {
        return getSupersetsInternalPredicate(root, key, key.cbegin(), predicate);
    }

    Assignment **
    Trie::findAssignment(const tnodePtr &node, Predicate predicate) const {
        if (node->last && node->value && predicate(*(node->value))) return node->value;
        for (const auto &child : node->children) {
            return findAssignment(child.second, predicate);
        }
        return nullptr;
    }


    void Trie::TrieNode::storeNode(CacheTrieNode::Builder &&nodeBuilder) const {
        nodeBuilder.setLast(this->last);
        if (value) {
            CacheAssignment::Builder builder = nodeBuilder.initValue();
            if (*value) {
                (*value)->serialize(std::forward<CacheAssignment::Builder>(builder));
            } else {
                builder.setNoBinding(true);
            }
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