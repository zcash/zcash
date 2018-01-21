"""Generates a BUILD.bazel file suitable for ZCash given a proton source
directory. Run this script in the proton source directory. It creates a
BUILD.bazel file there.
"""

import generator_util
import os
import re
import shlex
import subprocess

minimal_build_patch = '''
From 03f5fc0826115edbfca468261b70c0daf627f488 Mon Sep 17 00:00:00 2001
From: Simon <simon@bitcartel.com>
Date: Thu, 27 Apr 2017 17:15:59 -0700
Subject: [PATCH] Enable C++11, build static library and cpp bindings with minimal dependencies.

---
 CMakeLists.txt                            | 13 +++++++------
 examples/cpp/CMakeLists.txt               |  1 +
 proton-c/CMakeLists.txt                   | 32 +++++++++++++++----------------
 proton-c/bindings/CMakeLists.txt          |  6 +++---
 proton-c/bindings/cpp/CMakeLists.txt      | 24 +++++++++++------------
 proton-c/bindings/cpp/docs/CMakeLists.txt |  2 +-
 proton-c/docs/api/CMakeLists.txt          |  2 +-
 7 files changed, 41 insertions(+), 39 deletions(-)

diff --git a/CMakeLists.txt b/CMakeLists.txt
index b538ffd..4a5e787 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -18,14 +18,15 @@
 #
 cmake_minimum_required (VERSION 2.8.7)
 
+set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
 project (Proton C)
 
 # Enable C++ now for examples and bindings subdirectories, but make it optional.
 enable_language(CXX OPTIONAL)
 
 # Enable testing
-enable_testing()
-include (CTest)
+#enable_testing()
+#include (CTest)
 
 # Pull in local cmake modules
 set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/tools/cmake/Modules/")
@@ -141,7 +142,7 @@ set (BINDINGS_DIR ${LIB_INSTALL_DIR}/proton/bindings)
 
 set (SYSINSTALL_BINDINGS OFF CACHE BOOL "If SYSINSTALL_BINDINGS is OFF then proton bindings will be installed underneath ${BINDINGS_DIR} and each user will need to modify their interpreter configuration to load the appropriate binding. If SYSINSTALL_BINDINGS is ON, then each language interpreter will be queried for the appropriate directory and proton bindings will be installed and available system wide with no additional per user configuration.")
 
-set (BINDING_LANGS PERL PHP PYTHON RUBY)
+#set (BINDING_LANGS PERL PHP PYTHON RUBY)
 
 foreach (LANG ${BINDING_LANGS})
   set (SYSINSTALL_${LANG} OFF CACHE BOOL "Install ${LANG} bindings into interpreter specified location.")
@@ -156,10 +157,10 @@ set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton-${PN_VERSION})
 # End of variables used during install
 
 # Check for valgrind here so tests under proton-c/ and examples/ can use it.
-find_program(VALGRIND_EXE valgrind DOC "Location of the valgrind program")
+#find_program(VALGRIND_EXE valgrind DOC "Location of the valgrind program")
 mark_as_advanced (VALGRIND_EXE)
 
-option(ENABLE_VALGRIND "Use valgrind to detect run-time problems" ON)
+#option(ENABLE_VALGRIND "Use valgrind to detect run-time problems" ON)
 if (ENABLE_VALGRIND)
   if (NOT VALGRIND_EXE)
     message(STATUS "Can't locate the valgrind command; no run-time error detection")
@@ -171,7 +172,7 @@ if (ENABLE_VALGRIND)
 endif (ENABLE_VALGRIND)
 
 add_subdirectory(proton-c)
-add_subdirectory(examples)
+#add_subdirectory(examples)
 
 install (FILES LICENSE README.md
          DESTINATION ${PROTON_SHARE})
diff --git a/examples/cpp/CMakeLists.txt b/examples/cpp/CMakeLists.txt
index 304d899..f4877b4 100644
--- a/examples/cpp/CMakeLists.txt
+++ b/examples/cpp/CMakeLists.txt
@@ -17,6 +17,7 @@
 # under the License.
 #
 
+set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
 find_package(ProtonCpp REQUIRED)
 
 include_directories(${ProtonCpp_INCLUDE_DIRS})
diff --git a/proton-c/CMakeLists.txt b/proton-c/CMakeLists.txt
index 8edb661..dc7b99c 100644
--- a/proton-c/CMakeLists.txt
+++ b/proton-c/CMakeLists.txt
@@ -22,24 +22,24 @@ include(CheckSymbolExists)
 
 include(soversion.cmake)
 
-add_custom_target(docs)
-add_custom_target(doc DEPENDS docs)
+#add_custom_target(docs)
+#add_custom_target(doc DEPENDS docs)
 
 # Set the default SSL/TLS implementation
-find_package(OpenSSL)
+#find_package(OpenSSL)
 find_package(PythonInterp REQUIRED)
-find_package(SWIG)
+#find_package(SWIG)
 # FindSwig.cmake "forgets" make its outputs advanced like a good citizen
 mark_as_advanced(SWIG_DIR SWIG_EXECUTABLE SWIG_VERSION)
 
 # See if Cyrus SASL is available
-find_library(CYRUS_SASL_LIBRARY sasl2)
-find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
-find_package_handle_standard_args(CyrusSASL DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)
+#find_library(CYRUS_SASL_LIBRARY sasl2)
+#find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
+#find_package_handle_standard_args(CyrusSASL DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)
 mark_as_advanced(CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)
 
 # Find saslpasswd2 executable to generate test config
-find_program(SASLPASSWD_EXE saslpasswd2 DOC "Program used to make SASL user db for testing")
+#find_program(SASLPASSWD_EXE saslpasswd2 DOC "Program used to make SASL user db for testing")
 mark_as_advanced(SASLPASSWD_EXE)
 
 if(WIN32 AND NOT CYGWIN)
@@ -315,8 +315,8 @@ pn_absolute_install_dir(EXEC_PREFIX "." ${CMAKE_INSTALL_PREFIX})
 pn_absolute_install_dir(LIBDIR ${LIB_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX})
 pn_absolute_install_dir(INCLUDEDIR ${INCLUDE_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX})
 
-add_subdirectory(docs/api)
-add_subdirectory(../tests/tools/apps/c ../tests/tools/apps/c)
+#add_subdirectory(docs/api)
+#add_subdirectory(../tests/tools/apps/c ../tests/tools/apps/c)
 
 # for full source distribution:
 set (qpid-proton-platform-all
@@ -507,7 +507,7 @@ if (BUILD_WITH_CXX)
 endif (BUILD_WITH_CXX)
 
 add_library (
-  qpid-proton-core SHARED
+  qpid-proton-core STATIC
   ${qpid-proton-core}
   ${qpid-proton-layers}
   ${qpid-proton-platform}
@@ -527,7 +527,7 @@ set_target_properties (
   )
 
 add_library(
-  qpid-proton SHARED
+  qpid-proton STATIC
   # Proton Core
   ${qpid-proton-core}
   ${qpid-proton-layers}
@@ -629,7 +629,7 @@ install (FILES
 
 # c tests:
 
-add_subdirectory(src/tests)
+#add_subdirectory(src/tests)
 
 if (CMAKE_SYSTEM_NAME STREQUAL Windows)
   # No change needed for windows already use correct separator
@@ -712,7 +712,7 @@ if (BUILD_PYTHON)
 
 endif (BUILD_PYTHON)
 
-find_program(RUBY_EXE "ruby")
+#find_program(RUBY_EXE "ruby")
 if (RUBY_EXE AND BUILD_RUBY)
   set (rb_root "${pn_test_root}/ruby")
   set (rb_src "${CMAKE_CURRENT_SOURCE_DIR}/bindings/ruby")
@@ -751,8 +751,8 @@ if (RUBY_EXE AND BUILD_RUBY)
   else (DEFAULT_RUBY_TESTING)
     message(STATUS "Skipping Ruby tests: missing dependencies")
   endif (DEFAULT_RUBY_TESTING)
-else (RUBY_EXE)
-  message (STATUS "Cannot find ruby, skipping ruby tests")
+#else (RUBY_EXE)
+#  message (STATUS "Cannot find ruby, skipping ruby tests")
 endif()
 
 mark_as_advanced (RUBY_EXE RSPEC_EXE)
diff --git a/proton-c/bindings/CMakeLists.txt b/proton-c/bindings/CMakeLists.txt
index 6b88384..d1a50a5 100644
--- a/proton-c/bindings/CMakeLists.txt
+++ b/proton-c/bindings/CMakeLists.txt
@@ -19,14 +19,14 @@
 
 # Add bindings that do not require swig here - the directory name must be the same as the binding name
 # See below for swig bindings
-set(BINDINGS javascript cpp go)
+set(BINDINGS cpp)
 
 # Prerequisites for javascript.
 #
 # It uses a C/C++ to JavaScript cross-compiler called emscripten (https://github.com/kripken/emscripten). Emscripten takes C/C++
 # and compiles it into a highly optimisable subset of JavaScript called asm.js (http://asmjs.org/) that can be
 # aggressively optimised and run at near-native speed (usually between 1.5 to 10 times slower than native C/C++).
-find_package(Emscripten)
+#find_package(Emscripten)
 if (EMSCRIPTEN_FOUND)
   set (DEFAULT_JAVASCRIPT ON)
 endif (EMSCRIPTEN_FOUND)
@@ -37,7 +37,7 @@ if (CMAKE_CXX_COMPILER)
 endif (CMAKE_CXX_COMPILER)
 
 # Prerequisites for Go
-find_program(GO_EXE go)
+#find_program(GO_EXE go)
 mark_as_advanced(GO_EXE)
 if (GO_EXE)
   if(WIN32)
diff --git a/proton-c/bindings/cpp/CMakeLists.txt b/proton-c/bindings/cpp/CMakeLists.txt
index 0cc4024..796fe29 100644
--- a/proton-c/bindings/cpp/CMakeLists.txt
+++ b/proton-c/bindings/cpp/CMakeLists.txt
@@ -16,7 +16,7 @@
 # specific language governing permissions and limitations
 # under the License.
 #
-
+set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
 include(cpp.cmake) # Compiler checks
 
 include_directories(
@@ -89,7 +89,7 @@ set_source_files_properties (
   COMPILE_FLAGS "${LTO}"
   )
 
-add_library(qpid-proton-cpp SHARED ${qpid-proton-cpp-source})
+add_library(qpid-proton-cpp STATIC ${qpid-proton-cpp-source})
 
 target_link_libraries (qpid-proton-cpp ${PLATFORM_LIBS} qpid-proton)
 
@@ -120,8 +120,8 @@ endif (MSVC)
 
 install (DIRECTORY "include/proton" DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")
 
-add_subdirectory(docs)
-add_subdirectory(${CMAKE_SOURCE_DIR}/tests/tools/apps/cpp ${CMAKE_BINARY_DIR}/tests/tools/apps/cpp)
+#add_subdirectory(docs)
+#add_subdirectory(${CMAKE_SOURCE_DIR}/tests/tools/apps/cpp ${CMAKE_BINARY_DIR}/tests/tools/apps/cpp)
 
 # Pkg config file
 configure_file(
@@ -171,12 +171,12 @@ macro(add_cpp_test test)
   endif ()
 endmacro(add_cpp_test)
 
-add_cpp_test(codec_test)
+#add_cpp_test(codec_test)
 #add_cpp_test(engine_test)
-add_cpp_test(thread_safe_test)
-add_cpp_test(interop_test ${CMAKE_SOURCE_DIR}/tests)
-add_cpp_test(message_test)
-add_cpp_test(scalar_test)
-add_cpp_test(value_test)
-add_cpp_test(container_test)
-add_cpp_test(url_test)
+#add_cpp_test(thread_safe_test)
+#add_cpp_test(interop_test ${CMAKE_SOURCE_DIR}/tests)
+#add_cpp_test(message_test)
+#add_cpp_test(scalar_test)
+#add_cpp_test(value_test)
+#add_cpp_test(container_test)
+#add_cpp_test(url_test)
diff --git a/proton-c/bindings/cpp/docs/CMakeLists.txt b/proton-c/bindings/cpp/docs/CMakeLists.txt
index d512d15..8576867 100644
--- a/proton-c/bindings/cpp/docs/CMakeLists.txt
+++ b/proton-c/bindings/cpp/docs/CMakeLists.txt
@@ -17,7 +17,7 @@
 # under the License.
 #
 
-find_package(Doxygen)
+#find_package(Doxygen)
 
 if (DOXYGEN_FOUND)
   configure_file (
diff --git a/proton-c/docs/api/CMakeLists.txt b/proton-c/docs/api/CMakeLists.txt
index 7756e48..71ebb93 100644
--- a/proton-c/docs/api/CMakeLists.txt
+++ b/proton-c/docs/api/CMakeLists.txt
@@ -17,7 +17,7 @@
 # under the License.
 #
 
-find_package(Doxygen)
+#find_package(Doxygen)
 if (DOXYGEN_FOUND)
   configure_file (${CMAKE_CURRENT_SOURCE_DIR}/user.doxygen.in
                   ${CMAKE_CURRENT_BINARY_DIR}/user.doxygen)
-- 
2.7.4

'''

