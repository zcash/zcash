#********************************************************************************
# Makefile for the libsnark library.
#********************************************************************************
#* @author     This file is part of libsnark, developed by SCIPR Lab
#*             and contributors (see AUTHORS).
#* @copyright  MIT license (see LICENSE file)
#*******************************************************************************/

# Determine building operating system
ifeq ($(OS),Windows_NT)
    uname_S := Windows
else
    uname_S := $(shell uname -s)
endif

# To override these, use "make OPTFLAGS=..." etc.
CURVE = BN128
OPTFLAGS = -O2 -march=x86-64 -g -mtune=x86-64

ifneq ($(PLATFORM),darwin)
	FEATUREFLAGS = -DUSE_ASM -DMONTGOMERY_OUTPUT
else
	FEATUREFLAGS = -DUSE_ASM -DMONTGOMERY_OUTPUT -D__SIZE_TYPE__="unsigned long long"
endif

# Initialize this using "CXXFLAGS=... make". The makefile appends to that.
CXXFLAGS += -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wno-comment -Wfatal-errors $(OPTFLAGS) $(FEATUREFLAGS) -DCURVE_$(CURVE)

DEPSRC = depsrc
DEPINST = depinst

CXXFLAGS += -I$(DEPINST)/include -Ilibsnark
LDFLAGS += -L$(DEPINST)/lib -Wl,-rpath,$(DEPINST)/lib
LDLIBS += -lgmpxx -lgmp -lboost_program_options -lsodium
# List of .a files to include within libsnark.a and libsnark.so:
AR_LIBS =
# List of library files to install:
INSTALL_LIBS = $(LIB_FILE)
# Sentinel file to check existence of this directory (since directories don't work as a Make dependency):
DEPINST_EXISTS = $(DEPINST)/.exists

ifneq ($(NO_GTEST),1)
	# Compile GTest from sourcecode if we can (e.g., Ubuntu). Otherwise use precompiled one (e.g., Fedora).
	# See https://github.com/google/googletest/blob/master/googletest/docs/FAQ.md#why-is-it-not-recommended-to-install-a-pre-compiled-copy-of-google-test-for-example-into-usrlocal
	ifneq ($(NO_COMPILE_LIBGTEST),1)
		GTESTDIR=/usr/src/gtest
		COMPILE_LIBGTEST = $(shell test -d $(GTESTDIR) && echo -n 1)
	endif
	GTEST_LDLIBS += -lgtest -lpthread
endif

ifneq ($(NO_SUPERCOP),1)
	SUPERCOP_LDLIBS += -lsupercop
	INSTALL_LIBS += depinst/lib/libsupercop.a
	# Would have been nicer to roll supercop into libsnark.a ("AR_LIBS += $(DEPINST)/lib/libsupercop.a"), but it doesn't support position-independent code (libsnark issue #20).
endif

LIB_SRCS = \
	libsnark/algebra/curves/alt_bn128/alt_bn128_g1.cpp \
	libsnark/algebra/curves/alt_bn128/alt_bn128_g2.cpp \
	libsnark/algebra/curves/alt_bn128/alt_bn128_init.cpp \
	libsnark/algebra/curves/alt_bn128/alt_bn128_pairing.cpp \
	libsnark/algebra/curves/alt_bn128/alt_bn128_pp.cpp \
	libsnark/common/profiling.cpp \
	libsnark/common/utils.cpp \
	libsnark/gadgetlib1/constraint_profiling.cpp \

ifeq ($(CURVE),BN128)
	LIB_SRCS += \
		libsnark/algebra/curves/bn128/bn128_g1.cpp \
		libsnark/algebra/curves/bn128/bn128_g2.cpp \
		libsnark/algebra/curves/bn128/bn128_gt.cpp \
		libsnark/algebra/curves/bn128/bn128_init.cpp \
		libsnark/algebra/curves/bn128/bn128_pairing.cpp \
		libsnark/algebra/curves/bn128/bn128_pp.cpp

	CXXFLAGS += -DBN_SUPPORT_SNARK
	AR_LIBS += $(DEPINST)/lib/libzm.a
