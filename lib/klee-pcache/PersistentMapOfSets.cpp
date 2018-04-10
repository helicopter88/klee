//
// Created by dmarino on 18/02/18.
//

#include <fstream>
#include <klee/util/Cache.pb.h>
#include <llvm/Support/Path.h>
#include <capnp/message.h>
#include "PersistentMapOfSets.h"
#include <capnp/serialize.h>
#include <capnp/serialize-packed.h>
#include <fcntl.h>
#include <unistd.h>
#include <klee/SolverStats.h>

using namespace llvm;

namespace klee {

    struct NullAssignment {
        bool operator()(Assignment *a) const { return !a; }
    };

    struct NonNullAssignment {
        bool operator()(Assignment *a) const { return a != 0; }
    };

    struct NullOrSatisfyingAssignment {
        std::set<ref<Expr>> &key;

        explicit NullOrSatisfyingAssignment(std::set<ref<Expr>> &_key) : key(_key) {}

        bool operator()(Assignment *a) const {
            return !a || a->satisfies(key.begin(), key.end());
        }
    };

    PersistentMapOfSets::PersistentMapOfSets(const std::string &_path) : path(_path) {
        for (int i = 0; i <= INT_MAX; i++) {
            SmallString<128> s(_path);
            llvm::sys::path::append(s, "cache" + std::to_string(i) + ".bin");
            int fd = open(s.c_str(), O_RDWR);
            if (fd < 0) {
                if (errno != ENOENT)
                    errs() << "Unable to load: " << s.c_str() << " - reason: " << strerror(errno);
                break;
            }
            ::capnp::ReaderOptions readerOptions;
            readerOptions.nestingLimit = 256;
            readerOptions.traversalLimitInWords = INT_MAX >> 8;
            ::capnp::PackedFdMessageReader *pfd = new capnp::PackedFdMessageReader(fd, readerOptions);
            CapCache::Reader cacheReader = pfd->getRoot<CapCache>();
            for (const auto &elem : cacheReader.getElems()) {
                const auto &exprs = elem.getKey().getKey();
                std::set<ref<Expr>> expressions;
                for (auto expr : exprs) {
                    expressions.insert(
                            Expr::deserialize(std::forward<CacheExpr::Reader>(expr))
                    );
                }
                Assignment *a = nullptr;
                if (elem.hasAssignment()) {
                    a = Assignment::deserialize(elem.getAssignment());
                }
                set(expressions, &a);
            }
            close(fd);
        }
    }

    Assignment **PersistentMapOfSets::get(std::set<ref<Expr>> &key) {
        Assignment **value = cache.lookup(key);
        return value;
    }

    Assignment **PersistentMapOfSets::tryAll_get(std::set<ref<Expr>> &key) {
        Assignment **value = cache.findSubset(key, NullAssignment());
        if (!value) {
            // If a superset is satisfiable then this subset must be satisfiable
            value = cache.findSuperset(key, NonNullAssignment());
        }
        if (!value)
            value = cache.findSubset(key, NullOrSatisfyingAssignment(key));

        return value;
    }

    void PersistentMapOfSets::set(std::set<ref<Expr>> &key, Assignment **const &value) {
        ++stats::pcachePMapSize;
        cache.insert(key, *value);
    }

    const char *nextFile(SmallString<128> &p, int &i, const std::string &thing) {
        llvm::sys::path::remove_filename(p);
        llvm::sys::path::append(p, thing + std::to_string(++i) + ".bin");
        std::remove(p.c_str());
        return p.c_str();
    }

    void PersistentMapOfSets::store() {
        int i = -1;
        unsigned iter = 0;
        SmallString<128> p(path);
        llvm::sys::fs::create_directory(p.c_str());
        p.append("/placeholder");
        ::capnp::MallocMessageBuilder messageBuilder;
        CapCache::Builder cacheBuilder = messageBuilder.initRoot<CapCache>();
        auto capCache = cacheBuilder.initElems(stats::pcachePMapSize);
        for (const auto &c : cache) {
            auto keyList = capCache[iter].initKey().initKey(c.first.size());
            unsigned j = 0;
            for (const auto &expr : c.first) {
                expr->serialize(keyList[j++]);
            }

            auto ass = capCache[iter].initAssignment();
            if (!c.second) {
                ass.setNoBinding(true);

            } else {
                c.second->serialize(std::forward<CacheAssignment::Builder>(ass));
            }
            iter++;
        }
        int fd = open(nextFile(p, i, "cache"), O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if(fd < 0) {
            errs() << "Could not store to file: " << strerror(errno) << "\n";
            return;
        }
        ::capnp::writePackedMessageToFd(fd, messageBuilder);
        close(fd);
    }
}