//===-- Assignment.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "klee/util/Assignment.h"
#include "klee/util/Cache.pb.h"

namespace klee {
    ProtoAssignment *Assignment::serialize() const {
        ProtoAssignment *protoAssignment = new ProtoAssignment();
        protoAssignment->set_allowfreevalues(this->allowFreeValues);
        for (const auto b : bindings) {
            protoAssignment->add_values((char *) b.second.data());
            protoAssignment->mutable_objects()->AddAllocated(b.first->serialize());
        }
        return protoAssignment;
    }

    void Assignment::dump() {
        if (bindings.size() == 0) {
            llvm::errs() << "No bindings\n";
            return;
        }
        for (bindings_ty::iterator i = bindings.begin(), e = bindings.end(); i != e;
             ++i) {
            llvm::errs() << (*i).first->name << "\n[";
            for (int j = 0, k = (*i).second.size(); j < k; ++j)
                llvm::errs() << (int) (*i).second[j] << ",";
            llvm::errs() << "]\n";
        }
    }

    void Assignment::createConstraintsFromAssignment(
            std::vector<ref<Expr> > &out) const {
        assert(out.size() == 0 && "out should be empty");
        for (bindings_ty::const_iterator it = bindings.begin(), ie = bindings.end();
             it != ie; ++it) {
            const Array *array = it->first;
            const std::vector<unsigned char> &values = it->second;
            for (unsigned arrayIndex = 0; arrayIndex < array->size; ++arrayIndex) {
                unsigned char value = values[arrayIndex];
                out.push_back(EqExpr::create(
                        ReadExpr::create(UpdateList(array, 0),
                                         ConstantExpr::alloc(arrayIndex, array->getDomain())),
                        ConstantExpr::alloc(value, array->getRange())));
            }
        }
    }

    Assignment* Assignment::deserialize(const ProtoAssignment &pa) {
        std::vector<const Array *> objects;
        std::vector<std::vector<unsigned char>> values;
        for (const std::string &value : pa.values()) {
            std::vector<unsigned char> v(value.c_str(), value.c_str() + value.size());
            values.emplace_back(v);
        }
        for (const auto &obj : pa.objects()) {
            objects.emplace_back(Array::deserialize(obj));
        }
        return new Assignment(objects, values, pa.allowfreevalues());
    }
}