proton_cmake_opts = [
    "-DCMAKE_CXX_STANDARD=11",
    "-DCMAKE_INSTALL_PREFIX=/",
    "-DSYSINSTALL_BINDINGS=ON",
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
    "-DBUILD_PYTHON=OFF",
    "-DBUILD_PHP=OFF",
    "-DBUILD_JAVA=OFF",
    "-DBUILD_PERL=OFF",
    "-DBUILD_RUBY=OFF",
    "-DBUILD_JAVASCRIPT=OFF",
    "-DBUILD_GO=OFF",
]

patch_filename = "minimal_build.patch"
if not os.path.isfile(patch_filename):
    # Use existence of the patch file as a proxy to guess if the patch has
    # already been applied.
    generator_util.write_file(patch_filename, minimal_build_patch)
    subprocess.call("patch -p1 < %s" % patch_filename, shell = True)

build_dir = "zcash_build"
subprocess.call(["rm", "-r", build_dir])
subprocess.call(["mkdir", "-p", build_dir])
with generator_util.cd(build_dir):
    subprocess.call(["cmake", ".."] + proton_cmake_opts)
    subprocess.call(["mkdir", "-p", "proton-c/src"])
    # Generate encodings.h and protocol.h
    subprocess.call(["make", "generated_c_files"])

