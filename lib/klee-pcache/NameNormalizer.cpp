//
// Created by dmarino on 11/04/18.
//

#include <klee/util/ExprVisitor.h>
#include <klee/util/ArrayCache.h>
#include "NameNormalizer.h"
#include "klee/util/ExprUtil.h"

using namespace llvm;
namespace klee {
    ArrayCache ac;

    static const Array *newArray(const Array *orig, const std::string &newName) {
        const Array *newArray;
        if (orig->constantValues.empty()) {
            newArray = ac.CreateArray(newName, orig->size, nullptr, nullptr, orig->domain, orig->range);
        } else {
            newArray = ac.CreateArray(newName,
                                      orig->size,
                                      &orig->constantValues[0],
                                      &orig->constantValues[0] + orig->constantValues.size(),
                                      orig->domain,
                                      orig->range);
        }
        return newArray;
    }

    class RenamerVisitor : public ExprVisitor {
    private:
        const std::unordered_map<std::string, std::string> &namesMapping;

        UpdateNode *newNode(const UpdateNode *old) {
            UpdateNode *next = nullptr;
            if (old->next) {
                next = newNode(old->next);
            }
            return new UpdateNode(next, visit(old->index), visit(old->value));
        }

    public:
        explicit RenamerVisitor(const std::unordered_map<std::string, std::string> &_namesMapping) : namesMapping(
                _namesMapping) {};


        ExprVisitor::Action visitRead(const ReadExpr &rExpr) override {
            const Array *orig = rExpr.updates.root;
            const auto &iterator = namesMapping.find(orig->getName());
            if (iterator == namesMapping.cend()) {
                assert("Name was not in the mapping" && false);
            }
            const std::string &newName = iterator->second;
            const Array *newArr;
            newArr = newArray(orig, newName);
            UpdateNode *un = nullptr;
            if (rExpr.updates.head)
                un = newNode(rExpr.updates.head);
            UpdateList updateList(newArr, un);

            return Action::changeTo(ReadExpr::create(updateList, visit(rExpr.index)));
        }

    };


    ref<Expr> NameNormalizer::normalizeExpression(const ref<Expr> &expr) {
        std::vector<ref<ReadExpr>> readExprs;
        std::set<std::string> names;

        findReads(expr, true, readExprs);
        for (const ref<ReadExpr> &rExpr: readExprs) {
            names.insert(rExpr->updates.root->getName());
        }
        unsigned i = namesMappings.size();
        for (const std::string &name : names) {
            if (namesMappings.find(name) == namesMappings.cend()) {
                const auto number = i++;
                const std::string &normalized = "VAR" + std::to_string(number);
                namesMappings.insert(std::make_pair(name, normalized));
                reverseMappings.insert(std::make_pair(normalized, name));
            }

        }
        RenamerVisitor rv(namesMappings);
        return rv.visit(expr);
    }

    std::set<ref<Expr>> NameNormalizer::normalizeExpressions(const std::set<ref<Expr>> &exprs) {
        std::set<std::string> names;
        std::vector<ref<ReadExpr>> readExprs;
        std::set<ref<Expr>> result;
        for (const ref<Expr> &expr : exprs) {
            findReads(expr, true, readExprs);
        }
        for (const ref<ReadExpr> &rExpr: readExprs) {
            names.insert(rExpr->updates.root->getName());
        }
        unsigned i = 0;
        for (const std::string &name : names) {
            const auto number = i++;
            namesMappings.insert(std::make_pair(name, "VAR" + std::to_string(number)));
            reverseMappings.insert(std::make_pair("VAR" + std::to_string(number), name));
        }
        RenamerVisitor rv(namesMappings);
        for (const ref<Expr> &expr : exprs) {
            result.insert(rv.visit(expr));
        }
        return result;
    }

    Assignment *NameNormalizer::normalizeAssignment(const klee::Assignment *assignment) const {
        if (!assignment) {
            return nullptr;
        }
        std::vector<const Array *> arrays;
        std::vector<std::vector<unsigned char>> bvs;
        for (const auto &bin : assignment->bindings) {
            arrays.push_back(normalizeArray(bin.first));
            bvs.push_back(bin.second);
        }
        return new Assignment(arrays, bvs);
    }

    const Array *NameNormalizer::normalizeArray(const Array *orig) const {
        if (!orig) {
            return nullptr;
        }
        const auto &iterator = namesMappings.find(orig->getName());
        if (iterator == namesMappings.cend()) {
#ifndef NDEBUG
            for (const auto &b : namesMappings) {
                std::cout << b.first << "->" << b.second << std::endl;
            }
#endif
            assert("Name was not in the mapping" && false);
        }
        return newArray(orig, iterator->second);
    }


    Assignment *NameNormalizer::denormalizeAssignment(const Assignment *assignment) const {
        if (!assignment) {
            return nullptr;
        }
        std::vector<const Array *> arrays;
        std::vector<std::vector<unsigned char>> bvs;
        for (const auto &bin : assignment->bindings) {
            arrays.push_back(denormalizeArray(bin.first));
            bvs.push_back(bin.second);
        }
        return new Assignment(arrays, bvs);
    }

    const Array *NameNormalizer::denormalizeArray(const Array *orig) const {
        const auto &iterator = reverseMappings.find(orig->getName());
        if (iterator == reverseMappings.cend()) {
#ifndef NDEBUG
            for (const auto &b : namesMappings) {
                llvm::errs() << b.first << "->" << b.second << "\n";
            }
            for (const auto &b : reverseMappings) {
                llvm::errs() << b.first << "->" << b.second << "\n";
            }
            llvm::errs() << "Could not find: " << orig->getName() << "\n";
#endif
            return orig;
        }
        return newArray(orig, iterator->second);
    }

    std::vector<const Array *> NameNormalizer::normalizeArrays(const std::vector<const Array *> &arrays) const {
        std::vector<const Array *> out;
        for (const Array *arr : arrays) { out.emplace_back(normalizeArray(arr)); }
        return out;
    }

    std::vector<const Array *> NameNormalizer::denormalizeArrays(const std::vector<const Array *> &arrays) const {
        std::vector<const Array *> out(arrays.size());
        for (const Array *arr : arrays) { out.emplace_back(denormalizeArray(arr)); }
        return out;
    }
}