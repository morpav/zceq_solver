"""
This SConstruct prepares 2 build environments:
- final_env - used for building the sa_solver binary
- lib_env - used for building the shared library. This environment is a clone of final_env with some modifications specific to building the library

Build targets are provided by SConscript.{solver,lib} resp.

Crossbuild with mingw to have a windows version is supported.

OpenCL headers detection is supported
"""
import os
import SCons.Scanner
import SCons.Action

default_cross_prefix = 'x86_64-w64-mingw32-'
AddOption('--cross-prefix',
          dest='cross_prefix',
          type='string',
          nargs=1,
          action='store',
          metavar='PREFIX',
          default='x86_64-w64-mingw32-',
          help='Cross toolchain prefix (default: {})'.format(default_cross_prefix))
AddOption('--enable-win-cross-build',
          dest='enable_win_cross_build',
          action='store_true',
          default=False,
          help='Enable cross build (default: {})'.format(False))
AddOption('--no-profiling',
          dest='enable_profiling',
          action='store_false',
          default=True,
          help="Disable code profiling and don't build with profiler data (default: Enabled)")
AddOption('--wrap-memcpy',
          dest='wrap_memcpy',
          action='store_true',
          default=False,
          help="Tell the linker to wrap calls to memcpy (for replacing it by legacy version)")
AddOption('--march',
          dest='march',
          type='string',
          nargs=1,
          action='store',
          metavar='MACHINE_ARCH',
          default='native',
          help='Set machine architecture (default: native)')


final_env = Environment(CROSS_PREFIX=GetOption('cross_prefix'),
                        MARCH=GetOption('march'),
                        CXX='clang++',
                        LD='clang++',
                        # the project links with pre-built object
                        # modules, scons doesn't like this unless we
                        # tell it they are usable the same way
                        STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME=1)


final_env.Append(CCFLAGS=['-march=${MARCH}', '-O3',
                          '-Wall', '-Wno-deprecated-declarations'],
                 CFLAGS=['-std=gnu99'],
                 CPPDEFINES=['NDEBUG'],
                 CXXFLAGS=['-std=c++11'],
                 LINKFLAGS=['-static-libgcc', '-static-libstdc++'])

env_replace_options = {}
env_append_options = {}
if GetOption('enable_win_cross_build'):
    env_replace_options = {
        'AR': '${CROSS_PREFIX}ar',
        'AS': '${CROSS_PREFIX}as',
        'CC': '${CROSS_PREFIX}gcc',
        'CPP': '${CROSS_PREFIX}cpp',
        'CXX': '${CROSS_PREFIX}g++',
        'RANLIB': '${CROSS_PREFIX}ranlib',
        'SHLIBSUFFIX': '.dll',
        'LIBPREFIX': '',
        'PROGSUFFIX': '.exe',
        'SHCCFLAGS': ['$CCFLAGS'],
        'SHCFLAGS': ['$CFLAGS'],
    }
    env_append_options = {
        'CPPDEFINES': [# this is needed to use mingw's printf not
                       # ms_printf (for format strings using int64_t)
                       '__USE_MINGW_ANSI_STDIO']
    }

final_env.Replace(**env_replace_options)
final_env.Append(**env_append_options)

final_env.Append(COMMON_SRC=['zceq_solver.cpp',
                             'zceq_blake2b.cpp',
                             'zceq_space_allocator.cpp'])

profiling_env = final_env.Clone()
# make variant dirs part of the environments
profiling_env.Append(VARIANT_DIR='build-profiling')

# Run profiler for native builds only
if GetOption('enable_win_cross_build'):
    final_env.Append(VARIANT_DIR='build-win')
else:
    final_env.Append(VARIANT_DIR='build-native')

profile_raw_file = 'code.profraw'
profile_data_file = 'code.profdata'

gen_profile_data_flags = '-fprofile-instr-generate=$VARIANT_DIR/{}'.format(profile_raw_file)
use_profile_data_flags = profiling_env.subst('-fprofile-instr-use=$VARIANT_DIR/{}'.format(profile_data_file))
profiling_env.Append(CPPDEFINES=[],
                     LINKFLAGS=[gen_profile_data_flags],
                     CXXFLAGS=[gen_profile_data_flags],
                     PROFILE_RAW_FILE=profile_raw_file,
                     PROFILE_DATA_FILE=profile_data_file)



# Run profiler for native builds only
if not GetOption('enable_win_cross_build') and GetOption('enable_profiling'):
    profiling_env.SConscript('SConscript',
                             exports={'env': profiling_env,
                                      'lib_env': profiling_env},
                             variant_dir=profiling_env['VARIANT_DIR'],
                             duplicate=0)
    # Adjust the final build environment with profile data and signal
    final_env.Append(LINKFLAGS=[use_profile_data_flags],
                     CXXFLAGS=[use_profile_data_flags],
                     PROFILE_DATA_FILE=profiling_env['PROFILE_DATA_FILE'],
                     ADD_PROFILE_DATA_DEPS=True)

lib_final_env = final_env.Clone()
# Set static build only for the benchmark - that's why we append it
# after cloning the final shared library environment
final_env.Append(LINKFLAGS=['-static', '-Wl,--no-export-dynamic'])

# Optionally, append compatibility when linking the shared library
if not GetOption('enable_win_cross_build') and GetOption('wrap_memcpy'):
    lib_final_env.Append(LINKFLAGS=['-Wl,--wrap=memcpy'],
                         COMMON_SRC=['libc_compatibility.c'])

final_env.SConscript('SConscript', exports={'env': final_env,
                                            'lib_env': lib_final_env},
                     variant_dir=final_env['VARIANT_DIR'],
                     duplicate=0)
