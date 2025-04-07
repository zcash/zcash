#!/usr/bin/env python
# Copyright (c) 2014 Wladimir J. van der Laan
# Copyright (c) 2016-2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .
'''
A script to check that the (Linux) executables produced by gitian only contain
allowed gcc and glibc version symbols.  This makes sure they are still compatible
with the minimum supported Linux distribution versions.

Example usage:

    find ../gitian-builder/build -type f -executable | xargs python contrib/devtools/symbol-check.py
'''
from __future__ import division, print_function, unicode_literals
import subprocess
import re
import sys
import os

# Ubuntu 20.04 LTS (Focal Fossa; End of Support April 2025) has:
#
# - g++ version 9.3.0 (https://packages.ubuntu.com/search?suite=all&searchon=names&keywords=g%2B%2B)
# - libc6 version 2.31 (https://packages.ubuntu.com/search?suite=all&searchon=names&keywords=libc6)
#
# Debian 10 (Buster; LTS EOL June 2024) has:
#
# - g++ version 8.3.0 (https://packages.debian.org/search?suite=default&section=all&arch=any&searchon=names&keywords=g%2B%2B)
# - libc6 version 2.28 (https://packages.debian.org/search?suite=default&section=all&arch=any&searchon=names&keywords=libc6)
#
# RedHat Enterprise Linux 8 (EOL: long and complicated) is based on Fedora 28 (EOL 2019-05-28) and uses the same base packages:
#
# - g++ version 8.0.1 (https://archives.fedoraproject.org/pub/archive/fedora/linux/releases/28/Everything/x86_64/os/Packages/g/ search for gcc-)
# - libc6 version 2.27 (https://archives.fedoraproject.org/pub/archive/fedora/linux/releases/28/Everything/x86_64/os/Packages/g/ search for glibc)
#
# Fedora 31 (EOL ~November 2020) has:
#
# - g++ version 9.2.1 (https://dl.fedoraproject.org/pub/fedora/linux/releases/31/Everything/x86_64/os/Packages/g/ search for gcc-)
# - libc6 version 2.30 (https://dl.fedoraproject.org/pub/fedora/linux/releases/31/Everything/x86_64/os/Packages/g/ search for glibc)
#
# Arch is a rolling release, and as of October 2020 has packages for:
#
# - g++ version 8.4.0 / 9.3.0 / 10.2.0 (https://www.archlinux.org/packages/?q=gcc)
# - libc6 version 2.32 (https://www.archlinux.org/packages/?q=glibc)
#
# We take the minimum of these as our target. In practice, if we build on Buster without
# upgrading GCC or libc, then we should get a binary that works for all these systems, and
# later ones.
#
# According to the GNU ABI document (https://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html) this corresponds to:
#   GCC 8.0.0: GCC_8.0.0, GLIBCXX_3.4.24, CXXABI_1.3.11
#   libc6:     GLIBC_2_27

# We statically link libc++ and libc++abi in our builds. Set this to allow dynamic linking to libstdc++.
ALLOW_DYNAMIC_LIBSTDCXX = False

MAX_VERSIONS = {
    b'GCC':   (8,0,0),
    b'GLIBC': (2,27),
}
if ALLOW_DYNAMIC_LIBSTDCXX:
    MAX_VERSIONS.update({
        b'GLIBCXX': (3,4,14),
        b'CXXABI':  (1,3,4),
    })

# See here for a description of _IO_stdin_used:
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=634261#109

# Ignore symbols that are exported as part of every executable
IGNORE_EXPORTS = {
    b'_edata', b'_end', b'_init', b'__bss_start', b'_fini', b'_IO_stdin_used'
}
READELF_CMD = os.getenv('READELF', '/usr/bin/readelf')
CPPFILT_CMD = os.getenv('CPPFILT', '/usr/bin/c++filt')

