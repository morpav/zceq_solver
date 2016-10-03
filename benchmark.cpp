/* Copyright @ 2016 Pavel Moravec */
#include <chrono>
#include <functional>
#include <iostream>

#include "zceq_misc.h"
#include "zceq_solver.h"
#include "args.hxx"

using namespace zceq_solver;

int main(const int argc, const char * const * argv) {
  if (Const::kRecomputeHashesByRefImpl)
    fprintf(stderr, "[zceq_solver] Warning: `Const::kRecomputeBatchHashes` == true.");

  if (Const::kGenerateTestSet)
    fprintf(stderr, "[zceq_solver] Warning: `Const::kRecomputeBatchHashes` == true.");

  args::ArgumentParser parser("This is a simple benchmarking program for zceq_solver", "");
  args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
  args::Flag profiling(parser, "profiling", "Run limited number of iterations used when profiling the code.", {"profiling"});
  args::ValueFlag<int> iterations(parser, "iterations", "Number of different nonces to iterate", {'i', "iterations"});

  try {
    parser.ParseCLI(argc, argv);
  }
  catch (args::Help) {
    std::cout << parser;
    return 0;
  }
  catch (args::ParseError e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  }
  catch (args::ValidationError e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  int iterations_count = 50;
  if (iterations)
    iterations_count = iterations.Get();
  if (profiling)
    iterations_count = 30;

  // std::srand(std::chrono::steady_clock::now().time_since_epoch().count());
  std::srand(33);
  int random_shift = std::rand();

  using ZcEqSolver = zceq_solver::Solver;
  using Inputs = zceq_solver::Inputs;

  Inputs inputs;
  // Just produce some "random" block header.
  memset(inputs.data, 'Z', 140);

  ZcEqSolver solver;
  solver.Reset(inputs);
  printf("Warming up... \n");
  fflush(stdout);
  solver.Run();

  // Start to measure time.
  ScopeTimer gt;
  auto total_solutions = 0;
  auto total_invalid_sols = 0;
  for (auto iter : range(iterations_count)) {
    ScopeTimer t;
    inputs.SetSimpleNonce((u64)(iter + random_shift));
    solver.Reset(inputs);
    auto solution_count = solver.Run();
    auto solutions = solver.GetSolutions();
    total_invalid_sols += solver.GetInvalidSolutionCount();
    assert(solutions.size() == solution_count);
    printf("%2d solutions in %ldms (%d inv.)\n", solution_count, t.Micro() / 1000,
           solver.GetInvalidSolutionCount());
    t.Reset();

    total_solutions += solution_count;
    if ((iter + 1) % 10 == 0) {
      auto iters_done = iter + 1;
      printf("(%d iters, %2d sols (%d inv.), %.4G sol/iter, %.4G sol/s, %.5G s/iter)\n",
             iters_done, total_solutions, total_invalid_sols,
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
  return 0;
}
