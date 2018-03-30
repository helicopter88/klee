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

using namespace klee;
using namespace llvm;

typedef std::set<ref<Expr> > KeyType;

class PersistentCachingSolver : public SolverImpl {
private:
    Solver *solver;
    SubSupersetFinder finder;
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

    ~PersistentCachingSolver() noexcept {
        finder.close();
        emf.close();
    };
};


PersistentCachingSolver::PersistentCachingSolver(Solver *pSolver) : solver(pSolver), timeout(1000.0) {

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
    std::future<cpp_redis::reply> redisFind = emf.future_find(key);
    Assignment **r = finder.find(key);
    if (r) {
        result = *r;
        return true;
    }
    std::future<Assignment **> subsupersetFind = std::async(std::launch::async,
                                                            [&]() { return finder.findSubSuperSet(key); });
    redisFind.wait();
    if (redisFind.valid()) {
        Assignment **binding = emf.processResponse(std::forward<std::future<cpp_redis::reply>>(redisFind));
        if (binding) {
            result = *binding;
            return true;
        }
    }
    subsupersetFind.wait();
    if (subsupersetFind.valid()) {
        Assignment **binding = subsupersetFind.get();
        if (!binding) {
            return false;
        }
        result = *binding;
        return true;
    }
    return false;
}

bool PersistentCachingSolver::lookupAssignment(const Query &query, KeyType &key, Assignment *&result) {


    bool found = searchForAssignment(key, result);
    if (found)
        ++stats::previousCacheHits;
    else ++stats::previousCacheMisses;

    return found;
}

bool PersistentCachingSolver::getAssignment(const Query &query, Assignment *&result) {
    KeyType key = KeyType(query.constraints.begin(), query.constraints.end());
    ref<Expr> neg = Expr::createIsZero(query.expr);
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(neg)) {
        if (CE->isFalse()) {
            result = nullptr;
            ++stats::previousCacheHits;
            return true;
        }
    } else {
        key.insert(neg);
    }

    std::future<Assignment **> solverLookup = std::async(std::launch::deferred, [=]() -> Assignment ** {
        std::vector<const Array *> objects;
        findSymbolicObjects(key.cbegin(), key.cend(), objects);

        std::vector<std::vector<unsigned char> > values;

        bool hasSolution;
        if (!solver->impl->computeInitialValues(query, objects, values,
                                                hasSolution))
            return nullptr;

        auto **binding = new Assignment *;
        if (hasSolution) {
            *binding = new Assignment(objects, values);
        } else {
            *binding = nullptr;
        }
        return binding;
    });

    if (lookupAssignment(query, key, result))
        return true;

    solverLookup.wait();
#ifdef ENABLE_KLEE_DEBUG
    errs() << "We did not find a match in the caches for: \n";
    for (const auto &e : key) e->dump();
    errs() << "\n";
#endif
    Assignment **binding = solverLookup.get();
    if (!binding) {
        return false;
    }

    result = *binding;
    finder.insert(key, *binding);
    emf.insert(key, *binding);
    return true;
}


bool PersistentCachingSolver::computeValue(const Query &query, ref<Expr> &result) {
    TimerStatIncrementer t(stats::cexCacheTime);

    Assignment *a;
    if (!getAssignment(query.withFalse(), a))
        return false;
    assert(a && "computeValue() must have assignment");
    result = a->evaluate(query.expr);
    assert(isa<ConstantExpr>(result) &&
           "assignment evaluation did not result in constant");
    return true;
}

bool PersistentCachingSolver::computeInitialValues(const Query &query, const std::vector<const Array *> &objects,
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
        Assignment::bindings_ty::iterator it = a->bindings.find(os);

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
Solver *klee::createPersistentCachingSolver(Solver *_solver) {
    return new Solver(new PersistentCachingSolver(_solver));
}