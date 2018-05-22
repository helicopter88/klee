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

    Assignment **emptyAssignment = new Assignment *;

    Assignment **TrieFinder::find(std::set<ref<Expr>> &exprs) {
        if (exprs.empty()) {
            return emptyAssignment;
        }
        Assignment **ret = trie.get(exprs);
        if (ret) {
            return ret;
        }

        ret = trie.existsSubset(exprs);
        if (ret) {
            NullAssignment s;
            if (s(*ret)) {
                return ret;
            }
        }
        return nullptr;
    }

    Assignment **TrieFinder::findSpecial(std::set<ref<Expr>> &exprs) {
        if (exprs.empty()) {
            return emptyAssignment;
        };
        std::vector<Assignment**> assigments = trie.getSubsets(exprs);
        NullOrSatisfyingAssignment p(exprs);
        for(Assignment** pAssignment : assigments) {
            if(p(*pAssignment)) {
                return pAssignment;
            }
        }
        SatisfyingAssignment s(exprs);
        assigments = trie.getSupersets(exprs);
        for(Assignment** pAssignment : assigments) {
            if(s(*pAssignment)) {
                return pAssignment;
            }
        }
        return nullptr;
    }

    void TrieFinder::insert(std::set<ref<Expr>> &exprs, Assignment *assignment) {
        ++stats::pcacheTrieSize;
        trie.set(exprs, &assignment);
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
        // Store an empty assignment to not malloc every time it is requested
        *emptyAssignment = new Assignment;

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