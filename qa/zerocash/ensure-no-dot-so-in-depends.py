#! /usr/bin/env python2

import sys
import os

def main():
    this_script = os.path.abspath(sys.argv[0])
    basedir = os.path.dirname(this_script)
    lib_folder = os.path.join(
        basedir,
        '..',
        '..',
        'depends',
        'x86_64-unknown-linux-gnu',
        'lib'
    )

    libraries = os.listdir(lib_folder)

    exit_code = 0

    for lib in libraries:
        if lib.find(".so") != -1:
            print lib
            exit_code = 1

    if exit_code == 0:
        print "PASS."
    else:
        print "FAIL."

    sys.exit(exit_code)

if __name__ == '__main__':
    main()