endif

# FIXME: most of these are broken due to removed code.
DISABLED_EXECUTABLES = \
	libsnark/common/routing_algorithms/profiling/profile_routing_algorithms \
	libsnark/common/routing_algorithms/tests/test_routing_algorithms \
	libsnark/gadgetlib1/gadgets/cpu_checkers/fooram/examples/test_fooram \
	libsnark/gadgetlib1/gadgets/hashes/knapsack/tests/test_knapsack_gadget \
	libsnark/gadgetlib1/gadgets/routing/profiling/profile_routing_gadgets \
	libsnark/gadgetlib1/gadgets/set_commitment/tests/test_set_commitment_gadget \
	libsnark/gadgetlib1/gadgets/verifiers/tests/test_r1cs_ppzksnark_verifier_gadget \
	libsnark/reductions/ram_to_r1cs/examples/demo_arithmetization \
	libsnark/relations/arithmetic_programs/ssp/tests/test_ssp \
	libsnark/zk_proof_systems/pcd/r1cs_pcd/r1cs_mp_ppzkpcd/profiling/profile_r1cs_mp_ppzkpcd \
	libsnark/zk_proof_systems/pcd/r1cs_pcd/r1cs_mp_ppzkpcd/tests/test_r1cs_mp_ppzkpcd \
	libsnark/zk_proof_systems/pcd/r1cs_pcd/r1cs_sp_ppzkpcd/profiling/profile_r1cs_sp_ppzkpcd \
	libsnark/zk_proof_systems/pcd/r1cs_pcd/r1cs_sp_ppzkpcd/tests/test_r1cs_sp_ppzkpcd \
	libsnark/zk_proof_systems/ppzksnark/bacs_ppzksnark/profiling/profile_bacs_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/bacs_ppzksnark/tests/test_bacs_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/r1cs_gg_ppzksnark/profiling/profile_r1cs_gg_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/r1cs_gg_ppzksnark/tests/test_r1cs_gg_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/profiling/profile_r1cs_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/ram_ppzksnark/examples/demo_ram_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/ram_ppzksnark/examples/demo_ram_ppzksnark_generator \
	libsnark/zk_proof_systems/ppzksnark/ram_ppzksnark/examples/demo_ram_ppzksnark_prover \
	libsnark/zk_proof_systems/ppzksnark/ram_ppzksnark/examples/demo_ram_ppzksnark_verifier \
	libsnark/zk_proof_systems/ppzksnark/ram_ppzksnark/profiling/profile_ram_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/ram_ppzksnark/tests/test_ram_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/tbcs_ppzksnark/profiling/profile_tbcs_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/tbcs_ppzksnark/tests/test_tbcs_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/uscs_ppzksnark/profiling/profile_uscs_ppzksnark \
	libsnark/zk_proof_systems/ppzksnark/uscs_ppzksnark/tests/test_uscs_ppzksnark \
	libsnark/zk_proof_systems/zksnark/ram_zksnark/profiling/profile_ram_zksnark \
	libsnark/zk_proof_systems/zksnark/ram_zksnark/tests/test_ram_zksnark

EXECUTABLES =

EXECUTABLES_WITH_GTEST =

EXECUTABLES_WITH_SUPERCOP = \
	libsnark/zk_proof_systems/ppzkadsnark/r1cs_ppzkadsnark/examples/demo_r1cs_ppzkadsnark

GTEST_TESTS =

GTEST_SRCS =

DOCS = README.html

LIBSNARK_A = libsnark.a

# For documentation of the following options, see README.md .

ifeq ($(NO_PROCPS),1)
	CXXFLAGS += -DNO_PROCPS
else
	LDLIBS += -lprocps
endif

ifeq ($(LOWMEM),1)
	CXXFLAGS += -DLOWMEM
endif

ifeq ($(PROFILE_OP_COUNTS),1)
	STATIC = 1
	CXXFLAGS += -DPROFILE_OP_COUNTS
endif