generated_public_files = [
    "proton-c/include/proton/version.h",
]
generated_private_files = [
    "proton-c/src/encodings.h",
    "proton-c/src/protocol.h",
]
generated_files = generated_public_files + generated_private_files

# TODO(per-gron): This is not very nice; it assumes that the package is
# being imported with a given name.
external_dir = "external/proton/"

libraries = {
    "qpid-proton-core": {
        "deps": [],
        "public": False,
        "dir": "proton-c",
        "includes": ["proton-c/include"],
        "cflags": [
            "-I%sproton-c/src" % external_dir,
            "-I$(GENDIR)/%sproton-c/src" % external_dir,
        ],
    },
    "qpid-proton": {
        "deps": [":qpid-proton-core"],
        "public": True,
        "dir": "proton-c",
        "includes": [],
        "cflags": [
            "-I%sproton-c/src" % external_dir,
            "-I$(GENDIR)/%sproton-c/src" % external_dir,
        ],
    },
    "qpid-proton-cpp": {
        "deps": [":qpid-proton", ":qpid-proton-core"],
        "public": True,
        "dir": "proton-c/bindings/cpp",
        "includes": ["proton-c/bindings/cpp/include"],
        "cflags": [
            "-I%sproton-c/bindings/cpp/src/include" % external_dir,
        ]
    },
}

