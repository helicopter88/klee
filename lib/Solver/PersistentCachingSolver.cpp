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

#include "../klee-pcache/ExactMatchFinder.h"
#include "../klee-pcache/SubSupersetFinder.h"
#include "../klee-pcache/NameNormalizer.h"
#include "../klee-pcache/Trie.h"
#include "../klee-pcache/TrieFinder.h"

using namespace llvm;
namespace {
    llvm::cl::opt<std::string> PCachePath("pcache-path",
                                          cl::init("/home/klee/cache"),
                                          cl::desc("Path for the persistent cache"));
}
namespace klee {
    typedef std::set<ref<Expr>> KeyType;

    class PersistentCachingSolver : public SolverImpl {
    private:
        TrieFinder trie;
        SubSupersetFinder finder;

        Solver *solver;

        ExactMatchFinder emf;

        bool getAssignment(const Query &query, Assignment *&result);

        bool lookupAssignment(const Query &query, KeyType &key, Assignment *&result);

        bool searchForAssignment(KeyType &key, Assignment *&result);

        double timeout;
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
            finder.close();
            emf.close();
        };
    };


    PersistentCachingSolver::PersistentCachingSolver(Solver *pSolver) : finder(PCachePath), solver(pSolver),
                                                                        timeout(1000.0) {

    }

    bool PersistentCachingSolver::computeTruth(const Query &query, bool &isValid) {
        TimerStatIncrementer t(stats::cexCacheTime);

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
        TimerStatIncrementer t(stats::cexCacheTime);
        Assignment *a;
        if (!getAssignment(query.withFalse(), a))
            return false;
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
        Assignment **r = trie.find(key);//finder.find(key);
        if (r) {
            result = *r;
            return true;
        }
        r = emf.find(key);
        if (r) {
            result = *r;
            finder.insert(key, *r);
            return true;
        }

        if (!UseCexCache) {
            r = finder.findSubSuperSet(key);
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
        if (ConstantExpr *CE = dyn_cast<ConstantExpr>(neg)) {
            if (CE->isFalse()) {
                result = nullptr;
                ++stats::pcacheHits;
                return true;
            }
        } else {
            key.insert(neg);
        }

        if (lookupAssignment(query, key, result))
            return true;

#ifdef DEBUG
        errs() << "We did not find a match in the caches for: \n";
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
        finder.insert(key, result);
        emf.insert(key, result);
        trie.insert(key, result);
        return true;
    }


    bool PersistentCachingSolver::computeValue(const Query &query, ref<Expr> &result) {
        TimerStatIncrementer t(stats::cexCacheTime);

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
        TimerStatIncrementer t(stats::cexCacheTime);
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
        this->timeout = timeout;
        solver->impl->setCoreSolverTimeout(timeout);
    }

////////////
    Solver *createPersistentCachingSolver(Solver *_solver) {
        return new Solver(new PersistentCachingSolver(_solver));
    }
}
