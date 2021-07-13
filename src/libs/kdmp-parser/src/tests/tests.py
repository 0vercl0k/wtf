# Axel '0vercl0k' Souchet - July 25 2020
import urllib.request
import os
import sys
import zipfile
import subprocess
import itertools
import platform
import argparse

testdatas_url = 'https://github.com/0vercl0k/kdmp-parser/releases/download/v0.1/testdatas.zip'

def test(bin_dir, dmp_path):
    cmd = (
        os.path.join(bin_dir, 'testapp'),
        dmp_path
    )

    print('Launching "{0}"..'.format(' '.join(cmd)))
    return subprocess.call(cmd)

def test_python(script_dir, bin_dir, dmp_path, pymodule):
    py_exe = sys.executable
    if pymodule == 'kdmp_d':
        if py_exe.endswith('3.exe'):
            py_exe = py_exe.replace('3.exe', '_d.exe')
        else:
            py_exe = py_exe.replace('.exe', '_d.exe')
    cmd = (
        py_exe,
        os.path.join(script_dir, 'tests_bindings.py'),
        os.path.abspath(bin_dir),
        dmp_path
    )

    print('Launching "{0}"..'.format(' '.join(cmd)))
    return subprocess.call(cmd)

def main():
    parser = argparse.ArgumentParser('Run test')
    parser.add_argument('--bindir', required = True)
    parser.add_argument('--pymodule', required = True, nargs = '?')
    args = parser.parse_args()

    script_dir = os.path.dirname(__file__)
    if not os.path.isfile(os.path.join(script_dir, 'full.dmp')):
        # Download the test datas off github.
        print(f'Downloading {testdatas_url}..')
        archive_path, _ = urllib.request.urlretrieve(testdatas_url)
        print(f'Successfully downloaded the test datas in {archive_path}, extracting..')

        # Unzip its content in the source directory so that we don't download the files
        # for every targets we are building.
        zipfile.ZipFile(archive_path).extractall(script_dir)

        # Once we have extracted the archive content, we can delete it.
        os.remove(archive_path)

    # Build full path for both the full / bitmap dumps.
    full = os.path.join(script_dir, 'full.dmp')
    bmp = os.path.join(script_dir, 'bmp.dmp')
    dmp_paths = (full, bmp)

    # Now iterate through all the configurations and run every flavor of test.exe against
    # both dumps.
    for dmp_path in dmp_paths:
        if test(args.bindir, dmp_path) != 0:
            print(f'{args.bindir}/{dmp_path} test failed, bailing.')
            return 1
    
        # Run python bindings tests
        if args.pymodule:
            if test_python(script_dir, args.bindir, dmp_path, args.pymodule) != 0:
                print(f'{args.bindir}/{dmp_path} python test failed, bailing.')
                return 1

    print('All good!')
    return 0

if __name__ == '__main__':
    sys.exit(main())