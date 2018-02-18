//
// Created by dmarino on 18/02/18.
//

#ifndef KLEE_STORAGE_H
#define KLEE_STORAGE_H


#include <cpp_redis/core/client.hpp>

template <typename K, typename V> class Storage {
    virtual const V get(const K&& key) = 0;
    virtual void set(const K&& key, const V&& value) = 0;
    virtual void store() =0;
};


#endif //KLEE_STORAGE_H
