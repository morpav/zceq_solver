## Overview

This project contains a C++11 implementation of Equihash problem
solver, particularly its "200,9" variant as currently used by
Zcash. In addition, there is a python binding for using the solver from
python.

For building options and target support, please see "Future work"
section. Currently, only Linux and x86-64 is supported (tested).

An intention of this README is to describe algorithmic and
implementation approach of the contained solver and to help a reader
to understand the code better. It expects that the user is already
familiar with Equihash problem (generalized birthday paradox problem).

Current performance of the solver is ~7 solutions/s on Intel(R)
Core(TM) i5-4670K CPU @ 3.40GHz, which translates to roughly 270ms
and average of 1.88 solutions for algorithm iteration (one nonce).

(For **binary builds**, please go to
https://github.com/morpav/zceq_solver--bin).

(Document in progres..)

Index:

 - Build
 - Algorithm outline
   - Used terminology
 - Design discussion and optimizations
   - Blake2b
   - Memory management
   - Main solver
   - General optimization
 - Future work and enhancements
 - Acknowledgments
 - Other


## Build

The code has been compiled only on Linux by GCC and Clang
compilers. We don't expact any particular issues with porting to
windows, but no work has been done here. There is no external
dependecy other then C++ standard library in the code.

The provided makefile expects clang++ installed. It builds an
executable with profiling enabled, runs a short benchmark and then
builds final executable and shared library (in build-final).

Run `make NOPROFILING=1` to skip the profiling build. You can switch
to gcc then. (Profiling using gcc could be done too).

### SCons

Alternatively, the software can be build with scons.

The scons build also supports cross building for windows (profiling is
automatically disabled for this scenario). See ```scons -h``` for all
local options.

### Python Package



## Algorithm outline

### Used terminology

 - String = A sequence of bits as produced by Blake2 hash function and
   conforming to equihash definition. During the solution process,
   strings are reduced in length when collisions are detected.

 - Segment = A part of string subject to finding collisions in one
   step of the algorithm. Particularly, 200,9 variant of the problem
   has 10 segments, 20bits each.

 - (Algorithm) iteration = Process of finding solutions for one input
   (a block header and a fixed nonce).

 - Step = Part of iteration which finds collisions of one string
   segment and produces strings one segment shorter. The last step
   produces solutions, not other strings (there is nothing to collide).

 - (Collision) Pair = Two strings with the same first segment's value,
   later XOR-ed together to produce a new string for the next algorithm
   step.

 - Pair link = Small object containing indices of two strings which were
   source strings from step i for some string in step i+1.

 - Pair index = (or only index) Data structure holding information
   about which two source strings produced which output string.

 - Bucket = A collection of strings with similar first segment (the
   same bit prefix of the segment). The main property of a bucket is
   that string collisions can happen only among strings from the same
   bucket. Buckets allow decomposing the problem and solve it per
   partes. A size of buckets (or bit length) can be changed in alg.
   configuration, but 8bit prefix seems to work the best.

 - Collision group = A set of strings (from the same bucket) with the
   same first hash segment.

### Main algorithm

Basically, the algorithm is a variant on a classical hashtable-based
collision finding. Since every problem is different the algorithm is
designed to fit the problem as closely as possible.

Some implementation aspects of the algorithm are discussed separately
later. Here are the main steps:

 1) At first, 2^21 strings is generated and distributed into
 buckets. Within buckets, the strings are stored sequentially in
 memory. Each string has its own unique number `j`.  These numbers
 form a solution if the string is later a part of solution. The number
 j is physically stored in the string object itself, not in any
 separate data structure. At first. It shares space with future pair
 link (l0, l1), placed into the string as well. Initial string number
 can be considered as a trivial link with only one virtual
 predecessor.

 2) Perform one step of collision searching. Particularly, for each
 bucket in increasing order:

   2.1) We linearly scan strings in the bucket and compute a histogram
   of first segment values. So we count number of occurrences of every
   possible first segment's value. First few bits of the segment value
   are the same for all stings in the segment, so they are ignored.

   For this counting, we have an array Counts with number of items equal to
   number of possible different segment values. When we load a string,
   we increase a number in a particular field of the array.

   2.1) During the scan, we store linearly the first segment's value
   into another array TmpSeg for later usage. Indices of the array are
   the same as indices of the source strings. Also we read a link
   field of the string in hand and copy the link value into and
   separate index data structure, again indices of strings and
   links correspond.

   2.3) We iterate through the histogram and eliminate all segment
   values (collision groups) with number of strings smaller then 2 or
   higher then 13 (configurable). These strings cannot typically
   produce a reasonable solution. For cases <2 it is obvious, there
   cannot be a collision. For the higher sizes, the insight is
   following. Input data should not be distinguishable from random
   data (given that blake2b is a good hash function). It is highly
   improbable that more then 13 strings collide in a particular
   segment. But it happens in reality quite often when duplicate
   strings are involved in in some colliding strings
   (transitively). Filtering collision groups greater then 13 tries to
   filters invalid solutions early. It is a really cheap check as
   well.

   While eliminating collision groups of bad sizes, we calculate
   cumulative sum of number of strings participating in a
   not-eliminated collision groups. At the and, each collision group
   (segment value) has a continuous range of indices in an output
   array Collisions. Ranges of different collision groups are of
   course non-overlapping.

   2.4) We go through array TmpSeg (with stored first segment values)
   and if a segment value has not been eliminated in the previous
   step, index of the value(string) is stored into array Collisions
   (into the precomputed position). After processing complete TmpSeg
   array, we have indices of all colliding strings sitting next to
   each other in one array. We know where exactly indices for a
   particular collision group are and how big the group is.

   2.5) We take collision groups one by one and produce output strings
   (each pair of strings produces an output string). Here, we know
   that the groups are limited in size and complete. We generate all
   output strings (string for the next step) together.

   While generating new strings, we reduce one segment and write the
   strings into particular output bucket, based on the now new first segment.
   We store indices of the source strings into a pair link field
   within the producing string. This link field will be copied
   into standalone index data structure in the next step, while
   computing then next histogram.

   2.6) When all buckets are processed, we can forget the input
   strings and find collision for next semgent in the strings.

 4) If 8 segments has not been reduced yet, continue with step 2).

 4) When collisions for 9th segment are found, we do not produce any
    other strings, but we use information stored in the pair link
    objects to track down the source strings. The translation is
    pretty straightforward. The output is a list of 512 indices.

 5) If the list contains any duplicate items, the solution candidate
    is rejected. Otherwise indices are reorder to conform to zcash
    algorithm binding and a solution is produced.


