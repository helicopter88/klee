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
    ProtoAssignment* Assignment::serialize() const {
      auto* protoAssignment = new ProtoAssignment();
      protoAssignment->set_allowfreevalues(allowFreeValues);
      for(const auto& binding : bindings) {
        auto* protoBinding = new ProtoBinding();
        auto* protoArray = binding.first->serialize();
        auto* protoBitVector = new ProtoBitVector();
        std::for_each(binding.second.begin(), binding.second.end(), [&](const char value) {
            protoBitVector->add_value((uint32_t)value);
        });
        protoBinding->set_allocated_key(protoArray);
        protoBinding->set_allocated_value(protoBitVector);
        protoAssignment->mutable_binding()->AddAllocated(protoBinding);
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
      llvm::errs() << (int)(*i).second[j] << ",";
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
}
