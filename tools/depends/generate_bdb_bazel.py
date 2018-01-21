"""Generates a BUILD.bazel file suitable for ZCash given a bdb source
directory. Run this script in the bdb source directory. It creates a
BUILD.bazel file there.
"""
import os
import subprocess
import pprint
import generator_util

bdb_config_opts = [
    "--disable-shared",
    "--enable-cxx",
    "--disable-replication",
    "--with-pic",
]

os.chdir("build_unix")
subprocess.call(["../dist/configure"] + bdb_config_opts)

c_objs = generator_util.extract_variable_from_makefile("$(C_OBJS)")
cxx_objs = generator_util.extract_variable_from_makefile("$(CXX_OBJS)")

generated_headers_dir = 'genheaders'
generated_headers_c = {
    'db_int.h': generator_util.read_file('db_int.h'),
    'db.h': generator_util.read_file('db.h'),
    'clib_port.h': r"""
#include <limits.h>

#define INT64_FMT   "%ld"
#define UINT64_FMT  "%lu"
""",
    'db_config.h': generator_util.read_file('db_config.h'),
}
generated_headers_cxx = {
    'db_cxx.h': generator_util.read_file('db_cxx.h'),
}

all_files = {}
for file in subprocess.check_output(["find", ".."]).split("\n"):
    (name, ext) = os.path.splitext(os.path.basename(file))
    if (ext == '.c' or ext == '.cpp') and ('os_windows' not in file and 'os_qnx' not in file and 'os_vxworks' not in file):
        all_files[name] = file.replace('../', '')

def obj_to_path(obj):
    name = obj.replace(".o", "").strip()
    return all_files[name]

def process_lib(lib_name, objs, hdrs, generated_headers, deps):
    srcs = [obj_to_path(obj) for obj in objs.split(" ")]    
    header_paths = ["%s/%s" % (generated_headers_dir, header) for header in generated_headers.keys()]

    rule = ""
    rule += "cc_library(\n"
    rule += "    name = '%s',\n" % lib_name
    rule += "    copts = cflags,\n"
    rule += "    linkopts = lflags,\n"
    rule += "    visibility = ['//visibility:public'],\n"
    rule += "    includes = ['%s'],\n" % generated_headers_dir
    rule += "    hdrs = %s + %s,\n" % (pprint.pformat(header_paths), hdrs)
    rule += "    srcs = %s,\n" % pprint.pformat(srcs)
    rule += "    deps = %s,\n" % pprint.pformat(deps)
    rule += ")\n\n"

    return rule

# TODO(per-gron): This is not very nice; it assumes that the package is
# being imported with a given name.
external_dir = "external/bdb/"

build_file = generator_util.build_header()
build_file += """
cflags = [
    "-D_GNU_SOURCE",
    "-D_REENTRANT",
    "-I{{EXTERNAL_DIR}}src",
    "-Wno-unused-but-set-variable",
    "-Wno-strict-aliasing",
    "-Wno-maybe-uninitialized",
]
lflags = ["-lpthread"]

""".replace("{{EXTERNAL_DIR}}", external_dir)

def genheaders(generated_headers):
    res = ""
    for generated_header in generated_headers:
        res += generator_util.copy_file_genrule(generated_headers_dir + "/" + generated_header, generated_headers[generated_header])
    return res

build_file += genheaders(generated_headers_c)
build_file += genheaders(generated_headers_cxx)
build_file += process_lib("db", c_objs, 'glob(["src/**/*.h", "src/**/*.incl"])', generated_headers_c, [])
build_file += process_lib("db_cxx", cxx_objs, '[]', generated_headers_cxx, [":db"])

generator_util.write_file("../BUILD.bazel", build_file)
