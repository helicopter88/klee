//
// Created by dmarino on 17/02/18.
//
#include <fstream>
#include "klee/Solver.h"

#include "klee/Constraints.h"

#include "klee/SolverImpl.h"
#include <klee/TimerStatIncrementer.h>
#include <klee/SolverStats.h>
#include <klee/Internal/Support/ErrorHandling.h>

#include "klee/util/Assignment.h"
#include "klee/util/ExprUtil.h"

#include "../klee-pcache/RedisFinder.h"
#include "../klee-pcache/PersistentMapOfSetsFinder.h"
#include "../klee-pcache/NameNormalizer.h"
#include "../klee-pcache/Trie.h"
#include "../klee-pcache/TrieFinder.h"
#include "../klee-pcache/NameNormalizerFinder.h"
#include "../klee-pcache/Predicates.h"

using namespace llvm;
namespace {
    cl::opt<std::string> PCachePath("pcache-path",
                                    cl::init("/home/klee/cache"),
                                    cl::desc("Path for the persistent cache"
                                             "name normalized cache will be stored in the same path followed by 'nn'"
                                             "i.e. '/home/klee/cachenn'"));

    cl::opt<bool> PCacheUseNameNormalizer("pcache-use-name", cl::init(false),
                                          cl::desc(
                                                  "EXPERIMENTAL: Use the name normalizer when looking up queries in the caches"));

    cl::opt<std::string> PCacheRedisUrl("pcache-redis-url",
                                        cl::init("127.0.0.1"),
                                        cl::desc("Url for the redis instances"));

    cl::opt<bool> PCacheTryAll("pcache-try-all", cl::init(false), cl::desc(
            "EXPERIMENTAL: try everything(trie sub/superset, persistent map sub/superset and namenormalizers)"
            "when looking up in the caches"));

}
namespace klee {
    typedef std::set<ref<Expr>> KeyType;

    class PersistentCachingSolver : public SolverImpl {
    private:
        /** Underlying solver **/
        Solver *solver;
        /** Cache backend with PersistentMapOfSets used for direct lookups **/
        PersistentMapOfSetsFinder persistentMapOfSetsFinder;
        /** Cache backend with Trie used for direct lookups **/
        /** and cache backend with Trie used to build a NameNormalizerFinder **/
        TrieFinder trieFinder, secondTrieFinder;
        /** Cache backend backed by Redis used for direct lookups and to build a NameNormalizerFinder **/
        RedisFinder redisFinder, secondRedisFinder;
        /** NameNormalizerFinders, only used when @var{PCacheUseNameNormalizer} is set **/
        NameNormalizerFinder nnTrieFinder, nnRedisFinder;
        /** Debug stats **/
        size_t redisHits = 0, pmapHits = 0, trieHits = 0;


        bool getAssignment(const Query &query, Assignment *&result);

        /** TODO: this method is broken currently **/
        bool getAssignmentParallel(KeyType &key, const Query &query, Assignment *&result);

        bool lookupAssignment(const Query &query, KeyType &key, Assignment *&result);

        bool searchForAssignment(KeyType &key, Assignment *&result);

    public:

        bool computeTruth(const Query &, bool &isValid) override;

        bool computeValidity(const Query &, Solver::Validity &result) override;

        bool computeValue(const Query &, ref<Expr> &result) override;

        bool computeInitialValues(const Query &,
                                  const std::vector<const Array *> &objects,
                                  std::vector<std::vector<unsigned char> > &values,
                                  bool &hasSolution) override;

        SolverRunStatus getOperationStatusCode() override;

        char *getConstraintLog(const Query &query) override;

        void setCoreSolverTimeout(double timeout) override;

        explicit PersistentCachingSolver(Solver *pSolver);

        ~PersistentCachingSolver() noexcept override {
            redisFinder.storeFinder();
            trieFinder.storeFinder();
            persistentMapOfSetsFinder.storeFinder();
            if (PCacheUseNameNormalizer) {
                nnTrieFinder.storeFinder();
                nnRedisFinder.storeFinder();
            }
#ifdef DEBUG
            llvm::errs() << "R: " << redisHits << " P: " << pmapHits << " T: " << trieHits << "\n";
#endif
        };


    };


