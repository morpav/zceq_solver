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

  args::ArgumentParser parser("This is a simple benchmarking and program for zceq_solver. It helps with profili guided"
                                  " optimizations too. The isntruction sets flags are independent in a sense that for example"
                                  " by disabling SSE2 you don't disable SSE4.1.", "");
  args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
  args::Flag noavx2(parser, "no-avx2", "Disable support for AVX2 instructions.", {"no-avx2"});
  args::Flag noavx1(parser, "no-avx1", "Disable support for AVX1 instructions.", {"no-avx1"});
  args::Flag nosse41(parser, "no-sse41", "Disable support for SSE4.1 instructions.", {"no-sse41"});
  args::Flag nossse3(parser, "no-ssse3", "Disable support for SSSE3 instructions.", {"no-ssse3"});
  args::Flag nosse2(parser, "no-sse2", "Disable support for SSE2 instructions.", {"no-sse2"});
  args::Flag no_batch_blake(parser, "no-batch-blake", "Don't use batch versions of blake2b hash functions.", {"no-batch-blake"});
  args::Flag no_asm_blake(parser, "no-asm-blake", "Don't use asm versions of AVX2 and AVX1 batch blake implementations.", {"no-asm-blake"});

  args::ValueFlag<int> iterations(parser, "iterations", "Number of different nonces to iterate (default = 50)", {'i', "iterations"});

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

  // Process command line options and disable various instrucion sets.
  auto& batch = RunTimeConfig.kBatchBlakeAllowed;
  auto& scalar = RunTimeConfig.kScalarBlakeAllowed;
  if (noavx2)
    batch.AVX2 = scalar.AVX2 = false;
  if (noavx1)
    batch.AVX1 = scalar.AVX1 = false;
  if (nosse41)
    batch.SSE41 = scalar.SSE41 = false;
  if (nossse3)
    batch.SSSE3 = scalar.SSSE3 = false;
  if (nosse2)
    batch.SSE2 = scalar.SSE2 = false;
  if (no_batch_blake)
    RunTimeConfig.kAllowBlake2bInBatches = false;
  if (no_asm_blake)
    RunTimeConfig.kUseAsmBlake2b = false;

  int iterations_count = 50;
  if (iterations)
    iterations_count = iterations.Get();

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
