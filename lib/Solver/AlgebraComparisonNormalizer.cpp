#include <klee/Expr.h>
#include <klee/Solver.h>
#include <klee/SolverImpl.h>
#include "klee/SolverStats.h"
#include <klee/Constraints.h>
#include "../klee-pcache/AlgebraComparisonNormalizer.h"

using namespace llvm;
namespace klee {
    class AlgebraComparisonNormalizerSolver : public SolverImpl {
    private:
        Solver *solver;
    public:
        explicit AlgebraComparisonNormalizerSolver(Solver *_solver) : solver(_solver) {}

        bool computeValidity(const Query &query, Solver::Validity &result) override {
            std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
            std::set<ref<Expr>> nConstraints = AlgebraComparisonNormalizer::normalizeExpressions(constraints);
            ConstraintManager cm(std::vector<ref<Expr>>(nConstraints.begin(), nConstraints.end()));
            const Query q(cm, AlgebraComparisonNormalizer::normalizeExpr(query.expr));
            return solver->impl->computeValidity(q, result);
        }

        char *getConstraintLog(const Query &query) override {
            return solver->impl->getConstraintLog(query);
        }

        void setCoreSolverTimeout(double timeout) override {
            solver->impl->setCoreSolverTimeout(timeout);
        }

        bool computeTruth(const Query &query, bool &isValid) override {
            std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
            std::set<ref<Expr>> nConstraints = AlgebraComparisonNormalizer::normalizeExpressions(constraints);
            ConstraintManager cm(std::vector<ref<Expr>>(nConstraints.begin(), nConstraints.end()));
            const Query q(cm, AlgebraComparisonNormalizer::normalizeExpr(query.expr));
            return solver->impl->computeTruth(q, isValid);

        }

        bool computeValue(const Query &query, ref<Expr> &result) override {
            std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
            std::set<ref<Expr>> nConstraints = AlgebraComparisonNormalizer::normalizeExpressions(constraints);
            ConstraintManager cm(std::vector<ref<Expr>>(nConstraints.begin(), nConstraints.end()));
            const Query q(cm, AlgebraComparisonNormalizer::normalizeExpr(query.expr));
            return solver->impl->computeValue(q, result);
        }

        bool computeInitialValues(const Query &query, const std::vector<const Array *> &objects,
                                  std::vector<std::vector<unsigned char> > &values, bool &hasSolution) override {
            std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
            std::set<ref<Expr>> nConstraints = AlgebraComparisonNormalizer::normalizeExpressions(constraints);
            ConstraintManager cm(std::vector<ref<Expr>>(nConstraints.begin(), nConstraints.end()));
            const Query q(cm, AlgebraComparisonNormalizer::normalizeExpr(query.expr));
            return solver->impl->computeInitialValues(q, objects, values, hasSolution);
        }

        SolverRunStatus getOperationStatusCode() override {
            return solver->impl->getOperationStatusCode();
        }

        ~AlgebraComparisonNormalizerSolver() noexcept override { delete solver; }
    };

    Solver *createAlgebraComparisonNormalizerSolver(Solver *_solver) {
        return new Solver(new AlgebraComparisonNormalizerSolver(_solver));
    }
}