### Design discussion and implementation

It can seem that this algorithm does too much unnecessary work. There
are some explanations of this approach.

### Design discussion and implementation

In general, to produce a fast solver one has to make a lot of design
decisions. There is a strong tension between solution generality and
exploiting every possible information to be faster.

We tried to go the second way with allowing the code to be general in
cases when a compiler can reduce a general case to efficient
particular case in compile time.

There are two main logical parts of the solver. A strings generator,
producing blake2b hashes and collision search part. An algorithm of
the first is exactly defined and the only space for efficiency is
reduction of unnecessary work and efficient usage of CPU power.


### Blake2b

The following ideas are used (not claiming originality)

  -



#### Blake2b

TBD:

 - Excessive calls to compress
 -

#### Implementation details

TBD

 - Compile time evaluation and type derivation.
 - Data structures selection (sizes, motivation), alignment.
 - Memory allocation (space allocator).

Explain low-level optimization techniques applied, how and why the
code is shaped becasuse of this:
 - Cache locality
 - Exploit HW prefetching
 - SW prefetching
 - Non-temporal writes to indices.
 - Manual loop unrolling or other "forcing" techniques for compilers.
 -
 - Branch-less code
 - Profile guided optimization
   - (in makefile, but can be distributed with source code for users to compile)
 - Huge pages - The solver tries to allocate its data using mmap and to obtain huge
pages


### improvements

- Reduce segment prefix already defined by bucket, where the string is stored.
- Take bucket bits not from the beginning.



### Hash reordering

Since the strings must be collided in prescribed way and the strings
are treated as big endian, some extra work is needed for efficient
implementation.

We want to read the first segment of a string really fast (on little
endian machines) because it is a frequent operation in a hot loop. So
we preprocess the strings to reorder bits in the produced hashes. The
final form is so that only single load is needed with possibly one
bitwise shift.

