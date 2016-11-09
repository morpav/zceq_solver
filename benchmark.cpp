/* Copyright @ 2016 Pavel Moravec */
#include <chrono>
#include <functional>
#include <iostream>

#include "zceq_misc.h"
#include "zceq_solver.h"
#include "args.hxx"

using namespace zceq_solver;

void RunBenchmark(int iterations_count, int shift, bool profiling, bool warmup);

int main(const int argc, const char * const * argv) {
  std::srand(33);

  if (Const::kRecomputeHashesByRefImpl)
    fprintf(stderr,
            "[zceq_solver] Warning: `Const::kRecomputeBatchHashes` == true.");

  if (Const::kGenerateTestSet)
    fprintf(stderr,
            "[zceq_solver] Warning: `Const::kRecomputeBatchHashes` == true.");

  args::ArgumentParser parser(
      "This is a simple benchmarking and program for zceq_solver. It helps with profile guided"
          " optimizations too. The instruction sets flags are independent in a sense that for example"
          " by disabling SSE2 you don't disable SSE4.1.", "");
  args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
  args::Flag noavx2(parser, "no-avx2", "Disable support for AVX2 instructions.", {"no-avx2"});
  args::Flag noavx1(parser, "no-avx1", "Disable support for AVX1 instructions.", {"no-avx1"});
  args::Flag nosse41(parser, "no-sse41", "Disable support for SSE4.1 instructions.", {"no-sse41"});
  args::Flag nossse3(parser, "no-ssse3", "Disable support for SSSE3 instructions.", {"no-ssse3"});
  args::Flag nosse2(parser, "no-sse2", "Disable support for SSE2 instructions.", {"no-sse2"});
  args::Flag no_batch_blake(parser, "no-batch-blake", "Don't use batch versions of blake2b hash functions.", {"no-batch-blake"});
  args::Flag no_asm_blake(parser, "no-asm-blake", "Don't use asm versions of AVX2 and AVX1 batch blake implementations.", {"no-asm-blake"});
  args::Flag random(parser, "random", "Start from random nonce.", {'r', "random"});
  args::Flag no_warmup(parser, "no-warmup", "Start from random nonce.", {'w', "no-warmup"});

  args::ValueFlag<int> iterations(parser, "iterations", "Number of different nonces to iterate (default = 50)", {'i', "iterations"});
  args::Flag profiling(parser, "profiling", "Run limited number of iterations for each supported intrcution set variant. "
      "Requires AVX2 support for proper behaviour. When specified, other options instuction set options are ignored.", {"profiling"});


  if (random)
    std::srand((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());

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

  auto& batch = RunTimeConfig.kBatchBlakeAllowed;
  auto& scalar = RunTimeConfig.kScalarBlakeAllowed;

  if (!profiling) {
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
    int shift = std::rand();
    RunBenchmark(iterations_count, shift, false, !no_warmup);
  } else {

    if (HasAvx2Support()) {
      printf("WARNING: Running with `--profiling` is best on CPU with AVX2 support.\n");
      printf("  (Otherwise, not all code paths can be fully profiled.)\n");
    }

    InstructionSet all_enabled;
    all_enabled.AVX2 = all_enabled.AVX1 = all_enabled.SSE41
        = all_enabled.SSSE3 = all_enabled.SSE2 = true;

    // 5 iterations per case should be enough, but can be changed.
    int iterations_count = 5;
    if (iterations)
      iterations_count = iterations.Get();

    int shift = std::rand();
    for (auto allow_batch : range(2)) {
      for (auto variant : range(8)) {
        batch = all_enabled;
        scalar = all_enabled;
        RunTimeConfig.kUseAsmBlake2b = true;
        RunTimeConfig.kAllowBlake2bInBatches = (allow_batch > 0);
        switch (variant) {
          case 0:
            // No special instruction set
            batch.SSE2 = scalar.SSE2 = false;
          case 1:
            // SSE2
            batch.SSSE3 = scalar.SSSE3 = false;
          case 2:
            // SSSE3
            batch.SSE41 = scalar.SSE41 = false;
          case 3:
            // SSE41
            batch.AVX1 = scalar.AVX1 = false;
          case 4:
            // AVX1 asm
            batch.AVX2 = scalar.AVX2 = false;
          case 5:
            // AVX2 asm
            break;
          // ------------------
          case 6:
            // AVX1 no asm
            batch.AVX2 = scalar.AVX2 = false;
          case 7:
            // AVX2 no asm
            RunTimeConfig.kUseAsmBlake2b = false;
        }
        printf("=======================================================================\n");
        printf(" Batch-hash=%d | SSE2=%d SSSE3=%d SSE4.1=%d AVX1=%d (asm=%d) AVX2=%d (asm=%d) \n",
               RunTimeConfig.kAllowBlake2bInBatches, scalar.SSE2, scalar.SSSE3, scalar.SSE41,
               scalar.AVX1, RunTimeConfig.kUseAsmBlake2b, scalar.AVX2, RunTimeConfig.kUseAsmBlake2b);
        printf("-----------------------------------------------------------------------\n");
        RunBenchmark(iterations_count, shift, true, !no_warmup);
        if (random)
          shift = std::rand();
      }
    }
  }
  return 0;
}


void RunBenchmark(int iterations_count, int shift, bool profiling, bool warmup) {
  // The solver needn't to copy the data when they are aligned properly.
  alignas(32) Inputs inputs;
  // Just produce some "random" block header.
  memset(inputs.data, 'Z', 140);

  Solver solver;
  if (warmup) {
    solver.Reset(inputs);
    printf("Warming up... \n");
    fflush(stdout);
    solver.Run();
  }

  // Start to measure time.
  ScopeTimer gt;
  auto total_solutions = 0;
  auto total_invalid_sols = 0;
  for (auto iter : range(iterations_count)) {
    ScopeTimer t;
    inputs.SetSimpleNonce((u64)(iter + shift));
    solver.Reset(inputs);
    auto solution_count = solver.Run();
    auto solutions = solver.GetSolutions();
    total_invalid_sols += solver.GetInvalidSolutionCount();
    assert(solutions.size() == solution_count);
    printf("%2d solutions in %" PRId64 " ms (%d inv.)\n", solution_count, t.Micro() / 1000,
           solver.GetInvalidSolutionCount());
    t.Reset();

    total_solutions += solution_count;
    if ((iter + 1) % 10 == 0 || (iter + 1) == iterations_count) {
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
  if (!profiling) {
    printf("*******************************\n");
    printf("Total %d solutions in %" PRId64 " ms\n", total_solutions,
           gt.Micro() / 1000);
  }
}
