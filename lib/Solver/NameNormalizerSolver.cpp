//
// Created by dmarino on 04/05/18.
//
#include <klee/Constraints.h>
#include <klee/Internal/Support/Debug.h>
#include "klee/Solver.h"
#include "klee/SolverImpl.h"
#include "klee/SolverStats.h"
#include "NameNormalizer.h"

using namespace llvm;
namespace klee {
    class NameNormalizerSolver : public SolverImpl {
    private:
        Solver *solver;
    public:
        bool computeValidity(const Query &, Solver::Validity &result) override;

        bool computeTruth(const Query &, bool &isValid) override;

        bool computeValue(const Query &, ref<Expr> &result) override;

        bool computeInitialValues(const Query &,
                                  const std::vector<const Array *> &objects,
                                  std::vector<std::vector<unsigned char> > &values,
                                  bool &hasSolution) override;

        SolverRunStatus getOperationStatusCode() override;

        explicit NameNormalizerSolver(Solver *_solver) : solver(_solver) {}

        char *getConstraintLog(const Query &query) override;

        void setCoreSolverTimeout(double timeout) override;

        ~NameNormalizerSolver() override {
            delete solver;
        }

    };

    SolverImpl::SolverRunStatus NameNormalizerSolver::getOperationStatusCode() {
        return solver->impl->getOperationStatusCode();
    }

    bool NameNormalizerSolver::computeInitialValues(const Query &query,
                                                    const std::vector<const Array *> &objects,
                                                    std::vector<std::vector<unsigned char> > &values,
                                                    bool &hasSolution) {
        NameNormalizer nn;
        std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
        std::set<ref<Expr>> nConstraints = nn.normalizeExpressions(constraints);
        std::vector<ref<Expr>> v(nConstraints.begin(), nConstraints.end());
        ConstraintManager cm(v);

        const Query q(cm, nn.normalizeExpression(query.expr));
        std::vector<const Array *> nObjects = nn.normalizeArrays(objects);

        bool res = solver->impl->computeInitialValues(q, nObjects, values, hasSolution);
        if (!res || !hasSolution) {
            KLEE_DEBUG(
                    llvm::errs() << "Could not find normalized solution for query:\n";
                    q.dump();
                    for (const auto &object : nObjects) llvm::errs() << object->getName() << "\n";
                    llvm::errs() << "Trying clean query\n";
            );
            hasSolution = true;
            return solver->impl->computeInitialValues(query, objects, values, hasSolution);
        }
        return true;
    }


    bool NameNormalizerSolver::computeValue(const Query &query, ref<Expr> &result) {
        NameNormalizer nn;
        std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
        std::set<ref<Expr>> nConstraints = nn.normalizeExpressions(constraints);
        std::vector<ref<Expr>> v(nConstraints.begin(), nConstraints.end());
        ConstraintManager cm(v);
        const Query q(cm, nn.normalizeExpression(query.expr));

        if (!solver->impl->computeValue(q, result)) {
            KLEE_DEBUG(
                    llvm::errs() << "Could not find normalized solution for query:\n";
                    q.dump();
                    llvm::errs() << "Trying clean query\n";
            );
            return solver->impl->computeValue(query, result);
        }
        result = nn.denormalizeExpression(result);
        return true;

    }

    bool NameNormalizerSolver::computeTruth(const Query &query, bool &isValid) {

        NameNormalizer nn;
        std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
        std::set<ref<Expr>> nConstraints = nn.normalizeExpressions(constraints);
        std::vector<ref<Expr>> v(nConstraints.begin(), nConstraints.end());
        ConstraintManager cm(v);
        const Query q(cm, nn.normalizeExpression(query.expr));
        if (!solver->impl->computeTruth(q, isValid)) {
            KLEE_DEBUG(
                    llvm::errs() << "Could not find normalized solution for query:\n";
                    q.dump();
                    llvm::errs() << "Trying clean query\n";
            );
            return solver->impl->computeTruth(query, isValid);
        }
        return true;
    }

    bool NameNormalizerSolver::computeValidity(const Query &query, Solver::Validity &result) {

        NameNormalizer nn;
        std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
        std::set<ref<Expr>> nConstraints = nn.normalizeExpressions(constraints);
        std::vector<ref<Expr>> v(nConstraints.begin(), nConstraints.end());
        ConstraintManager cm(v);
        const Query q(cm, nn.normalizeExpression(query.expr));
        if (!solver->impl->computeValidity(q, result)) {
            KLEE_DEBUG(
                    llvm::errs() << "Could not find normalized solution for query:\n";
                    q.dump();
                    llvm::errs() << "Trying clean query\n";
            );
            return solver->impl->computeValidity(query, result);
        }
        return true;
    }

    char *NameNormalizerSolver::getConstraintLog(const Query &query) {
        return solver->impl->getConstraintLog(query);
    }

    void NameNormalizerSolver::setCoreSolverTimeout(double timeout) {
        solver->impl->setCoreSolverTimeout(timeout);
    }


    Solver *createNameNormalizerSolver(Solver *_solver) {
        return new Solver(new NameNormalizerSolver(_solver));
    }

}