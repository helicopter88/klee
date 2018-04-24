//
// Created by dmarino on 12/04/18.
//

#include <klee/util/Assignment.h>
#include <capnp/message.h>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>
#include <capnp/serialize-packed.h>
#include <klee/SolverStats.h>
#include "TrieFinder.h"
#include "NameNormalizer.h"
#include "Predicates.h"

namespace klee {

    Assignment **TrieFinder::find(std::set<ref<Expr>> &exprs) {
        if (exprs.empty()) {
            auto **nAss = new Assignment *;
            *nAss = new Assignment;
            return nAss;
        }
        Assignment **ret = trie.search(exprs);
        if (ret) {
            return ret;
        }
        return nullptr;
    }

    Assignment **TrieFinder::findSpecial(std::set<ref<Expr>> &exprs) {
        if (exprs.empty()) {
            auto **nAss = new Assignment *;
            *nAss = new Assignment;
            return nAss;
        };
        Assignment **ret = trie.existsSubset(exprs);
        if (ret) {
            NullOrNotSatisfyingAssignment s(exprs);
            if (s(*ret)) {
                return ret;
            }
        }

        ret = trie.existsSuperset(exprs);
        if(ret) {
            SatisfyingAssignment s;
            if (s(*ret)) {
                return ret;
            }
        }
        return nullptr;
    }

    void TrieFinder::insert(std::set<ref<Expr>> &exprs, Assignment *assignment) {
        ++stats::pcacheTrieSize;
        trie.insert(exprs, assignment);
    }

    void TrieFinder::storeFinder() {
        llvm::sys::fs::create_directories(path);
        capnp::MallocMessageBuilder messageBuilder;
        CacheTrie::Builder builder = messageBuilder.initRoot<CacheTrie>();
        trie.store(std::forward<CacheTrie::Builder>(builder));
        std::string cPath = path + "/tree_cache.bin";
        int fd = open(cPath.c_str(), O_TRUNC | O_RDWR | O_CREAT | O_SYNC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd < 0) {
            return;
        }
        capnp::writePackedMessageToFd(fd, messageBuilder);
        close(fd);
    }

    TrieFinder::TrieFinder(const std::string &opt) : path(opt) {
        const std::string &cPath = path + "/tree_cache.bin";
        int fd = open(cPath.c_str(), O_SYNC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (fd == -1) {
            return;
        }
        capnp::ReaderOptions ro;
        ro.nestingLimit = 10240;
        ro.traversalLimitInWords = UINT64_MAX >> 12;
        capnp::PackedFdMessageReader pfd(fd, ro);
        CacheTrie::Reader reader = pfd.getRoot<CacheTrie>();
        trie = Trie(std::forward<CacheTrie::Reader>(reader));
        close(fd);
    }

    std::string TrieFinder::name() const {
        return "TrieFinder";
    }

};