ifeq ($(STATIC),1)
ifneq ($(PLATFORM),darwin)
	CXXFLAGS += -static
endif
	CXXFLAGS += -DSTATIC
else
	CXXFLAGS += -fPIC
endif

ifeq ($(MULTICORE),1)
	CXXFLAGS += -DMULTICORE -fopenmp
endif

ifeq ($(CPPDEBUG),1)
        CXXFLAGS += -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
        DEBUG = 1
endif

ifeq ($(DEBUG),1)
        CXXFLAGS += -DDEBUG -ggdb3
endif

ifeq ($(PERFORMANCE),1)
        OPTFLAGS = -O3 -march=x86-64 -g -mtune=x86-64
        CXXFLAGS += -DNDEBUG
        # Enable link-time optimization:
        CXXFLAGS += -flto -fuse-linker-plugin
        LDFLAGS += -flto
endif

LIB_OBJS  =$(patsubst %.cpp,%.o,$(LIB_SRCS))
EXEC_OBJS =$(patsubst %,%.o,$(EXECUTABLES) $(EXECUTABLES_WITH_GTEST) $(EXECUTABLES_WITH_SUPERCOP))
GTEST_OBJS =$(patsubst %.cpp,%.o,$(GTEST_SRCS))

all: \
     $(if $(NO_GTEST),,$(EXECUTABLES_WITH_GTEST) $(GTEST_TESTS)) \
     $(if $(NO_SUPERCOP),,$(EXECUTABLES_WITH_SUPERCOP)) \
     $(EXECUTABLES) \
     $(LIBSNARK_A) \
     $(if $(NO_DOCS),,doc)

doc: $(DOCS)

$(DEPINST_EXISTS):
	# Create placeholder directories for installed dependencies. Some make settings (including the default) require actually running ./prepare-depends.sh to populate this directory.
	mkdir -p $(DEPINST)/lib $(DEPINST)/include
	touch $@

# In order to detect changes to #include dependencies. -MMD below generates a .d file for each .o file. Include the .d file.
-include $(patsubst %.o,%.d, $(LIB_OBJS) $(GTEST_OBJS) $(EXEC_OBJS) )

$(LIB_OBJS) $(if $(NO_GTEST),,$(GTEST_OBJS)) $(EXEC_OBJS): %.o: %.cpp
	$(CXX) -o $@   $< -c -MMD $(CXXFLAGS)

LIBGTEST_A = $(DEPINST)/lib/libgtest.a

$(LIBGTEST_A): $(GTESTDIR)/libsnark/gtest-all.cc $(DEPINST_EXISTS)
	$(CXX) -o $(DEPINST)/lib/gtest-all.o   -I $(GTESTDIR) -c -isystem $(GTESTDIR)/include $< $(CXXFLAGS)
	$(AR) -rv $(LIBGTEST_A) $(DEPINST)/lib/gtest-all.o

