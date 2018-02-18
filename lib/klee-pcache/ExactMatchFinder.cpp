//
// Created by dmarino on 17/02/18.
//

#include <klee/util/Assignment.h>
#include <klee/TimerStatIncrementer.h>
#include <klee/SolverStats.h>
#include "ExactMatchFinder.h"

using namespace klee;

ProtoAssignment *ExactMatchFinder::find(const std::set<ref<Expr>>& expressions) {
    TimerStatIncrementer t(stats::deserializationTime);
    ProtoCacheElem protoCacheElem;
    for (const ref<Expr> &r : expressions) {
        protoCacheElem.mutable_key()->AddAllocated(r->serialize());
    }
    std::string res = instance.get(protoCacheElem.SerializeAsString());
    if (res.empty()) {
        return nullptr;
    }
    auto *pa = new ProtoAssignment();
    pa->ParseFromString(res);
    return pa;
}

void ExactMatchFinder::insert(const std::set<ref<Expr>>& expressions, const Assignment *value){
    TimerStatIncrementer t(stats::serializationTime);
    ProtoCacheElem protoCacheElem;
    for (const ref<Expr> &r : expressions) {
        protoCacheElem.mutable_key()->AddAllocated(r->serialize());
    }
    ProtoAssignment *assignment;
    if (value) {
        assignment = value->serialize();
    } else {
        assignment = new ProtoAssignment();
        assignment->set_nobinding(true);
    }
    instance.set(protoCacheElem.SerializeAsString(), assignment->SerializeAsString());
}
