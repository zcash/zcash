#! /usr/bin/env python2

import sys
import os

def main():
    this_script = os.path.abspath(sys.argv[0])
    basedir = os.path.dirname(this_script)
    arch_dir = os.path.join(
        basedir,
        '..',
        '..',
        'depends',
        'x86_64-unknown-linux-gnu',
    )

    exit_code = 0

    if os.path.isdir(arch_dir):
        lib_dir = os.path.join(arch_dir, 'lib')
        libraries = os.listdir(lib_dir)

        for lib in libraries:
            if lib.find(".so") != -1:
                print lib
                exit_code = 1
    else:
        exit_code = 2
        print "arch-specific build dir not present: {}".format(arch_dir)
        print "Did you build the ./depends tree?"
        print "Are you on a currently unsupported architecture?"

    if exit_code == 0:
        print "PASS."
    else:
        print "FAIL."

    sys.exit(exit_code)

if __name__ == '__main__':
    main()
