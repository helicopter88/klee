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
    class NameNormalizer {
    private:
        std::map<std::string, std::string> namesMappings;
        std::map<std::string, std::string> reverseMappings;
    public:
        std::set<ref<Expr>> normalizeExpressions(const std::set<ref<Expr>> &expressions);

        Assignment *denormalizeAssignment(const Assignment *assignment) const;

        Assignment *normalizeAssignment(const Assignment *assignment) const;

        const std::map<std::string, std::string> &getMappings() const {
            return namesMappings;
        };

    };
}
#endif //KLEE_NAMENORMALIZER_H
