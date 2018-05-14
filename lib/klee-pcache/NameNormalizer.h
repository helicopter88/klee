//
// Created by dmarino on 11/04/18.
//

#ifndef KLEE_NAMENORMALIZER_H
#define KLEE_NAMENORMALIZER_H


#include <klee/util/Ref.h>
#include <klee/Expr.h>
#include <klee/util/Assignment.h>

using namespace llvm;
namespace klee {
    typedef std::map<std::string, std::string> NameMapping;

    class NameNormalizer {
    private:
        NameMapping namesMappings;
        NameMapping reverseMappings;
    public:
        std::set<ref<Expr>> normalizeExpressions(const std::set<ref<Expr>> &expressions);

        Assignment *denormalizeAssignment(const Assignment *assignment) const;

        Assignment *normalizeAssignment(const Assignment *assignment) const;

        const NameMapping &getMappings() const {
            return namesMappings;
        };

        const Array *denormalizeArray(const Array *orig) const;

        std::vector<const Array *> denormalizeArrays(const std::vector<const Array *> &arrays) const;

        const Array *normalizeArray(const Array *orig) const;

        std::vector<const Array *> normalizeArrays(const std::vector<const Array *> &arrays) const;

        ref<Expr> normalizeExpression(const ref<Expr> &expr);

        ref<Expr> denormalizeExpression(const ref<Expr> &expr) const;
    };
}
#endif //KLEE_NAMENORMALIZER_H