# Allowed NEEDED libraries
ALLOWED_LIBRARIES = {
    # zcashd
    b'libgcc_s.so.1',        # GCC support library (also used by clang)
    b'libc.so.6',            # C library
    b'libpthread.so.0',      # threading
    b'libanl.so.1',          # DNS resolver
    b'libm.so.6',            # math library
    b'librt.so.1',           # real-time (POSIX compatibility)
    b'ld-linux-x86-64.so.2', # 64-bit dynamic linker
    b'ld-linux.so.2',        # 32-bit dynamic linker
    b'libdl.so.2'            # programming interface to dynamic linker
}
if ALLOW_DYNAMIC_LIBSTDCXX:
    ALLOWED_LIBRARIES.add(b'libstdc++.so.6')  # C++ standard library


def escape(s):
    return s.decode('us-ascii', 'backslashreplace')

class CPPFilt(object):
    '''
    Demangle C++ symbol names.

    Use a pipe to the 'c++filt' command.
    '''
    def __init__(self):
        self.proc = subprocess.Popen(CPPFILT_CMD, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def __call__(self, mangled):
        self.proc.stdin.write(mangled + b'\n')
        self.proc.stdin.flush()
        return self.proc.stdout.readline().rstrip()

    def close(self):
        self.proc.stdin.close()
        self.proc.stdout.close()
        self.proc.wait()

def read_symbols(executable, imports=True):
    '''
    Parse an ELF executable and return a list of (symbol,version) tuples
    for dynamic, imported symbols.
    '''
    p = subprocess.Popen([READELF_CMD, '--dyn-syms', '-W', executable], stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
    (stdout, stderr) = p.communicate()
    if p.returncode:
        raise IOError(u"Could not read symbols for %s: %s" % (executable, escape(stderr.strip())))
    syms = []
    for line in stdout.split(b'\n'):
        line = line.split()
        if len(line)>7 and re.match(b'[0-9]+:$', line[0]):
            (sym, _, version) = line[7].partition(b'@')
            is_import = line[6] == b'UND'
            if version.startswith(b'@'):
                version = version[1:]
            if is_import == imports:
                syms.append((sym, version))
    return syms

def check_version(max_versions, version):
    if b'_' in version:
        (lib, _, ver) = version.rpartition(b'_')
    else:
        lib = version
        ver = '0'
    ver = tuple([int(x) for x in ver.split(b'.')])
    if not lib in max_versions:
        return False
    return ver <= max_versions[lib]

def read_libraries(filename):
    p = subprocess.Popen([READELF_CMD, '-d', '-W', filename], stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
    (stdout, stderr) = p.communicate()
    if p.returncode:
        raise IOError('Error opening file')
    libraries = []
    for line in stdout.split(b'\n'):
        tokens = line.split()
        if len(tokens)>2 and tokens[1] == b'(NEEDED)':
            match = re.match(b'^Shared library: \[(.*)\]$', b' '.join(tokens[2:]))
            if match:
                libraries.append(match.group(1))
            else:
                raise ValueError('Unparseable (NEEDED) specification')
    return libraries

if __name__ == '__main__':
    cppfilt = CPPFilt()
    retval = 0
    for filename in sys.argv[1:]:
        print(u"Checking %s..." % (filename,))
        # Check imported symbols
        for sym,version in read_symbols(filename, True):
            if version and not check_version(MAX_VERSIONS, version):
                print(u"%s: symbol %s from unsupported version %s" % (filename, escape(cppfilt(sym)), escape(version)))
                retval = 1
        # Check exported symbols
        for sym,version in read_symbols(filename, False):
            if sym in IGNORE_EXPORTS:
                continue
            print(u"%s: export of symbol %s not allowed" % (filename, escape(cppfilt(sym))))
            retval = 1
        # Check dependency libraries
        for library_name in read_libraries(filename):
            if library_name not in ALLOWED_LIBRARIES:
                print(u"%s: NEEDED library %s is not allowed" % (filename, escape(library_name)))
                retval = 1
        print()

    if retval == 0:
        print("Everything OK")
    else:
        print("Note: this script is intended to ensure that Gitian builds meet our compatibility policy.")
        print("The above warnings do not necessarily mean the program(s) will not work on your system.")

    exit(retval)
