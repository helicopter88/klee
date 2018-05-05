//
// Created by dmarino on 04/05/18.
//
#include <klee/Constraints.h>
#include "klee/Solver.h"
#include "klee/SolverImpl.h"
#include "klee/SolverStats.h"
#include "../klee-pcache/NameNormalizer.h"

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
        //nn.denormalizeArrays(objects);
        if (!res) {
            q.dump();
            assert(false);
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
            q.dump();
            assert(false);
        }
        return true;

    }

    bool NameNormalizerSolver::computeTruth(const Query &query, bool &isValid) {

        NameNormalizer nn;
        std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
        std::set<ref<Expr>> nConstraints = nn.normalizeExpressions(constraints);
        std::vector<ref<Expr>> v(nConstraints.begin(), nConstraints.end());
        ConstraintManager cm(v);
        const Query q(cm, nn.normalizeExpression(query.expr));
        return solver->impl->computeTruth(q, isValid);
    }

    bool NameNormalizerSolver::computeValidity(const Query &query, Solver::Validity &result) {

        NameNormalizer nn;
        std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
        std::set<ref<Expr>> nConstraints = nn.normalizeExpressions(constraints);
        std::vector<ref<Expr>> v(nConstraints.begin(), nConstraints.end());
        ConstraintManager cm(v);
        const Query q(cm, nn.normalizeExpression(query.expr));
        return solver->impl->computeValidity(q, result);
    }


    Solver *createNameNormalizerSolver(Solver *_solver) {
        return new Solver(new NameNormalizerSolver(_solver));
    }

}