The reorder is done only once, during initial strings generation,
particularly lower and higher 4 bits are swapped in every 3rd byte in
every 5 bytes. The exact representation as in original solver is not
needed, only each segment must contain the same bits (even reordered)
as specified by "algorithm binding".

There is an option of the solver to produce "expanded" version of the
strings so that each segment occupies exactly 3 bytes (not only
20bits). This eliminates additional shift operation otherwise needed
when accessing the first segment every second algorithm step. It turns
out that it the additional memory causes the algorithm to run slower
(it is not so surprising).

The reorder is done during copying the strings to a final position
to eliminate copying. 32B is processes instead of 25B to allow compiler
to use vector instruction on modern architectures.


 - Inefficient interface (python better - holds solver instance)
 - Diff. implementation for solution validation and main solving.


It can be beneficial to switch to simpler algorithm for later steps.
Some measurements show that  when the strings

There is a plenty of parameters (zceq_config.h) which can be tweaked
for better general performance. It is easy to find best ones for
one machine or maybe
CPU, but it is usually not the wanted result.


### Supported architectures

Currently, the code performs best on AVX2 CPUs, built target "native".
But it can be run on any platform capable of building the code (x86-64
considered mainly).

There is a run time detection of the CPU features and different code
runs for AVX2, AVX, etc. Better support for older architectures can be
added.

There are three different implmentations of the hash function for
AVX2, one from sodium library (not competitive in solving mode, great
in checking mode), our original implementation using intrinsics and
lately, we integrated xenon's great ASM implementation when its C
binding is finally available. Thanks to this integration AVX1 machines
can benefit from fast blake2b as well.

The fact that intrinsic implementation for AVX2 is quite competitive
performance-wise to manually written asm code is promising. Very
similar (vectorized/batch) implementation can be done for generic SSE2
(or better SSSE3) CPUs. The amount of code for the intrinsics-based
AVX2 batch implementation is roughly 150 lines of code. Roughly the
same code can be written for older CPUs to still benefit from
independent vectorization of 2 blake2b.


## Future work, potential

The project is clearly just after rush development/research mode. The
code needs to be properly cleaned up and documented. Besides theese
engineering tasks, there is a few of options how to make it run even
faster. We didn't have a time to try all options.

 - The final step of the alg. is not optimal. For example, it would be
   probably beneficial to detect and store all candidate solutions
   during last collisions finding and then start to validate them if
   they are really solutions or not. Currently, we check every
   candidate immediately and it must negatively interact with the just
   running collision search. It is mostly cache issue.

 - We can not store first segment's prefix (already defined by bucket
   in which the string is located) so that the strings could be
   smaller in memory.

 - The solution extraction (translation of pair link into a solution)
   and validation itself can be improved as well. Currently we find
   duplicates by sorting all 512 indices and linearly looking for
   duplicates.

   E.g. if we produce all candidate solution and then find duplicates
   at one moment by a kind of hash/lookup table (for example by) it
   can be done pretty efficiently.  We can use some large lookup table
   (few kB) because we would not risk bad L1 cache interacions with
   other running tasks.

 - Better testing and tweaking of the algorithm to produce consistent
   result across different CPUs (and memory subsystems).

 - Memory consumption can be reduced. Not all memory blocks are
   currently managed by space space allocator (manually).

 - Better build process, port to Windows.

 - Code documentation.

 - TBD

## Acknowledgments

The biggest influancer for this solver has been xenon's asm
solver. Besides the obvious reuse of his asm code, we reused an idea
of encoding two bits of informaion into a position of the strings
(allowed by two level bucketing or partitioning). It is really a great
idea that allows to stay on 32bits per pair link instance!

---

### Threading, interfaces

There is no threading support added. The solver can be easily executed
from more threads in the same process or even from different
processes. The reason why we didn't spend time with threads is that we
cannot see an obvious benefit from solving the same problem instance
by more threads at the same time. It seems quite sure (a guess based
on quite a few hours with the algorithms and implementation) that it
is significatnly more efficient (and easier) to run independent
solving processes (different nonces). Since one iteration takes ~300ms
on modern hardware, we don't see it as a latency/timing issue.

The python binding is pretty new so there can be bugs there. Obvious
benefit of the python binding in comparin with CLI inteface is that it
can hold a state, so the solver can by reused for a lot of
computations.


ZCash Open Source Miner Challenge interface -
