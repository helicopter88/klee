//
// Created by dmarino on 18/02/18.
//

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <capnp/serialize-packed.h>

#include <fcntl.h>
#include <fstream>
#include <unistd.h>

#include <klee/util/Cache.pb.h>
#include <klee/SolverStats.h>
#include <llvm/Support/Path.h>

#include "PersistentMapOfSets.h"
#include "Predicates.h"

using namespace llvm;

static const constexpr uint64_t MAX_SIZE = UINT64_MAX >> 10;
namespace klee {
    /** Transforms a set of expresions into a set of hashes **/
#define TO_HASHES(key) std::set<unsigned> hashes; \
                       std::transform((key).cbegin(), (key).cend(), std::inserter(hashes, hashes.end()), \
                       [](const ref<Expr> &expr) -> unsigned { return expr->hash(); });

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
            readerOptions.nestingLimit = 10240;
            readerOptions.traversalLimitInWords = MAX_SIZE << 1;
            ::capnp::PackedFdMessageReader *pfd = new capnp::PackedFdMessageReader(fd, readerOptions);
            CapCache::Reader cacheReader = pfd->getRoot<CapCache>();
            for (const CapCache::Elem::Reader &elem : cacheReader.getElems()) {
                const auto &exprs = elem.getKey().getKey();
                std::set<unsigned> expressions;
                for (auto expr : exprs) {
                    expressions.insert(expr);
                }
                Assignment *a = nullptr;
                if (elem.hasAssignment()) {
                    a = Assignment::deserialize(elem.getAssignment());
                }
                this->cache.insert(expressions, a);
                ++stats::pcachePMapSize;
            }
            close(fd);
            delete pfd;
        }
    }

    Assignment **PersistentMapOfSets::get(std::set<ref<Expr>> &key) {
        if(key.empty()) return nullptr;
        TO_HASHES(key);
        Assignment **value = cache.lookup(hashes);
        return value;
    }

    Assignment **PersistentMapOfSets::tryAll_get(std::set<ref<Expr>> &key) {
        if(key.empty()) return nullptr;
        TO_HASHES(key);
        Assignment **value = cache.findSubset(hashes, NullAssignment());
        if (!value) {
            // If a superset is satisfiable then this subset must be satisfiable
            value = cache.findSuperset(hashes, NonNullAssignment());
        }
        if (!value)
            value = cache.findSubset(hashes, NullOrSatisfyingAssignment(key));

        return value;
    }

    void PersistentMapOfSets::set(std::set<ref<Expr>> &key, Assignment **const &value) {
        if(key.empty()) return;
        TO_HASHES(key);
        ++stats::pcachePMapSize;
        cache.insert(hashes, *value);
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
            CacheExprList::Builder keyList = capCache[iter].initKey();
            capnp::List<uint32_t>::Builder k = keyList.initKey(c.first.size());
            unsigned j = 0;
            for (const unsigned expr : c.first) {
                k.set(j++, expr);

            }

            auto ass = capCache[iter].initAssignment();
            if (!c.second) {
                ass.setNoBinding(true);

            } else {
                c.second->serialize(std::forward<CacheAssignment::Builder>(ass));
            }
            iter++;
        }
        int fd = open(nextFile(p, i, "cache"), O_TRUNC | O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd < 0) {
            errs() << "Could not store to file: " << strerror(errno) << "\n";
            return;
        }
        ::capnp::writePackedMessageToFd(fd, messageBuilder);
        close(fd);
    }
}