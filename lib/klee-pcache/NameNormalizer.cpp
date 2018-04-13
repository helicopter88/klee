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
            newArray = ac.CreateArray(newName, orig->size, &orig->constantValues[0],
                                      &orig->constantValues[0] + orig->constantValues.size(), orig->domain,
                                      orig->range);
        }
        return newArray;
    }

    class RenamerVisitor : public ExprVisitor {
    private:
        const std::map<std::string, std::string> &namesMapping;

        UpdateNode *newNode(const UpdateNode *old) {
            UpdateNode *next = nullptr;
            if (old->next) {
                next = newNode(old->next);
            }
            return new UpdateNode(next, visit(old->index), visit(old->value));
        }

    public:
        explicit RenamerVisitor(const std::map<std::string, std::string> &_namesMapping) : namesMapping(
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


    std::set<ref<Expr>>
    NameNormalizer::normalizeExpressions(const std::set<ref<Expr>> &exprs) {
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
            namesMappings.insert(std::make_pair(name, "___" + std::to_string(number)));
            reverseMappings.insert(std::make_pair("___" + std::to_string(number), name));
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
            const Array *orig = bin.first;
            const auto &iterator = namesMappings.find(orig->getName());
            if (iterator == namesMappings.cend()) {
                assert("Name was not in the mapping" && false);
            }
            const std::string &newName = iterator->second;
            const Array *arr = newArray(orig, newName);
            arrays.push_back(arr);
            bvs.push_back(bin.second);
        }
        return new Assignment(arrays, bvs);
    }

    Assignment *NameNormalizer::denormalizeAssignment(const Assignment *assignment) const {
        if (!assignment) {
            return nullptr;
        }
        std::vector<const Array *> arrays;
        std::vector<std::vector<unsigned char>> bvs;
        for (const auto &bin : assignment->bindings) {
            const Array *orig = bin.first;
            const auto &iterator = reverseMappings.find(orig->getName());
            if (iterator == reverseMappings.cend()) {
                assert("Name was not in the mapping" && false);
            }
            const std::string &newName = iterator->second;
            const Array *arr = newArray(orig, newName);
            arrays.push_back(arr);
            bvs.push_back(bin.second);
        }
        return new Assignment(arrays, bvs);
    }
}