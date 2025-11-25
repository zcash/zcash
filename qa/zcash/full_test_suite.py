#!/usr/bin/env python3
#
# Execute all automated tests related to Zcash.
# This script organizes, runs, and reports results for C++, Rust, and security tests.
#

import argparse
from glob import glob
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Callable, Dict, List, Union

# Determine the repository root directory by navigating up three levels from this script's location.
REPOROOT = Path(__file__).resolve().parent.parent.parent.parent

def repofile(filename: str) -> str:
    """Returns the absolute path to a file within the repository root."""
    return str(REPOROOT / filename)

def get_arch_dir() -> Optional[str]:
    """
    Detects the architecture-specific build directory inside the 'depends' folder.
    Prioritizes Linux, then MacOS, then Windows (MinGW).
    """
    depends_dir = REPOROOT / 'depends'

    # 1. Linux (x86_64-pc-linux-gnu)
    arch_dir = depends_dir / 'x86_64-pc-linux-gnu'
    if arch_dir.is_dir():
        return str(arch_dir)

    # 2. MacOS
    arch_dirs = glob(str(depends_dir / 'x86_64-apple-darwin*'))
    if arch_dirs:
        return arch_dirs[0]

    # 3. Windows (MinGW)
    arch_dirs = glob(str(depends_dir / 'x86_64-w64-mingw32*'))
    if arch_dirs:
        return arch_dirs[0]

    # Error handling moved to caller for cleaner execution flow
    return None


# --- Security Check Utilities ---

RE_RPATH_RUNPATH = re.compile(r'No RPATH.*No RUNPATH')
RE_FORTIFY_AVAILABLE = re.compile(r'FORTIFY_SOURCE support available.*Yes')
RE_FORTIFY_USED = re.compile(r'Binary compiled with FORTIFY_SOURCE support.*Yes')

# List of binaries that are C++ (CXX) and Rust-based
CXX_BINARIES = [
    'src/zcashd',
    'src/zcash-cli',
    'src/zcash-gtest',
    'src/zcash-tx',
    'src/test/test_bitcoin',
]
RUST_BINARIES = [
    'src/zcashd-wallet-tool',
]

def _run_checksec_script(filename: str, options: List[str]) -> bytes:
    """Helper to execute the checksec.sh script."""
    command = [repofile('qa/zcash/checksec.sh')] + options
    try:
        return subprocess.check_output(command)
    except subprocess.CalledProcessError as e:
        print(f"ERROR: checksec.sh failed for {filename}. Output:\n{e.output.decode()}")
        return b'' # Return empty bytes on error

def test_rpath_runpath(filename: str) -> bool:
    """Checks if the binary has RPATH or RUNPATH set (should be absent)."""
    output = _run_checksec_script(filename, [f'--file={repofile(filename)}'])
    
    if RE_RPATH_RUNPATH.search(output.decode('utf-8')):
        print(f'PASS: {filename} has no RPATH or RUNPATH.')
        return True
    else:
        print(f'FAIL: {filename} has an RPATH or a RUNPATH.')
        print(output.decode('utf-8'))
        return False

