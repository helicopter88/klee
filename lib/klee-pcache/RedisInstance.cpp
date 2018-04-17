//
// Created by dmarino on 17/02/18.
//

#include "RedisInstance.h"

RedisInstance::RedisInstance(const std::string &url, size_t port, int dbNum) {
    client.connect(url, port,
                   [](const std::string &host, std::size_t port, cpp_redis::client::connect_state status) {
#ifdef DEBUG
                       if (status == cpp_redis::client::connect_state::dropped) {
                           std::cout << "Goodbye!" << std::endl;
                       }
#endif
                   });
    client.select(dbNum, [](const cpp_redis::reply& reply) {});
}

int64_t RedisInstance::getSize() {
    int64_t size = 0;
    client.dbsize([&](const cpp_redis::reply &reply) {
        size = reply.as_integer();
    });
    return size;
}

std::future<cpp_redis::reply> RedisInstance::future_get(const std::string &key) {
    std::future<cpp_redis::reply> fut = client.get(key);
    client.commit();
    return fut;
}

const std::string RedisInstance::get(const std::string &key) {
    std::string res;
    client.get(key, [&](const cpp_redis::reply &reply) {
        if (!reply.is_null()) {
            res = reply.as_string();
        }
    });
    client.sync_commit();
    return res;
}

void RedisInstance::set(const std::string &key, const std::string &value) {
    client.set(key, value, [](const cpp_redis::reply &r) {});
    client.commit();
}

void RedisInstance::store() {
    client.sync_commit();
    client.save();
    client.disconnect();
}

RedisInstance::~RedisInstance() {
    store();
}


