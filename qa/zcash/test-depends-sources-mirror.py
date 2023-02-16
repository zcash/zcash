#!/usr/bin/env python3

# This script tests that the package mirror at https://download.z.cash/depends-sources/
# contains all of the packages required to build this version of Zcash.
#
# This script assumes you've just built Zcash, and that as a result of that
# build, all of the dependency packages have been downloaded into the
# depends/sources directory (inside the root of this repository). The script
# checks that all of those files are accessible on the mirror.

import sys
import os
import requests

MIRROR_URL_DIR = "https://download.z.cash/depends-sources/"
DEPENDS_SOURCES_DIR = os.path.realpath(os.path.join(
    os.path.dirname(__file__),
    "..", "..", "depends", "sources"
))

def get_depends_sources_list():
    return filter(os.path.isfile, [os.path.join(DEPENDS_SOURCES_DIR, f) for f in os.listdir(DEPENDS_SOURCES_DIR)])

passing = True
for path in get_depends_sources_list():
    filename = os.path.basename(path)
    print("Checking [" + filename + "] ...")

    resp = requests.head(MIRROR_URL_DIR + filename)
    if resp.status_code != 200:
        print("FAIL. File %s not found on server (status code %d)" % (filename, resp.status_code))
        passing = False
        continue

    expected_size = os.path.getsize(path)
    server_size = int(resp.headers['Content-Length'])
    if expected_size != server_size:
        print("FAIL. On the server, %s is %d bytes, but locally it is %d bytes." % (filename, server_size, expected_size))
        passing = False
        continue

    with open(path, 'rb') as f:
        expected_content = f.read()

    if resp.content != expected_content:
        print("FAIL. The server and local files for %s have the same length but different contents" % (filename,))
        passing = False
        continue

if passing: print("PASS.")
sys.exit(0 if passing else 1)