def test_fortify_source(filename: str) -> bool:
    """Checks if the C++ binary was compiled with FORTIFY_SOURCE enabled."""
    proc = subprocess.Popen(
        [repofile('qa/zcash/checksec.sh'), f'--fortify-file={repofile(filename)}'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, # Added stderr capture for safety
    )
    # Read the two crucial lines for FORTIFY_SOURCE check
    line1 = proc.stdout.readline()
    line2 = proc.stdout.readline()
    proc.terminate()
    
    # Check if FORTIFY support is available AND used
    if (RE_FORTIFY_AVAILABLE.search(line1.decode('utf-8')) and 
        RE_FORTIFY_USED.search(line2.decode('utf-8'))):
        print(f'PASS: {filename} has FORTIFY_SOURCE.')
        return True
    else:
        print(f'FAIL: {filename} is missing FORTIFY_SOURCE.')
        # Print the relevant lines for debugging
        print(line1.decode('utf-8').strip()) 
        print(line2.decode('utf-8').strip())
        return False

def check_security_hardening() -> bool:
    """Runs all security hardening checks (PIE, RELRO, Canary, NX, RPATH, FORTIFY)."""
    ret = True
    src_makefile = REPOROOT / 'src' / 'Makefile'
    
    if src_makefile.exists():
        # Prefer running the standard Makefile target
        ret &= subprocess.call(['make', '-C', repofile('src'), 'check-security']) == 0
    else:
        # Fallback for CI environments where make might not be fully configured
        bin_programs = CXX_BINARIES + RUST_BINARIES # Use the centralized list
        
        print(f"Checking security hardening for {len(bin_programs)} binaries/scripts...")

        for program in bin_programs:
            is_script = program in RUST_BINARIES # Check if it's a script/non-PIE binary
            command = [repofile('contrib/devtools/security-check.py'), repofile(program)]
            
            # Allow no canary for non-C++ binaries/scripts
            if is_script:
                command.insert(1, '--allow-no-canary')
                
            ret &= subprocess.call(command) == 0

    # The remaining checks are only applicable to ELF binaries (Linux/Unix executables)
    zcashd_path = REPOROOT / 'src' / 'zcashd'
    if zcashd_path.exists():
        with open(zcashd_path, 'rb') as f:
            magic = f.read(4)
            # Check for ELF magic number (\x7f followed by E L F)
            if not magic.startswith(b'\x7fELF'):
                return ret # Not ELF, skip remaining security checks

    # Check RPATH/RUNPATH for all binaries
    for bin in CXX_BINARIES + RUST_BINARIES:
        ret &= test_rpath_runpath(bin)

    # Check FORTIFY_SOURCE for C++ binaries only
    for bin in CXX_BINARIES:
        ret &= test_fortify_source(bin)

    return ret

def ensure_no_dot_so_in_depends() -> bool:
    """Checks the depends/lib directory for shared libraries (.so files), which should not be present."""
    arch_dir_path = get_arch_dir()
    
    if arch_dir_path is None:
        print("!!! arch-specific build dir not present. Did you build the ./depends tree? !!!")
        return False
    
    lib_dir = Path(arch_dir_path) / 'lib'
    exit_code = 0
    
    if lib_dir.is_dir():
        shared_objects = [lib.name for lib in lib_dir.iterdir() if lib.name.endswith(".so")]

        if shared_objects:
            print("FAIL: Found unexpected shared object (.so) files in depends/lib:")
            for lib in shared_objects:
                print(f"  {lib}")
            exit_code = 1
        else:
            print("PASS: No shared object (.so) files found in depends/lib.")
            exit_code = 0
    else:
        # This case should be mostly covered by the arch_dir_path check, but acts as fallback.
        print("FAIL: lib directory not found under architecture dir.")
        exit_code = 2
        
    return exit_code == 0

# --- Standard Test Runners ---

def util_test() -> bool:
    """Runs the Python utility tests (bitcoin-util-test.py)."""
    return subprocess.call(
        [sys.executable, repofile('src/test/bitcoin-util-test.py')],
        cwd=repofile('src'),
        # Explicitly setting PYTHONPATH and srcdir for the test script environment
        env={'PYTHONPATH': repofile('src/test'), 'srcdir': repofile('src')}
    ) == 0

def rust_test() -> bool:
    """Runs unit and integration tests for Rust components using Cargo."""
    arch_dir = get_arch_dir()
    if arch_dir is None:
        # Error message is already printed in get_arch_dir check (or should be handled better by main)
        return False

    rust_env = os.environ.copy()
    # Configure Cargo to use the toolchain built by 'depends'
    rust_env['RUSTC'] = os.path.join(arch_dir, 'native', 'bin', 'rustc')
    
    cargo_path = os.path.join(arch_dir, 'native', 'bin', 'cargo')
    
    return subprocess.call([
        cargo_path,
        'test',
        '--manifest-path',
        repofile('Cargo.toml'),
    ], env=rust_env) == 0

# --- Test Orchestration ---

# Define the order and mapping of test stages
STAGES = [
    'rust-test',
    'btest',
    'gtest',
    'sec-hard',
    'no-dot-so',
    'util-test',
    'secp256k1',
    'univalue',
    'rpc',
]

STAGE_COMMANDS: Dict[str, Union[Callable[[], bool], List[str]]] = {
    'rust-test': rust_test,
    'btest': [repofile('src/test/test_bitcoin'), '-p'],
    'gtest': [repofile('src/zcash-gtest')],
    'sec-hard': check_security_hardening,
    'no-dot-so': ensure_no_dot_so_in_depends,
    'util-test': util_test,
    'secp256k1': ['make', '-C', repofile('src/secp256k1'), 'check'],
    'univalue': ['make', '-C', repofile('src/univalue'), 'check'],
    'rpc': [repofile('qa/pull-tester/rpc-tests.py')],
}


# --- Test Driver ---

def run_stage(stage: str) -> bool:
    """Executes a single test stage and prints status."""
    print(f'Running stage {stage}')
    print('=' * (len(stage) + 14))
    print()

    cmd = STAGE_COMMANDS[stage]
    
    if isinstance(cmd, list):
        # Command is a list of strings (external program call)
        ret = subprocess.call(cmd) == 0
    else:
        # Command is a callable function (internal Python logic)
        ret = cmd()

    print()
    print('-' * (len(stage) + 15))
    print(f'Finished stage {stage}')
    print()

    return ret

def main():
    """Main entry point for the test execution script."""
    parser = argparse.ArgumentParser(description='Execute Zcash automated tests.')
    parser.add_argument(
        '--list-stages', 
        dest='list', 
        action='store_true',
        help='List all available test stages.'
    )
    parser.add_argument(
        'stage', 
        nargs='*', 
        default=STAGES,
        help=f'One or more stages to run. Defaults to all: {", ".join(STAGES)}'
    )
    args = parser.parse_args()

    # Handle --list-stages
    if args.list:
        print("Available test stages:")
        for s in STAGES:
            print(f"- {s}")
        sys.exit(0)

    # Validate requested stages
    for s in args.stage:
        if s not in STAGE_COMMANDS:
            print(f"Invalid stage '{s}' (choose from {', '.join(STAGES)})")
            sys.exit(1)

    # Run the stages
    all_passed = True
    for s in args.stage:
        passed = run_stage(s)
        if not passed:
            print(f"!!! Stage {s} failed !!!")
        all_passed &= passed

    if not all_passed:
        print("!!! One or more test stages failed !!!")
        sys.exit(1)
        
    print("All requested test stages completed successfully.")

if __name__ == '__main__':
    main()
