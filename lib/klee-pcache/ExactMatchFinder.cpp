//
// Created by dmarino on 17/02/18.
//

#include <klee/util/Assignment.h>
#include <klee/TimerStatIncrementer.h>
#include <klee/SolverStats.h>
#include <capnp/message.h>
#include "ExactMatchFinder.h"

using namespace klee;

ProtoAssignment* pa;
ProtoAssignment * emptyAssignment;

ExactMatchFinder::ExactMatchFinder() {
    emptyAssignment = new ProtoAssignment;
    emptyAssignment->set_nobinding(true);
    pa = new ProtoAssignment;
}
Assignment **ExactMatchFinder::find(std::set<ref<Expr>> &expressions) {
    TimerStatIncrementer t(stats::pcacheLookupTime);
    std::string serialized = serializeToString(expressions);
    std::string res = instance.get(serialized);
    if (res.empty()) {
        return nullptr;
    }
    pa->ParseFromString(res);
    Assignment **ret = new Assignment *();
    *ret = Assignment::deserialize(*pa);
    return ret;
}

std::string ExactMatchFinder::serializeToString(std::set<ref<Expr>> &expressions) {
    std::string serialized;
    const auto thing = this->nameCache.find(expressions);
    if(thing != this->nameCache.cend()) {
        serialized = thing->second;
    } else {
        CacheExprList::Builder builder = this->mmb.initRoot<CacheExprList>();
        auto list = builder.initKey(expressions.size());
        int i = 0;
        ProtoCacheElem protoCacheElem;
        for (const ref<Expr> &r : expressions) {
            r->serialize(list[i++]);
        }
        serialized = builder.toString().flatten().cStr();//protoCacheElem.SerializeAsString();
        this->nameCache.insert(std::make_pair(expressions, serialized));
    }
    return serialized;
}

std::future<cpp_redis::reply> ExactMatchFinder::future_find(std::set<ref<Expr>>& expressions) {
    std::string serialized = serializeToString(expressions);
    return instance.future_get(serialized);
}

Assignment** ExactMatchFinder::processResponse(std::future<cpp_redis::reply>&& reply) const {
    if(!reply.valid()) {
        return nullptr;
    }
    cpp_redis::reply r = reply.get();
    if(r.is_null() || !r.is_string()) {
        return nullptr;
    }
    std::string result = r.as_string();
    pa->ParseFromString(result);
    Assignment **ret = new Assignment *();
    *ret = Assignment::deserialize(*pa);
    return ret;

}
void ExactMatchFinder::insert(std::set<ref<Expr>> &expressions, Assignment *value) {
    TimerStatIncrementer t(stats::deserializationTime);
    CacheExprList::Builder builder = mmb.initRoot<CacheExprList>();
    auto list = builder.initKey(expressions.size());
    int i = 0;
    for (const ref<Expr> &r : expressions) {
        r->serialize(list[i++]);
    }
    const std::string& serialized = builder.toString().flatten().cStr();

    ProtoAssignment *assignment;
    if (value) {
        assignment = value->serialize();
    } else {
        assignment = emptyAssignment;
    }
    nameCache.insert(std::make_pair(expressions, serialized));
    instance.set(serialized, assignment->SerializeAsString());
}

void ExactMatchFinder::close() {}