    PersistentCachingSolver::PersistentCachingSolver(Solver *pSolver) : solver(pSolver),
                                                                        persistentMapOfSetsFinder(PCachePath),
                                                                        trieFinder(PCachePath),
                                                                        secondTrieFinder(PCachePath + "nn"),
                                                                        redisFinder(PCacheRedisUrl),
                                                                        secondRedisFinder(PCacheRedisUrl, 6379, 1),
                                                                        nnTrieFinder(secondTrieFinder),
                                                                        nnRedisFinder(secondRedisFinder) {
        // Close the NameNormalizers immediately if they are not meant to be used
        if (!PCacheUseNameNormalizer) {
            nnRedisFinder.storeFinder();
            nnTrieFinder.storeFinder();
        }

    }

    bool PersistentCachingSolver::computeTruth(const Query &query, bool &isValid) {
        TimerStatIncrementer t(stats::pcacheTime);

        // There is a small amount of redundancy here. We only need to know
        // truth and do not really need to compute an assignment. This means
        // that we could check the cache to see if we already know that
        // state ^ query has no assignment. In that case, by the validity of
        // state, we know that state ^ !query must have an assignment, and
        // so query cannot be true (valid). This does get hits, but doesn't
        // really seem to be worth the overhead.

        Assignment *a;
        if (!getAssignment(query, a))
            return false;

        isValid = !a;

        return true;

    }

    bool PersistentCachingSolver::computeValidity(const Query &query, Solver::Validity &result) {
        TimerStatIncrementer t(stats::pcacheTime);
        Assignment *a;
        if (!getAssignment(query.withFalse(), a))
            return false;
        if (!a) {
            query.dump();
        }
        assert(a && "computeValidity() must have assignment");
        ref<Expr> q = a->evaluate(query.expr);
        assert(isa<ConstantExpr>(q) &&
               "assignment evaluation did not result in constant");

        if (cast<ConstantExpr>(q)->isTrue()) {
            if (!getAssignment(query, a))
                return false;
            result = !a ? Solver::True : Solver::Unknown;
        } else {
            if (!getAssignment(query.negateExpr(), a))
                return false;
            result = !a ? Solver::False : Solver::Unknown;
        }

        return true;
    }

    bool PersistentCachingSolver::searchForAssignment(KeyType &key, Assignment *&result) {

        Assignment **r;
        r = trieFinder.find(key);
        if (r) {
            trieHits++;
            result = *r;
            return true;
        }
        r = redisFinder.find(key);
        if (r) {
            redisHits++;
            result = *r;
            trieFinder.insert(key, result);
            delete r;
            return true;
        }
        r = persistentMapOfSetsFinder.find(key);
        if (r) {
            result = *r;
            trieFinder.insert(key, result);
            redisFinder.insert(key, result);
            pmapHits++;
            return true;
        }


        if (PCacheTryAll) {
            r = trieFinder.findSpecial(key);
            if (r) {
                result = *r;
                return true;
            }
            r = persistentMapOfSetsFinder.findSpecial(key);
            if (r) {
                result = *r;
                return true;
            }
        }
        if (PCacheTryAll || PCacheUseNameNormalizer) {
            r = nnTrieFinder.findSpecial(key);
            if (r) {
                result = *r;
                return true;
            }
            r = nnRedisFinder.findSpecial(key);
            if (r) {
                result = *r;
                delete r;
                return true;
            }
        }
        return false;
    }

    bool PersistentCachingSolver::lookupAssignment(const Query &query, KeyType &key, Assignment *&result) {
        TimerStatIncrementer t(stats::pcacheLookupTime);

        bool found = searchForAssignment(key, result);

        if (found)
            ++stats::pcacheHits;
        else ++stats::pcacheMisses;

        return found;
    }

