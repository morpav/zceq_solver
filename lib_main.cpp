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
  ScopeTimer t;
  auto solution_count = s.Run();
  // printf("zceq_solver::Solver::Run() finished in %ld us\n", t.Micro());
  auto sol_vector = s.GetSolutions();
  if (solution_count > 0) {
    max_solutions = std::min(max_solutions, solution_count);
    for (auto sol : range(max_solutions)) {
      GetMinimalFromIndices(sol_vector[sol]->data(),
                            sol_vector[sol]->size(),
                            (u8*)solutions[sol].data,
                            sizeof solutions[sol].data);
      // printf("reporting solution..\n");
    }
  }
  return solution_count;
}

int ValidateSolution(ZcEquihashSolver* solver, HeaderAndNonce* inputs, Solution* solution) {
  if (!solver || !inputs || !solution)
    return -1;
  auto& s = solver->solver;

  s.Reset((const u8*)inputs->data, sizeof Inputs::data);
  solver->temp_solution.resize(512);
  GetIndicesFromMinimal((const u8*)solution->data, sizeof solution->data,
                        solver->temp_solution.data(), 512);

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


void RunBenchmark(long long nonce_start, int iterations) {
  Inputs inputs;
  memset(inputs.data, 'Z', sizeof(inputs.data));

  Solver solver;
  solver.Reset(inputs);
  printf("Warming up... ");
  fflush(stdout);
  solver.Run();
  printf("done\n");

  ScopeTimer gt;
  auto total_solutions = 0;
  for (auto iter : range(iterations)) {
    ScopeTimer t;
    inputs.SetSimpleNonce(nonce_start + iter);
    solver.Reset(inputs);
    auto solutions = solver.Run();
    printf("%2d solutions in %ldms\n", solutions, t.Micro() / 1000);
    // printf("\nInput processed (%ld us)\n\n\n", t.Micro());
    total_solutions += solutions;
    if ((iter + 1) % 10 == 0) {
      auto iters_done = iter + 1;
      printf("(%d iters, %2d sols, %.4G sol/iter, %.4G sol/s, %.5G s/iter)\n",
             iters_done, total_solutions,
          // gt.Micro() / 1000,
             total_solutions / double(iters_done),
             (total_solutions * 1000000ll) / double(gt.Micro()),
             double(gt.Micro()) / (iters_done * 1000000ll)
      );
      fflush(stdout);
    }
  }
  solver.Reset(inputs);
  printf("*******************************\n");
  printf("Total %d solutions in %ld ms\n", total_solutions, gt.Micro() / 1000);
}

}
