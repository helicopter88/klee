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
    explicit RedisInstance(const std::string &url = "127.0.0.1", size_t port = 6379);

    void set(const std::string& key, const std::string& value) final;

    const std::string get(const std::string& key) final;

    void store() final;

    ~RedisInstance();
};


#endif //KLEE_PCACHE_REDISINSTANCE_H
