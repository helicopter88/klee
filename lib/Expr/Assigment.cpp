//===-- Assignment.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include <klee/util/Cache.pb.h>
#include "klee/util/Assignment.h"
#include <klee/util/cache_cap.capnp.h>

namespace klee {
    ProtoAssignment *Assignment::serialize() const {
        auto *protoAssignment = new ProtoAssignment();
        protoAssignment->set_allowfreevalues(this->allowFreeValues);
        if(bindings.empty()) {
            return protoAssignment;
        }
        for (auto b : bindings) {
            if(!b.first) continue;
            auto *pbv = new ProtoBitVector();
            for (unsigned char v : b.second) {
                pbv->mutable_values()->Add(v);
            }
            protoAssignment->mutable_bvs()->AddAllocated(pbv);
            protoAssignment->mutable_objects()->AddAllocated(b.first->serialize());
        }
        return protoAssignment;
    }

    void Assignment::serialize(CacheAssignment::Builder &&builder) const {
        builder.setAllowFreeValues(this->allowFreeValues);
        if(bindings.empty()) {
            builder.setNoBinding(true);
            return;
        }
        unsigned i = 0;
        auto bins = builder.initBindings(bindings.size());
        for(auto b : bindings) {
            b.first->serialize(bins[i].getObjects());
            auto bvs = bins[i].initBvs(b.second.size());
            for(unsigned j = 0; j < b.second.size(); j++) {
                bvs.set(j, b.second[j]);
            }
            i++;
        }
    }
    void Assignment::dump() const {
        if (bindings.empty()) {
            llvm::errs() << "No bindings\n";
            return;
        }
        for (const auto &binding : bindings) {
            llvm::errs() << binding.first->name << "\n[";
            for (unsigned char j : binding.second)
                llvm::errs() << (int) j << ",";
            llvm::errs() << "]\n";
        }
    }

    void Assignment::createConstraintsFromAssignment(
            std::vector<ref<Expr> > &out) const {
        assert(out.size() == 0 && "out should be empty");
        for (const auto &binding : bindings) {
            const Array *array = binding.first;
            const std::vector<unsigned char> &values = binding.second;
            for (unsigned arrayIndex = 0; arrayIndex < array->size; ++arrayIndex) {
                unsigned char value = values[arrayIndex];
                out.push_back(EqExpr::create(
                        ReadExpr::create(UpdateList(array, 0),
                                         ConstantExpr::alloc(arrayIndex, array->getDomain())),
                        ConstantExpr::alloc(value, array->getRange())));
            }
        }
    }

    Assignment *Assignment::deserialize(const ProtoAssignment &pa) {
        if(pa.nobinding()) {
            return nullptr;
        }
        std::vector<const Array *> objects;
        std::vector<std::vector<unsigned char>> values;
        for (const auto& pbv : pa.bvs()) {
            std::vector<unsigned char> tmp;
            for (const uint32_t value : pbv.values()) {
                tmp.push_back((unsigned char) value);
            }
            values.push_back(tmp);
        }
        for (const auto &obj : pa.objects()) {
            objects.emplace_back(Array::deserialize(obj));
        }
        return new Assignment(objects, values, pa.allowfreevalues());
    }
    Assignment* Assignment::deserialize(const CacheAssignment::Reader &&reader) {
        if(reader.getNoBinding())
            return nullptr;

        std::vector<const Array *> objects;
        std::vector<std::vector<unsigned char>> values;
        for(const auto& binding : reader.getBindings()) {
            std::vector<unsigned char> tmp;
            for(const uint8_t t : binding.getBvs()) {
                tmp.push_back(t);
            }
            values.push_back(tmp);
            objects.emplace_back(Array::deserialize(binding.getObjects()));
        }
        return new Assignment(objects, values, reader.getAllowFreeValues());
    }
}