extra_cflags = [
    "-DUSE_ATOLL",
    "-DUSE_CLOCK_GETTIME",
    "-DUSE_STRERROR_R",
    "-Dqpid_proton_EXPORTS",
    "-Dqpid_proton_cpp_EXPORTS",
]

def process_library(name, lib_descriptor):
    rule = ""
    lib_dir = lib_descriptor["dir"]

    def obj_to_src(obj):
        prefix = "CMakeFiles/%s.dir/" % (name)
        suffix = ".o"
        if not obj.startswith(prefix) or not obj.endswith(suffix):
            raise Exception("Unexpected object path %s" % obj)
        src = os.path.join(lib_dir, obj[len(prefix):-len(suffix)])
        if not os.path.isfile(src):
            raise Exception("File %s for object file %s does not exist" % (src, obj))
        return src

    makefiles_dir = "%s/%s/CMakeFiles/%s.dir" % (build_dir, lib_dir, name)
    cflags_str = generator_util.extract_variable_from_makefile(
        "$(CXX_FLAGS) $(C_FLAGS) $(CXX_DEFINES) $(C_DEFINES)",
        makefile = "%s/flags.make" % makefiles_dir,
        cwd = build_dir)
    cflags = shlex.split(cflags_str)
    cflags = [flag for flag in cflags if flag != "-g" and not re.match(r"^-O\d$", flag)]

    objs = shlex.split(generator_util.extract_variable_from_makefile(
        "$(%s_OBJECTS)" % name.replace("-", "__"),
        makefile = "%s/build.make" % makefiles_dir,
        cwd = build_dir))
    srcs = [obj_to_src(obj) for obj in objs]

    public_generated_headers = []
    if name == "qpid-proton":
        public_generated_headers = generated_public_files

    rule += "cc_library(\n"
    if lib_descriptor["public"]:
        rule += '    visibility = ["//visibility:public"],\n'
    rule += "    name = '%s',\n" % name
    rule += "    deps = %s,\n" % lib_descriptor["deps"]
    rule += "    includes = %s,\n" % lib_descriptor["includes"]
    rule += "    copts = %s,\n" % (cflags + extra_cflags + lib_descriptor["cflags"])
    rule += "    srcs = %s + %s + glob(['%s/**/*.h', '%s/**/*.hpp']) + %s,\n" % (srcs, generated_private_files, lib_dir, lib_dir, generated_public_files)
    if lib_descriptor["public"]:
        rule += "    hdrs = glob(['%s/include/**/*.h', '%s/include/**/*.hpp']) + %s,\n" % (lib_dir, lib_dir, public_generated_headers)
    rule += ")\n\n"

    return rule

def process_libraries():
    res = ""
    for name in libraries:
        res += process_library(name, libraries[name])
    return res

def process_generated_files():
    res = ""
    for generated_file in generated_files:
        res += generator_util.copy_file_genrule(
            generated_file,
            generator_util.read_file(os.path.join(build_dir, generated_file)))
    return res

build_file = generator_util.build_header()
build_file += process_libraries()
build_file += process_generated_files()

generator_util.write_file("BUILD.bazel", build_file)
