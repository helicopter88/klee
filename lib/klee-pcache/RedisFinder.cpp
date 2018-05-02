//
// Created by dmarino on 17/02/18.
//

#include <klee/util/Assignment.h>
#include <klee/SolverStats.h>
#include <capnp/message.h>
#include "RedisFinder.h"

using namespace klee;

ProtoAssignment *pa;
std::string emptyAssignmentStr;
Assignment **ret;

RedisFinder::RedisFinder(const std::string &url, size_t port, int dbNum) : instance(url, port, dbNum) {
    // Empty assignment is always the same, so serialize it only once
    auto *emptyAssignment = new ProtoAssignment;
    emptyAssignment->set_nobinding(true);
    emptyAssignmentStr = emptyAssignment->SerializeAsString();

    pa = new ProtoAssignment;
    ret = new Assignment *;
    stats::pcacheRedisSize += instance.getSize();
}

Assignment **RedisFinder::find(std::set<ref<Expr>> &expressions) {
    const std::string &serialized = serializeToString(expressions);
    std::string res = instance.get(serialized);
    if (res.empty()) {
        return nullptr;
    }
    pa->ParseFromString(res);
    *ret = Assignment::deserialize(*pa);
    return ret;
}

std::string RedisFinder::serializeToString(const std::set<ref<Expr>> &expressions) {
    std::string serialized;

    //// This combination formula is the same as the one used in JDK 1.7
    uint64_t hash = 1;
    for (const ref<Expr> &r : expressions) {
        hash = 31 * hash + r->hash();
    }
    serialized = std::to_string(hash);
    return serialized;
}

std::future<cpp_redis::reply> RedisFinder::future_find(std::set<ref<Expr>> &expressions) {
    const std::string &serialized = serializeToString(expressions);
    return instance.future_get(serialized);
}

Assignment **RedisFinder::processResponse(std::future<cpp_redis::reply> &&reply) const {
    if (!reply.valid()) {
        return nullptr;
    }
    cpp_redis::reply r = reply.get();
    if (r.is_null() || !r.is_string()) {
        return nullptr;
    }
    const std::string &result = r.as_string();
    pa->ParseFromString(result);
    *ret = Assignment::deserialize(*pa);
    return ret;

}

void RedisFinder::insert(std::set<ref<Expr>> &expressions, Assignment *value) {
    const std::string &serialized = serializeToString(expressions);

    if (value) {
        ProtoAssignment *assignment = value->serialize();
        instance.set(serialized, assignment->SerializeAsString());
    } else {
        instance.set(serialized, emptyAssignmentStr);
    }
    ++stats::pcacheRedisSize;
}

void RedisFinder::storeFinder() {
    delete ret;
    delete pa;
}

