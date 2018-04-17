//
// Created by dmarino on 17/02/18.
//

#include <klee/util/Assignment.h>
#include <klee/SolverStats.h>
#include <capnp/message.h>
#include "RedisFinder.h"

using namespace klee;

ProtoAssignment* pa;
ProtoAssignment * emptyAssignment;

RedisFinder::ExactMatchFinder(const std::string& url, size_t port, int dbNum) : instance(url, port, dbNum) {
    emptyAssignment = new ProtoAssignment;
    emptyAssignment->set_nobinding(true);
    pa = new ProtoAssignment;
    stats::pcacheRedisSize += instance.getSize();
}
Assignment **RedisFinder::find(std::set<ref<Expr>> &expressions) {
    const std::string& serialized = serializeToString(expressions);
    std::string res = instance.get(serialized);
    if (res.empty()) {
        return nullptr;
    }
    pa->ParseFromString(res);
    Assignment **ret = new Assignment *();
    *ret = Assignment::deserialize(*pa);
    return ret;
}

std::string RedisFinder::serializeToString(const std::set<ref<Expr>> &expressions) {
    std::string serialized;
    const auto& thing = this->nameCache.find(expressions);
    if(thing != this->nameCache.cend()) {
        serialized = thing->second;
    } else {
        CacheExprList::Builder builder = lookupMessageBuilder.initRoot<CacheExprList>();
        auto list = builder.initKey(expressions.size());
        unsigned i = 0;
        for (const ref<Expr> &r : expressions) {
            r->serialize(list[i++]);
        }
        assert(lookupMessageBuilder.getSegmentsForOutput().size());
        const kj::ArrayPtr<const unsigned char> chrs = lookupMessageBuilder.getSegmentsForOutput().asBytes();
        std::string str(chrs.begin(), chrs.end());
        serialized = str;//builder.toString().flatten().cStr();//protoCacheElem.SerializeAsString();
        this->nameCache.insert(std::make_pair(expressions, serialized));
        builder.disownKey();
    }
    return serialized;
}

std::future<cpp_redis::reply> RedisFinder::future_find(std::set<ref<Expr>>& expressions) {
    const std::string& serialized = serializeToString(expressions);
    return instance.future_get(serialized);
}

Assignment** RedisFinder::processResponse(std::future<cpp_redis::reply>&& reply) const {
    if(!reply.valid()) {
        return nullptr;
    }
    cpp_redis::reply r = reply.get();
    if(r.is_null() || !r.is_string()) {
        return nullptr;
    }
    const std::string &result = r.as_string();
    pa->ParseFromString(result);
    Assignment **ret = new Assignment *();
    *ret = Assignment::deserialize(*pa);
    return ret;

}
void RedisFinder::insert(std::set<ref<Expr>> &expressions, Assignment *value) {
    const std::string& serialized = serializeToString(expressions);

    ProtoAssignment *assignment;
    if (value) {
        assignment = value->serialize();
    } else {
        assignment = emptyAssignment;
    }
    nameCache.insert(std::make_pair(expressions, serialized));
    instance.set(serialized, assignment->SerializeAsString());
    ++stats::pcacheRedisSize;
}

void RedisFinder::storeFinder() {}

