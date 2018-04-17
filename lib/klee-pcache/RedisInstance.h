//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_REDISINSTANCE_H
#define KLEE_PCACHE_REDISINSTANCE_H


#include <cpp_redis/core/client.hpp>
#include "Storage.h"

class RedisInstance : Storage<const std::string, const std::string>{
private:
    cpp_redis::client client;
public:
    explicit RedisInstance(const std::string &url, size_t port, int dbNum);

    void set(const std::string& key, const std::string& value) final;

    const std::string get(const std::string& key) final;
    std::future<cpp_redis::reply> future_get(const std::string& key);
    void store() final;

    ~RedisInstance();

    int64_t getSize();
};


#endif //KLEE_PCACHE_REDISINSTANCE_H
