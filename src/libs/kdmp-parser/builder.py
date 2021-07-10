# Axel '0vercl0k' Souchet - February 23 2020
import urllib.request
import os
import sys
import zipfile
import subprocess
import itertools
import platform
import argparse

os_prefix = 'win' if 'Windows' in platform.platform(terse = 1) else 'lin'
is_windows = os_prefix == 'win'
is_linux = not is_windows
is_linux64 = is_linux and platform.architecture()[0] == '64bit'

vsdevprompt = r'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat'

def source_bat(bat_file, arch):
    '''https://dmerej.info/blog/post/cmake-visual-studio-and-the-command-line/'''
    result = {}
    process = subprocess.Popen(
        f'"{bat_file}" {arch} & set',
        stdout = subprocess.PIPE,
        shell = True
    )

    (out, _) = process.communicate()
    for line in out.splitlines():
        line = line.decode()
        if '=' not in line:
            continue
        line = line.strip()
        key, value = line.split('=', 1)
        result[key] = value

    return result

def build(arch, configuration, tests_on):
    # Grab the environment needed for the appropriate arch on Windows.
    env = source_bat(vsdevprompt, arch) if is_windows else os.environ
    dir_name = f'{os_prefix}{arch}-{configuration}'
    build_dir = os.path.join('build', dir_name)
    if not os.path.isdir(build_dir):
        os.mkdir(build_dir)

    # We build an absolute path here because otherwise Ninja ends up creating a 'bin' directory
    # in build\<target>\bin.
    output_dir = os.path.abspath(os.path.join('bin', dir_name))
    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    extra_opts = ()
    if is_linux64 and arch == 'x86':
        # To support 32b binary on a 64b host on Linux.
        extra_opts = (
            f'-DCMAKE_CXX_FLAGS=-m32',
            f'-DCMAKE_C_FLAGS=-m32'
        )

    # Allow the user to override the Python version if `pythonLocation` is found
    # in the environment. This is particularly useful for the CI as `setup-python`
    # sets this environment variable:
    # https://github.com/actions/setup-python/blob/main/src/find-python.ts#L117
    py_root = os.getenv('pythonLocation')
    if py_root is not None:
        extra_opts = extra_opts + (
            f'-DPython3_FIND_STRATEGY=LOCATION',
            f'-DPython3_ROOT_DIR={py_root}'
        )

    cmake_config = (
        'cmake',
        f'-DCMAKE_RUNTIME_OUTPUT_DIRECTORY={output_dir}',
        f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={output_dir}',
        f'-DCMAKE_BUILD_TYPE={configuration}',
        f'-DBUILD_TESTS={tests_on}'
    )
    cmake_config += extra_opts
    cmake_config += (
        '-GNinja',
        os.path.join('..', '..')
    )

    ret = subprocess.call(cmake_config, cwd = build_dir, env = env)

    if ret != 0: return ret

    ret = subprocess.call((
        'cmake',
        '--build',
        '.'
    ), cwd = build_dir, env = env)

    return ret

def main():
    parser = argparse.ArgumentParser('Build and run test')
    parser.add_argument('--run-tests', action = 'store_true', default = False)
    parser.add_argument('--configuration', action = 'append', choices = ('Debug', 'RelWithDebInfo'))
    parser.add_argument('--arch', action = 'append', choices = ('x64', 'x86'))
    args = parser.parse_args()

    if args.configuration is None:
        args.configuration = ('Debug', 'RelWithDebInfo')

    if args.arch is None:
        args.arch = ('x64', 'x86')

    matrix = tuple(itertools.product(
        args.arch,
        args.configuration
    ))

    tests_on = 'ON' if args.run_tests else 'OFF'
    for arch, configuration in matrix:
        if build(arch, configuration, tests_on) != 0:
            print(f'{arch}/{configuration} build failed, bailing.')
            return 1

    print('All good!')
    return 0

if __name__ == '__main__':
    sys.exit(main())