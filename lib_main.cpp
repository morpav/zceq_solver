/* Copyright @ 2016 Pavel Moravec */
#include <chrono>
#include <functional>

#include "zceq_misc.h"
#include "zceq_solver.h"
#include "lib_interface.h"

using namespace zceq_solver;

#include "equihash_reuse.h"


extern "C" {

struct ZcEquihashSolverT {
  Solver solver;
  // Temporary variable for checking solutions withou memory allocation.
  std::vector<u32> temp_solution;
};


ZcEquihashSolver* CreateSolver(void) {
  return new ZcEquihashSolver();
}

void DestroySolver(ZcEquihashSolver* solver) {
  if (solver != nullptr)
    delete (Solver*)(void*)solver;
}

int FindSolutions(ZcEquihashSolver* solver, HeaderAndNonce* inputs,
                  Solution solutions[], int max_solutions) {
  if (!solver || !inputs || !solutions || !max_solutions)
    return -1;
  auto& s = solver->solver;

  s.Reset((const u8*)inputs->data, sizeof Inputs::data);
  auto solution_count = s.Run();
  auto sol_vector = s.GetSolutions();
  if (solution_count > 0) {
    max_solutions = std::min(max_solutions, solution_count);
    for (auto sol : range(max_solutions)) {
      GetMinimalFromIndices(sol_vector[sol]->data(),
                            sol_vector[sol]->size(),
                            (u8*)solutions[sol].data,
                            sizeof solutions[sol].data);
    }
  }
  return solution_count;
}

int ValidateSolution(ZcEquihashSolver* solver, HeaderAndNonce* inputs, Solution* solution) {
  if (!solver || !inputs || !solution)
    return -1;
  auto& s = solver->solver;

  s.Reset((const u8*)inputs->data, sizeof Inputs::data);
  solver->temp_solution.resize(Const::kSolutionSize);
  GetIndicesFromMinimal((const u8*)solution->data, sizeof solution->data,
                        solver->temp_solution.data(), Const::kSolutionSize);

  if (s.ValidateSolution(solver->temp_solution))
    return 1;
  return 0;
}

bool ExpandedToMinimal(Solution* minimal, ExpandedSolution* expanded) {
  return GetMinimalFromIndices(expanded->data, sizeof expanded->data / sizeof *expanded->data,
                               (u8*)minimal->data, sizeof minimal->data);
}

bool MinimalToExpanded(ExpandedSolution* expanded, Solution* minimal) {
  return GetIndicesFromMinimal((u8*)minimal->data, sizeof minimal->data,
                               expanded->data, sizeof expanded->data / sizeof *expanded->data);
}

int SolverFunction(const unsigned char* input,
                   bool (*validBlock)(void*, const unsigned char*),
                   void* validBlockData,
                   bool (*cancelled)(void*),
                   void* cancelledData,
                   int numThreads,
                   int n, int k) {
  if (n != 200 || k != 9)
    return -1;

  // Make the instance on stack :/
  Solver s;
  s.Reset((const u8*)input, 140);
  auto solution_count = s.Run();
  auto sol_vector = s.GetSolutions();
  if (solution_count > 0) {
    u8 solution[1344];
    for (auto sol : range(solution_count)) {
      GetMinimalFromIndices(sol_vector[sol]->data(),
                            sol_vector[sol]->size(),
                            solution,
                            sizeof solution);
      validBlock(validBlockData, solution);
    }
  }
  return solution_count;
}
}
