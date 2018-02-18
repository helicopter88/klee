//
// Created by dmarino on 17/02/18.
//

#ifndef KLEE_PCACHE_REDISINSTANCE_H
#define KLEE_PCACHE_REDISINSTANCE_H


#include <cpp_redis/core/client.hpp>
#include "Storage.h"

class RedisInstance : public Storage<std::string, std::string> {
private:
    cpp_redis::client client;
public:
    explicit RedisInstance(const std::string &url = "127.0.0.1", size_t port = 6379);

    void set(const std::string&& key, const std::string&& value) override;

    const std::string get(const std::string&& key) override;

    void store() override;

    ~RedisInstance();
};


#endif //KLEE_PCACHE_REDISINSTANCE_H
