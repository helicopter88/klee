#include <klee/Expr.h>
#include <klee/Solver.h>
#include <klee/SolverImpl.h>
#include "klee/SolverStats.h"
#include <klee/Constraints.h>
#include <klee/Internal/Support/Debug.h>
#include <klee/util/Assignment.h>
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
            if (!solver->impl->computeValidity(q, result)) {
                llvm::errs() << "Could not find normalized solution for query:\n";
                q.dump();
                llvm::errs() << "Trying clean query\n";
                return solver->impl->computeValidity(query, result);
            }
            return true;
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
            if (!solver->impl->computeTruth(q, isValid)) {
                llvm::errs() << "Could not find normalized solution for query:\n";
                q.dump();
                llvm::errs() << "Trying clean query\n";
                return solver->impl->computeTruth(query, isValid);
            }
            return true;
        }

        bool computeValue(const Query &query, ref<Expr> &result) override {
            std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
            std::set<ref<Expr>> nConstraints = AlgebraComparisonNormalizer::normalizeExpressions(constraints);
            ConstraintManager cm(std::vector<ref<Expr>>(nConstraints.begin(), nConstraints.end()));
            const Query q(cm, AlgebraComparisonNormalizer::normalizeExpr(query.expr));
            if (!solver->impl->computeValue(q, result)) {
                llvm::errs() << "Could not find normalized solution for query:\n";
                q.dump();
                llvm::errs() << "Trying clean query\n";
                return solver->impl->computeValue(query, result);
            }
            return true;
        }

        bool computeInitialValues(const Query &query, const std::vector<const Array *> &objects,
                                  std::vector<std::vector<unsigned char> > &values, bool &hasSolution) override {
            std::set<ref<Expr>> constraints(query.constraints.begin(), query.constraints.end());
            std::set<ref<Expr>> nConstraints = AlgebraComparisonNormalizer::normalizeExpressions(constraints);
            ConstraintManager cm(std::vector<ref<Expr>>(nConstraints.begin(), nConstraints.end()));
            const Query q(cm, query.expr);
            bool res = solver->impl->computeInitialValues(q, objects, values, hasSolution);
            Assignment assignment = new Assignment(objects, values);
            if (!res || !hasSolution) {
                llvm::errs() << "Could not find normalized solution for query:\n";
                q.dump();
                llvm::errs() << "Trying clean query\n";
                query.dump();
                llvm::errs() << "------------------\n";
                hasSolution = true;
                return solver->impl->computeInitialValues(query, objects, values, hasSolution);
            }
            return true;
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
