import SCons.Action
import subprocess

Import('env')
Import('lib_env')

lib_src = ['lib_main.cpp']
lib_objs = lib_env.SharedObject(lib_env['COMMON_SRC'] +
                                lib_src)
asm_objs = env.Glob('blake2b-asm/zcblake2_avx*.o')

blake2_src = Glob('blake2/*.c')
blake2_lib_objs = lib_env.SharedObject(blake2_src)

shared_lib = lib_env.SharedLibrary('zceqsolver', lib_objs + blake2_lib_objs + asm_objs)

benchmark_src = ['benchmark.cpp']
benchmark_objs = env.Object(benchmark_src + blake2_src + env['COMMON_SRC'])

benchmark = env.Program('zceq_benchmark', benchmark_objs + asm_objs)
if 'PROFILE_RAW_FILE' in env:
    profile_raw_file = env.Command('${PROFILE_RAW_FILE}', benchmark,
                                   action=SCons.Action.Action(
                                       'LD_LIBRARY_PATH=$VARIANT_DIR ' \
                                       './$SOURCE --profiling -i3 --no-warmup',
                                       """
                                       **********************************************************

                                             PROFILING run - please, do NOT stop the process.
                                                     This is SLOWER then normal run.
                                           Alternatively, run the build as "scons --no-profiling"

                                       **********************************************************"""))
    # Unfortunately, on some distributions, llvm-profdata alias
    # doesn't exist, there is only binary with a version hardcoded
    version_str = subprocess.check_output(['llvm-config', '--version'])
    maj_min_version = '.'.join(version_str.split('.')[:2])
    profile_data_file = env.Command('${PROFILE_DATA_FILE}', profile_raw_file[0],
                                   action=SCons.Action.Action(
	                               'llvm-profdata-{} merge --output=$TARGET $SOURCE'.format(maj_min_version),
                                       'Processing profiler raw data: $SOURCE into $TARGET'
                                   ))
    env['PROFILE_DATA_FILE'] = profile_data_file[0]
else:
    # Extend dependencies if profiler data is needed
    if 'ADD_PROFILE_DATA_DEPS' in env:
        lib_env.Depends(benchmark_objs + lib_objs, env['PROFILE_DATA_FILE'])
    # install the shared library into the python package
    lib_env.Install('#pyzceqsolver', shared_lib)
    lib_env.Alias('pyinstall', '#pyzceqsolver')