# libsnark.a will contains all of our relevant object files, and we also mash in the .a files of relevant dependencies built by ./prepare-depends.sh
$(LIBSNARK_A): $(LIB_OBJS) $(AR_LIBS)
	$(AR) q $(LIBSNARK_A) $(LIB_OBJS)
	if [ -n "$(AR_LIBS)" ]; then mkdir -p tmp-ar; cd tmp-ar; for AR_LIB in $(AR_LIBS); do $(AR) x $$AR_LIB; done; $(AR) qc $(LIBSNARK_A) tmp-ar/*; cd ..; rm -r tmp-ar; fi;
	$(AR) s $(LIBSNARK_A)

libsnark.so: $(LIBSNARK_A) $(DEPINST_EXISTS)
	$(CXX) -o $@   --shared -Wl,--whole-archive $(LIBSNARK_A) $(CXXFLAGS) $(LDFLAGS) -Wl,--no-whole-archive $(LDLIBS)

libsnark/gadgetlib2/tests/gadgetlib2_test: \
	libsnark/gadgetlib2/tests/adapters_UTEST.cpp \
	libsnark/gadgetlib2/tests/constraint_UTEST.cpp \
	libsnark/gadgetlib2/tests/gadget_UTEST.cpp \
	libsnark/gadgetlib2/tests/integration_UTEST.cpp \
	libsnark/gadgetlib2/tests/protoboard_UTEST.cpp \
	libsnark/gadgetlib2/tests/variable_UTEST.cpp

$(EXECUTABLES): %: %.o $(LIBSNARK_A) $(DEPINST_EXISTS)
	$(CXX) -o $@   $@.o $(LIBSNARK_A) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

$(EXECUTABLES_WITH_GTEST): %: %.o $(LIBSNARK_A) $(if $(COMPILE_LIBGTEST),$(LIBGTEST_A)) $(DEPINST_EXISTS)
	$(CXX) -o $@   $@.o $(LIBSNARK_A) $(CXXFLAGS) $(LDFLAGS) $(GTEST_LDLIBS) $(LDLIBS)

$(EXECUTABLES_WITH_SUPERCOP): %: %.o $(LIBSNARK_A) $(DEPINST_EXISTS)
	$(CXX) -o $@   $@.o $(LIBSNARK_A) $(CXXFLAGS) $(LDFLAGS) $(SUPERCOP_LDLIBS) $(LDLIBS)

$(GTEST_TESTS): %: $(GTEST_OBJS) $(LIBSNARK_A) $(if $(COMPILE_LIBGTEST),$(LIBGTEST_A)) $(DEPINST_EXISTS)
	$(CXX) -o $@   $(GTEST_OBJS) $(LIBSNARK_A) $(CXXFLAGS) $(LDFLAGS) $(GTEST_LDLIBS) $(LDLIBS)


ifeq ($(STATIC),1)
LIB_FILE = $(LIBSNARK_A)
else
LIB_FILE = libsnark.so
endif

lib: $(LIB_FILE)

$(DOCS): %.html: %.md
	markdown_py -f $@ $^ -x toc -x extra --noisy
#	TODO: Would be nice to enable "-x smartypants" but Ubuntu 12.04 doesn't support that.
#	TODO: switch to redcarpet, to produce same output as GitHub's processing of README.md. But what about TOC?

ifeq ($(PREFIX),)
install:
	$(error Please provide PREFIX. E.g. make install PREFIX=/usr)
else
HEADERS_SRC=$(shell find libsnark -name '*.hpp' -o -name '*.tcc')
HEADERS_DEST=$(patsubst libsnark/%,$(PREFIX)/include/libsnark/%,$(HEADERS_SRC))

$(HEADERS_DEST): $(PREFIX)/include/libsnark/%: libsnark/%
	mkdir -p $(shell dirname $@)
	cp $< $@

install: $(INSTALL_LIBS) $(HEADERS_DEST) $(DEPINST_EXISTS)
	mkdir -p $(PREFIX)/lib
	cp -v $(INSTALL_LIBS) $(PREFIX)/lib/
ifneq ($(NO_COPY_DEPINST),1)
	cp -rv $(DEPINST)/include $(PREFIX)
endif
endif

check: $(GTEST_TESTS)
	$(GTEST_TESTS)

doxy:
	doxygen doxygen.conf

# Clean generated files, except locally-compiled dependencies
clean:
	$(RM) \
		$(LIB_OBJS) $(GTEST_OBJS) $(EXEC_OBJS) \
		$(EXECUTABLES) $(EXECUTABLES_WITH_GTEST) $(EXECUTABLES_WITH_SUPERCOP) $(GTEST_TESTS) \
		$(DOCS) \
		${patsubst %.o,%.d,${LIB_OBJS} ${GTEST_OBJS} ${EXEC_OBJS}} \
		libsnark.so $(LIBSNARK_A) \
	$(RM) -fr doxygen/ \
	$(RM) $(LIBGTEST_A) $(DEPINST)/lib/gtest-all.o

# Clean all, including locally-compiled dependencies
clean-all: clean
	$(RM) -fr $(DEPSRC) $(DEPINST)

.PHONY: all clean clean-all doc doxy lib install
