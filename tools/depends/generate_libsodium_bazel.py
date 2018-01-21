"""Generates a BUILD.bazel file suitable for ZCash given a libsodium source
directory. Run this script in the libsodium source directory. It creates a
BUILD.bazel file there.
"""

import generator_util
import os
import re
import shlex
import subprocess

libsodium_config_opts = [
    "--enable-static",
    "--disable-shared",
]

subprocess.call(["./configure"] + libsodium_config_opts)

libraries = [
    "sodium",
    "avx512f",
    "avx2",
    "sse41",
    "ssse3",
    "sse2",
    "aesni",
]

# TODO(per-gron): This is not very nice; it assumes that the package is
# being imported with a given name.
external_dir = "external/libsodium/"

extra_cflags = [
    "-I%ssrc/libsodium/crypto_generichash/blake2b/ref" % external_dir,
    "-I%ssrc/libsodium/crypto_pwhash/argon2" % external_dir,
    "-I$(GENDIR)/%ssrc/libsodium/include/sodium" % external_dir,
    "-Wno-unknown-pragmas",
]

makefile = "src/libsodium/Makefile"

cflags_str = generator_util.extract_variable_from_makefile(
    "$(DEFS) $(libaesni_la_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)",
    makefile)
cflags = shlex.split(cflags_str)
# Remove flags that are the task of the CROSSTOOL to decide if/when to apply.
flags_to_remove = ["-g", "-fPIC", "-fPIE", "-fstack-protector", "-fno-strict-aliasing"]
cflags = ["'%s'" % flag for flag in cflags if flag not in flags_to_remove and not re.match(r"^-O\d$", flag) and not re.match(r"^-D_FORTIFY_SOURCE=", flag)]

version_h = "src/libsodium/include/sodium/version.h"

def process_libraries():
    rule = ""

    def obj_to_src(lib_name, obj):
        without_ext = "src/libsodium/" + re.sub(r"/lib" + lib_name + r"_la-(.*)\.lo$", r"/\1", obj)
        for ext in [".S", ".c"]:
            candidate = "%s%s" % (without_ext, ext)
            if os.path.isfile(candidate):
                return candidate
        raise "Could not find file for %s" % without_ext

    srcs = set([version_h])
    for lib in libraries:
        objs = shlex.split(generator_util.extract_variable_from_makefile(
            "$(lib%s_la_OBJECTS)" % lib, makefile))
        for obj in objs:
            srcs.add(obj_to_src(lib, obj))

    rule += "cc_library(\n"
    rule += '    visibility = ["//visibility:public"],\n'
    rule += '    hdrs = headers,\n'
    rule += "    name = 'sodium',\n"
    rule += "    copts = cflags,\n"
    rule += "    includes = ['src/libsodium/include', 'src/libsodium/include/sodium'],\n"
    rule += "    srcs = %s + headers,\n" % list(srcs)
    rule += "    textual_hdrs = assembly_files,\n"
    rule += ")\n\n"

    return rule

build_file = generator_util.build_header()
build_file += "cflags = %s\n" % (cflags + extra_cflags)
build_file += "headers = glob(['src/**/*.h'])\n"
build_file += "assembly_files = glob(['src/**/*.S'])\n\n"
build_file += process_libraries()
build_file += generator_util.copy_file_genrule(
    version_h, generator_util.read_file(version_h))

generator_util.write_file("BUILD.bazel", build_file)
