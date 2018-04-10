//
// Created by dmarino on 17/02/18.
//

#include <klee/util/Assignment.h>
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

std::string ExactMatchFinder::serializeToString(const std::set<ref<Expr>> &expressions) {
    std::string serialized;
    const auto thing = this->nameCache.find(expressions);
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

std::future<cpp_redis::reply> ExactMatchFinder::future_find(std::set<ref<Expr>>& expressions) {
    const std::string& serialized = serializeToString(expressions);
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
    const std::string &result = r.as_string();
    //kj::ArrayPtr<const char> chars(result.c_str(), result.size());
    //::capnp::word(0);
    //::capnp::FlatArrayMessageReader flatArrayMessageReader(chars.asBytes());
    pa->ParseFromString(result);
    Assignment **ret = new Assignment *();
    *ret = Assignment::deserialize(*pa);
    return ret;

}
void ExactMatchFinder::insert(std::set<ref<Expr>> &expressions, Assignment *value) {
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

void ExactMatchFinder::close() {}