    /** TODO: this does not yet work, figure out why! **/
    bool PersistentCachingSolver::getAssignmentParallel(KeyType &key, const Query &query, Assignment *&result) {
        std::future<Assignment **> nnFind = std::async([&]() -> Assignment ** {
            Assignment **r = nnTrieFinder.findSpecial(key);
            if (r) {
                return r;
            }
            r = nnRedisFinder.findSpecial(key);
            if (r) {
                return r;
            }
            return nullptr;
        });
        std::future<Assignment **> solverFind = std::async([=]() -> Assignment ** {
            Assignment **r = nullptr;

            std::vector<const Array *> objects;
            findSymbolicObjects(key.cbegin(), key.cend(), objects);

            std::vector<std::vector<unsigned char> > values;

            bool hasSolution;
            if (!solver->impl->computeInitialValues(query, objects, values,
                                                    hasSolution))
                return r;
            r = new Assignment *;
            if (hasSolution) {
                *r = new Assignment(objects, values);
            } else {
                *r = nullptr;
            }
            return r;
        });
        nnFind.wait();
        Assignment **r = nnFind.get();
        if (r) {
            result = *r;
            return true;
        }
        solverFind.wait();
        r = solverFind.get();
        if (!r) {
            return false;
        }
        result = *r;
        return true;
    }

    bool PersistentCachingSolver::getAssignment(const Query &query, Assignment *&result) {
        KeyType key = KeyType(query.constraints.begin(), query.constraints.end());
        ref<Expr> neg = Expr::createIsZero(query.expr);
        if (ConstantExpr *CE = dyn_cast<ConstantExpr>(neg)) {
            if (CE->isFalse()) {
                result = nullptr;
                ++stats::pcacheHits;
                return true;
            }
        } else {
            key.insert(neg);
        }

        if (lookupAssignment(query, key, result)) {
            return true;
        }

#ifdef DEBUG
        errs() << "We did not find a match in the direct caches for: \n";
        for (const auto &e : key) e->dump();
        errs() << "\n";
#endif
        std::vector<const Array *> objects;
        findSymbolicObjects(key.cbegin(), key.cend(), objects);

        std::vector<std::vector<unsigned char> > values;

        bool hasSolution;
        if (!solver->impl->computeInitialValues(query, objects, values,
                                                hasSolution))
            return false;
        if (hasSolution) {
            result = new Assignment(objects, values);
        } else {
            result = nullptr;
        }
        TimerStatIncrementer statIncrementer(stats::pcacheInsertionTime);
        redisFinder.insert(key, result);
        trieFinder.insert(key, result);
        persistentMapOfSetsFinder.insert(key, result);
        if (PCacheUseNameNormalizer) {
            nnTrieFinder.insert(key, result);
            nnRedisFinder.insert(key, result);
        }
        return true;
    }


    bool PersistentCachingSolver::computeValue(const Query &query, ref<Expr> &result) {
        TimerStatIncrementer t(stats::pcacheTime);

        Assignment *a;
        if (!getAssignment(query.withFalse(), a)) {
            return false;
        }
        assert(a && "computeValue() must have assignment");
        result = a->evaluate(query.expr);
        assert(isa<ConstantExpr>(result) &&
               "assignment evaluation did not result in constant");
        return true;
    }

    bool
    PersistentCachingSolver::computeInitialValues(const Query &query, const std::vector<const Array *> &objects,
                                                  std::vector<std::vector<unsigned char> > &values,
                                                  bool &hasSolution) {
        TimerStatIncrementer t(stats::pcacheTime);
        Assignment *a;
        if (!getAssignment(query, a))
            return false;
        hasSolution = !!a;

        if (!a)
            return true;

        // FIXME: We should use smarter assignment for result so we don't
        // need redundant copy.
        values = std::vector<std::vector<unsigned char> >(objects.size());
        for (unsigned i = 0; i < objects.size(); ++i) {
            const Array *os = objects[i];
            const auto &it = a->bindings.find(os);

            if (it == a->bindings.end()) {
                values[i] = std::vector<unsigned char>(os->size, 0);
            } else {
                values[i] = it->second;
            }
        }

        return true;
    }

////////////

    SolverImpl::SolverRunStatus PersistentCachingSolver::getOperationStatusCode() {
        return solver->impl->getOperationStatusCode();
    }

    char *PersistentCachingSolver::getConstraintLog(const Query &query) {
        return solver->impl->getConstraintLog(query);
    }

    void PersistentCachingSolver::setCoreSolverTimeout(double timeout) {
        solver->impl->setCoreSolverTimeout(timeout);
    }

////////////
    Solver *createPersistentCachingSolver(Solver *_solver) {
        return new Solver(new PersistentCachingSolver(_solver));
    }
}
