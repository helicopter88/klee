
#include "klee/Config/config.h"
#ifdef ENABLE_PERSISTENT_CACHE

#include <klee/Constraints.h>
#include <klee/Internal/Support/Debug.h>
#include <klee/Solver.h>
#include "klee/SolverImpl.h"
#include <klee/SolverStats.h>
#include <klee/TimerStatIncrementer.h>

#include "klee/util/Assignment.h"
#include "klee/util/ExprUtil.h"

#include "../klee-pcache/PersistentMapOfSetsFinder.h"
#include "../klee-pcache/RedisFinder.h"
#include "../klee-pcache/TrieFinder.h"

using namespace llvm;
namespace {
    cl::opt<std::string> PCachePath("pcache-path",
                                    cl::init("/home/klee/cache"),
                                    cl::desc("Path for the persistent cache"
                                             "name normalized cache will be stored in the same path followed by 'nn'"
                                             "i.e. '/home/klee/cachenn'"));

    cl::opt<bool> PCacheUsePMap("pcache-use-pmap",
                                cl::init(false),
                                cl::desc("Use the Persistent Map of Sets instead of"
                                         " the Set-Trie backend for the persistent cache"));

    cl::opt<bool> PCacheCollectOnly("pcache-collect-only",
                                    cl::init(false),
                                    cl::desc("Do not fetch the persistent caches, but populate them"));

    cl::opt<std::string> PCacheRedisUrl("pcache-redis-url",
                                        cl::init("none"),
                                        cl::desc("Url for the redis instances, use 'none' to disable redis"));

    cl::opt<int> PCacheRedisPort("pcache-redis-port",
                                    cl::init(6379),
                                    cl::desc("Port for the redis instances (default=6379)"));

    cl::opt<int> PCacheRedisDBNum("pcache-redis-dbnum",
                                    cl::init(0),
                                    cl::desc("Database number to use for Redis (default=0)"));
    cl::opt<bool> PCacheTryAll("pcache-try-all",
                               cl::init(false),
                               cl::desc("EXPERIMENTAL: try everything(trie sub/superset,"
                                        " persistent map sub/superset)"
                                        "when looking up in the caches"));


}
namespace klee {

    /**
     * Use this Finder to create finders that do not perform lookups but only collect queries.
     */
    class CollectingFinder : public Finder {
        Finder *base;
    public:
        explicit CollectingFinder(Finder *_base) : base(_base) {};

        Assignment **find(std::set<ref<Expr>> &exprs) override {
            return nullptr;
        }

        void insert(std::set<ref<Expr>> &exprs, Assignment *assignment) override {
            base->insert(exprs, assignment);
        }

        void storeFinder() override {
            base->storeFinder();
        }

        ~CollectingFinder() override {
            delete base;
        }

        void printStats() const override {
            KLEE_DEBUG(errs() << "Collection only\n";);
            base->printStats();
        }
    };

    /**
     * Class used to build a chain of finders.
     */
    class ChainingFinder : public Finder {
        std::vector<Finder *> finders;

    public:
        explicit ChainingFinder(std::vector<Finder *> _finders) : finders(std::move(_finders)) {};

        /**
         * Performs a lookup in all the finders passed in, one by one.
         * If one finder has a hit, all the previous finders that missed will have the assignment inserted.
         * @param exprs set of constraints to be looked up
         * @return the assignment that matched the constraints or nullptr
         */
        Assignment **find(std::set<ref<Expr>> &exprs) override {
            std::vector<Finder *> missedFs;

            for (Finder *_f : finders) {
                Assignment **ret = _f->find(exprs);
                if (ret) {
                    _f->incrementHits();
                    for (Finder *mF : missedFs) {
                        mF->insert(exprs, *ret);
                    }
                    return ret;
                }
                _f->incrementMisses();
                missedFs.emplace_back(_f);
            }

            return nullptr;
        }

        void insert(std::set<ref<Expr>> &exprs, Assignment *assignment) override {
            for (Finder *_f : finders) {
                _f->insert(exprs, assignment);
            }
        }

