/* Copyright @ 2016 Pavel Moravec */
#ifndef ZCEQ_SOLVER_H_LIB_INTERFACE_H_
#define ZCEQ_SOLVER_H_LIB_INTERFACE_H_

extern "C" {

typedef struct {
  char data[1344];
} Solution;

typedef struct {
  unsigned int data[512];
} ExpandedSolution;

typedef struct HeaderAndNonce {
  char data[140];
} HeaderAndNonce;

typedef struct ZcEquihashSolverT ZcEquihashSolver;

ZcEquihashSolver* CreateSolver(void);

void DestroySolver(ZcEquihashSolver* solver);

int FindSolutions(ZcEquihashSolver* solver, HeaderAndNonce* inputs,
                  Solution solutions[], int max_solutions);

int ValidateSolution(ZcEquihashSolver* solver, HeaderAndNonce* inputs, Solution* solutions);

void RunBenchmark(long long nonce_start, int iterations);

bool ExpandedToMinimal(Solution* minimal, ExpandedSolution* expanded);

bool MinimalToExpanded(ExpandedSolution* expanded, Solution* minimal);

// Officially requested interface for solver challenge.
int SolverFunction(const unsigned char* input,
                   bool (*validBlock)(void*, const unsigned char*),
                   void* validBlockData,
                   bool (*cancelled)(void*),
                   void* cancelledData,
                   int numThreads,
                   int n, int k);

}

#endif  // ZCEQ_SOLVER_H_LIB_INTERFACE_H_
