//
// Created by dmarino on 17/02/18.
//

#include <klee/util/Assignment.h>
#include <klee/TimerStatIncrementer.h>
#include <klee/SolverStats.h>
#include "ExactMatchFinder.h"

using namespace klee;

Assignment **ExactMatchFinder::find(std::set<ref<Expr>> &expressions) {
    TimerStatIncrementer t(stats::pcacheLookupTime);
    std::string serialized;
    const auto thing = nameCache.find(expressions);
    if(thing != nameCache.cend()) {
        serialized = thing->second;
    } else {
        ProtoCacheElem protoCacheElem;
        for (const ref<Expr> &r : expressions) {
            protoCacheElem.mutable_key()->AddAllocated(r->serialize());
        }
        serialized = protoCacheElem.SerializeAsString();
        nameCache.insert(std::make_pair(expressions, serialized));
    }
    std::string res = instance.get(serialized);
    if (res.empty()) {
        return nullptr;
    }
    ProtoAssignment pa;
    pa.ParseFromString(res);
    Assignment **ret = new Assignment *();
    *ret = Assignment::deserialize(pa);
    return ret;
}

void ExactMatchFinder::insert(std::set<ref<Expr>> &expressions, Assignment *value) {
    TimerStatIncrementer t(stats::deserializationTime);
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
    const std::string& serialized = protoCacheElem.SerializeAsString();
    nameCache.insert(std::make_pair(expressions, serialized));
    instance.set(serialized, assignment->SerializeAsString());
}

void ExactMatchFinder::close() {}