        Assignment **findSpecial(std::set<ref<Expr>> &exprs) override {
            std::vector<Finder *> missedFs;
            for (Finder *_f : finders) {
                Assignment **ret = _f->findSpecial(exprs);
                if (ret) {
                    _f->incrementSpecialHits();
                    for (Finder *mF : missedFs) {
                        mF->insert(exprs, *ret);
                    }
                    return ret;
                }
                missedFs.emplace_back(_f);
                _f->incrementSpecialMisses();
            }

            return nullptr;
        }

        void storeFinder() override {
            this->printStats();
            for (Finder *_f: finders) {
                _f->storeFinder();
            }
        }

        void printStats() const final {
                    for (Finder *f : finders) {
                        f->printStats();
                    }
        }

        ~ChainingFinder() noexcept override {
            for (Finder *_f : finders) delete _f;
        }
    };

    typedef std::set<ref<Expr>> KeyType;

    class PersistentCachingSolver : public SolverImpl {
    private:
        Solver *solver;

        Finder *finder;


        bool getAssignment(const Query &query, Assignment *&result);

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
            finder->storeFinder();
            delete finder;
            delete solver;
        };

        Finder *constructChainingFinder() const;
    };

    Finder *PersistentCachingSolver::constructChainingFinder() const {
        std::vector<Finder *> finders;
        if (!PCacheUsePMap) {
            finders.emplace_back(new TrieFinder(PCachePath));
        } else {
            finders.emplace_back(new PersistentMapOfSetsFinder(PCachePath));
        }
#ifdef PCACHE_ENABLE_REDIS
        if (PCacheRedisUrl != "none") {
            finders.emplace_back(new RedisFinder(PCacheRedisUrl, (size_t)PCacheRedisPort, PCacheRedisDBNum));
        }
#endif

        Finder *cf = new ChainingFinder(finders);
        if (PCacheCollectOnly) {
            return new CollectingFinder(cf);
        }
        return cf;
    }

    PersistentCachingSolver::PersistentCachingSolver(Solver *pSolver) : solver(pSolver),
                                                                        finder(constructChainingFinder()) {

    }

    bool PersistentCachingSolver::computeTruth(const Query &query, bool &isValid) {
        TimerStatIncrementer t(stats::pcacheTime);

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
#ifndef NDEBUG
        if (!a) {
            query.dump();
        }
#endif
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

        Assignment **r = finder->find(key);
        if (r) {
            result = *r;
            return true;
        }
        if (PCacheTryAll) {
            r = finder->findSpecial(key);
            if (r) {
                result = *r;
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

    bool PersistentCachingSolver::getAssignment(const Query &query, Assignment *&result) {
        KeyType key = KeyType(query.constraints.begin(), query.constraints.end());
        ref<Expr> neg = Expr::createIsZero(query.expr);
        if (auto *CE = dyn_cast<ConstantExpr>(neg)) {
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

        KLEE_DEBUG(
                errs() << "PCache: did not find a match in the caches for: \n";
                for (const auto &e : key) {
                    e->dump();
                    errs() << "hash: " << e->hash() << "\n";
                }
                errs() << "--\n";
        );
        std::vector<const Array *> objects;
        findSymbolicObjects(key.cbegin(), key.cend(), objects);

        std::vector<std::vector<unsigned char> > values;

        bool hasSolution;
        if (!solver->impl->computeInitialValues(query, objects, values,
                                                hasSolution)) {
            return false;
        }
        if (hasSolution) {
            result = new Assignment(objects, values);
        } else {
            result = nullptr;
        }

        TimerStatIncrementer statIncrementer(stats::pcacheInsertionTime);
        finder->insert(key, result);
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

    bool PersistentCachingSolver::computeInitialValues(const Query &query,
                                                       const std::vector<const Array *> &objects,
                                                       std::vector<std::vector<unsigned char> > &values,
                                                       bool &hasSolution) {
        TimerStatIncrementer t(stats::pcacheTime);
        Assignment *a;
        if (!getAssignment(query, a))
            return false;
        hasSolution = a != nullptr;

        if (!a)
            return true;

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
#endif