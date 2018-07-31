#!/usr/bin/env python2

# This script tests that the package mirror at https://z.cash/depends-sources/
# contains all of the packages required to build this version of Zcash.
#
# This script assumes you've just built Zcash, and that as a result of that
# build, all of the dependency packages have been downloaded into the
# depends/sources directory (inside the root of this repository). The script
# checks that all of those files are accessible on the mirror.

import sys
import os
import requests

MIRROR_URL_DIR="https://z.cash/depends-sources/"
DEPENDS_SOURCES_DIR=os.path.realpath(os.path.join(
    os.path.dirname(__file__),
    "..", "..", "depends", "sources"
))

def get_depends_sources_list():
    return filter(
	lambda f: os.path.isfile(os.path.join(DEPENDS_SOURCES_DIR, f)),
	os.listdir(DEPENDS_SOURCES_DIR)
    )

for filename in get_depends_sources_list():
    resp = requests.head(MIRROR_URL_DIR + filename)

    print "Checking [" + filename + "] ..."

    if resp.status_code != 200:
	print "FAIL. File not found on server: " + filename
	sys.exit(1)

    expected_size = os.path.getsize(os.path.join(DEPENDS_SOURCES_DIR, filename))
    server_size = int(resp.headers['Content-Length'])
    if expected_size != server_size:
	print "FAIL. On the server, %s is %d bytes, but locally it is %d bytes." % (filename, server_size, expected_size)
	sys.exit(1)

print "PASS."
sys.exit(0)