#! /usr/bin/env python2

import os
import re
import sys
import subprocess
from git_diff import check_valid, BrokenPatch

base_dir = os.path.dirname(os.path.abspath(__file__)) + "/diff-tests/"

def get_immediate_subdirectories(a_dir):
    return [name for name in os.listdir(a_dir)
            if os.path.isdir(os.path.join(a_dir, name))]

for test in get_immediate_subdirectories(base_dir):
    path_to_test_root = base_dir + test + "/"

    os.chdir(path_to_test_root)
    diff = subprocess.Popen(('diff', '-burN', "a/", "b/"), stdout=subprocess.PIPE)
    output, err = diff.communicate()

    output = re.sub(r"\+\+\+ .*1969.*", r"+++ /dev/null", output)
    output = re.sub(r"diff -burN a/(.*?) b/(.*?)", r"", output)
    output = re.sub(r"--- a/(.*?)\t.*", r"--- \1", output)
    output = re.sub(r"\+\+\+ b/(.*?)\t.*", r"+++ \1", output)

    with open ("expected", "r") as expected:
        expected=expected.read().replace('\n', '')

        try:
            check_valid(output)
        except BrokenPatch as e:
            if e.reason != expected:
                print output
                print "[git_diff test] '%s' did not pass expectations." % test
                print "[git_diff test] expected reason: %s" % expected
                print "[git_diff test] we got instead: %s" % e.reason
                print "[git_diff test] full exception: %s" % e
                sys.exit(1)
        else:
            if expected != "OK":
                print output
                print "[git_diff test] '%s' did not pass expectations." % test
                print "[git_diff test] Test was expected to fail with %s" % expected
                print "[git_diff test] Instead it succeeded"
                sys.exit(1)

changes=subprocess.check_output(["git", "diff", "cf33f196e79b1e61d6266f8e5190a0c4bfae7224"])
check_valid(changes)
