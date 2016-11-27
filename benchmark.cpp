/* Copyright @ 2016 Pavel Moravec */
#include <chrono>
#include <functional>
#include <iostream>

#include "zceq_arch.h"
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
  args::Flag noneon64(parser, "no-neon64", "Disable support for NEON64 instructions.", {"no-neon64"});
  args::Flag noneon32(parser, "no-neon32", "Disable support for NEON32 instructions.", {"no-neon32"});
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
  
  InstructionSet all_valid;
  all_valid.NEON64 = IS_ARM_NEON;
  all_valid.NEON32 = IS_ARM_NEON;
  all_valid.AVX2 = IS_X86;
  all_valid.AVX1 = IS_X86;
  all_valid.SSE41 = IS_X86;
  all_valid.SSSE3 = IS_X86;
  all_valid.SSE2 = IS_X86;
  
  InstructionSet all_disabled;
  all_disabled.NEON64 = false;
  all_disabled.NEON32 = false;
  all_disabled.AVX2 = false;
  all_disabled.AVX1 = false;
  all_disabled.SSE41 = false;
  all_disabled.SSSE3 = false;
  all_disabled.SSE2 = false;

  if (!profiling) {
    batch.NEON64 = scalar.NEON64 = !noneon64;
    batch.NEON32 = scalar.NEON32 = !noneon32;
    batch.AVX2 = scalar.AVX2 = !noavx2;
    batch.AVX1 = scalar.AVX1 = !noavx1;
    batch.SSE41 = scalar.SSE41 = !nosse41;
    batch.SSSE3 = scalar.SSSE3 = !nossse3;
    batch.SSE2 = scalar.SSE2 = !nosse2;
    RunTimeConfig.kAllowBlake2bInBatches = no_batch_blake;
    RunTimeConfig.kUseAsmBlake2b = no_asm_blake;

    int iterations_count = 5;
    if (iterations)
      iterations_count = iterations.Get();
    int shift = std::rand();
    RunBenchmark(iterations_count, shift, false, !no_warmup);
  } else {

    if (HasAvx2Support()) {
      printf("WARNING: Running with `--profiling` is best on CPU with AVX2 support.\n");
      printf("  (Otherwise, not all code paths can be fully profiled.)\n");
    }

    InstructionSet tested_variant = all_disabled;
    
    int variants_count = all_valid.NEON64 +
                         all_valid.NEON32 +
                         all_valid.AVX2 +
                         all_valid.AVX1 +
                         all_valid.SSE41 +
                         all_valid.SSSE3 +
                         all_valid.SSE2 +
                         all_valid.AVX2 +
                         1; // Generic
    

    // 5 iterations per case should be enough, but can be changed.
    int iterations_count = 5;
    if (iterations)
      iterations_count = iterations.Get();

    int shift = std::rand();
    for (auto allow_batch : range(IS_X86+1)) {
      for (auto variant : range(variants_count)) {
        batch = all_disabled;
        scalar = all_disabled;
        RunTimeConfig.kUseAsmBlake2b = IS_X86;
        RunTimeConfig.kAllowBlake2bInBatches = (allow_batch > 0);

        printf("=======================================================================\n");
        printf(" Batch-hash=%d | ", RunTimeConfig.kAllowBlake2bInBatches);

        if (all_valid.NEON64 && !tested_variant.NEON64) {
          printf("NEON64\n");
          tested_variant.NEON64 = batch.NEON64 = scalar.NEON64 = true;
        } else if (all_valid.NEON32 && !tested_variant.NEON32) {
          printf("NEON32\n");
          tested_variant.NEON32 = batch.NEON32 = scalar.NEON32 = true;
        } else if (all_valid.AVX2 && tested_variant.AVX2) {
          printf("AVX2\n");
          tested_variant.AVX2 = batch.AVX2 = scalar.AVX2 = true;
        } else if (all_valid.AVX1 && !tested_variant.AVX1) {
          printf("AVX1\n");
          tested_variant.AVX1 = batch.AVX1 = scalar.AVX1 = true;
        } else if (all_valid.SSE41 && !tested_variant.SSE41) {
          printf("SSE41\n");
          tested_variant.SSE41 = batch.SSE41 = scalar.SSE41 = true;
        } else if (all_valid.SSSE3 && !tested_variant.SSSE3) {
          printf("SSSE3\n");
          tested_variant.SSSE3 = batch.SSSE3 = scalar.SSSE3 = true;
        } else if (all_valid.SSE2 && !tested_variant.SSE2) {
          printf("SSE2\n");
          tested_variant.SSE2 = batch.SSE2 = scalar.SSE2 = true;
        } else {
          printf("GENERIC\n");
          // Don't enable anything, use the generic backend
        }
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
