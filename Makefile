CXX = clang++
#CXX = g++
CC = clang
LD = clang++
MARCH = native
LLVM_PROFDATA = llvm-profdata-$(shell llvm-config-3.8 --version | sed -e 's/\(.\..\)\../\1/g')

DEBUG = -DNDEBUG -g3
ARCH = -march=armv8-a+crc -mtune=native -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -ftree-vectorize

CXXFLAGS += -std=c++11 -O3 $(DEBUG) -fPIC $(ARCH)
CFLAGS += -std=c99 -O3 $(DEBUG) -fPIC $(ARCH)


#CXXFLAGS += -std=c++11 -march=$(MARCH) -O3 -DNDEBUG -fPIC
#CFLAGS += -std=c99 -march=$(MARCH) -O3 -DNDEBUG -fPIC


ifeq ($(NOPROFILING), 1)
ifeq ($(USE_PROFILE_DATA), 1)
profile_data := build-profiling/code.profdata
use_profile_data_flags := -fprofile-instr-use=build-profiling/code.profdata
CXXFLAGS += $(use_profile_data_flags)
LDFLAGS += $(use_profile_data_flags)
endif
build_dir = build-final
targets += $(build_dir)/libsolver.so \
           $(build_dir)/zceq_benchmark
else
# Benchmark build
targets += run_benchmark
gen_profile_data_flags := -fprofile-instr-generate=build-profiling/code.profraw
CXXFLAGS += $(gen_profile_data_flags)
LDFLAGS += $(gen_profile_data_flags)
build_dir = build-profiling
endif

objs := zceq_solver.o \
	zceq_blake2b.o \
	zceq_space_allocator.o

blake2_objs := blake2b-ref.o \
	blake2b-compress-ref.o \
	blake2b-compress-neon64.o \
	blake2b-compress-avx2.o \
	blake2b-compress-sse41.o \
	blake2b-compress-ssse3.o

asm_objs :=
#asm_objs := blake2b-asm/zcblake2_avx2.o \
#    blake2b-asm/zcblake2_avx1.o

shared_lib_objs := lib_main.o

benchmark_objs := benchmark.o

shared_lib_ldflags := -static-libgcc -static-libstdc++ -shared -fPIC -Wl,-soname,libzceqsolver.so
executable_ldflags := -static -Wl,--no-export-dynamic

build-profiling/%.profdata: build-profiling/%.profraw | $(build_dir)
	$(LLVM_PROFDATA) merge --output=$@ $^

all: $(targets)

# Build the benchmark, run it and repeat the build with profiling data in place
run_benchmark: $(build_dir)/zceq_benchmark
	@echo '**********************************************************'
	@echo ''
	@echo '   PROFILING run - please, do NOT stop the process.'
	@echo '         This is SLOWER then normal run.'
	@echo '   Alternatively, run make as "$ make NOPROFILING=1"'
	@echo ''
	@echo '**********************************************************'
	$(build_dir)/zceq_benchmark --profiling -i5 --no-warmup
	make NOPROFILING=1 USE_PROFILE_DATA=1

$(build_dir)/zceq_benchmark: $(addprefix $(build_dir)/,$(benchmark_objs) $(blake2_objs) $(objs))
	$(LD) $(LDFLAGS) $(executable_ldflags) -o $@ $^ $(asm_objs)

$(build_dir)/libsolver.so: $(addprefix $(build_dir)/,$(shared_lib_objs) $(blake2_objs) $(objs)) | $(profile_data)
	$(LD) $(LDFLAGS) $(shared_lib_ldflags) -o $@ $^ $(asm_objs)

$(build_dir)/%.o: %.cpp | $(build_dir) $(profile_data)
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(build_dir)/%.o: blake2/%.c | $(build_dir)
	$(CC) $(CFLAGS) -o $@ -c $^

$(build_dir):
	mkdir -p $@

clean:
	rm -r -f build-*
