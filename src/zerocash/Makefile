OPTFLAGS = -march=native -mtune=native -O2
CXXFLAGS += -g -Wall -Wextra -Wno-unused-parameter -std=c++11 -fPIC -Wno-unused-variable
LDFLAGS += -flto

DEPSRC=depsrc
DEPINST=depinst

LIBZEROCASH=libzerocash
UTILS=$(LIBZEROCASH)/utils
TESTUTILS=tests
LDLIBS += -L $(DEPINST)/lib -Wl,-rpath $(DEPINST)/lib -L . -lsnark -lgmpxx -lgmp

ifeq ($(USE_MT),1)
	LDLIBS += -lboost_system-mt
	LDLIBS += -lboost_unit_test_framework-mt
else
	LDLIBS += -lboost_system
	LDLIBS += -lboost_unit_test_framework
endif

LDLIBS += -lcrypto -lcryptopp -lz -ldl

ifeq ($(LINK_RT),1)
LDLIBS += -lrt
endif


CXXFLAGS += -I $(DEPINST)/include -I $(DEPINST)/include/libsnark -I . -DUSE_ASM -DCURVE_ALT_BN128

LIBPATH = /usr/local/lib

SRCS= \
	$(UTILS)/sha256.cpp \
	$(UTILS)/util.cpp \
	$(LIBZEROCASH)/IncrementalMerkleTree.cpp \
	$(LIBZEROCASH)/Address.cpp \
	$(LIBZEROCASH)/CoinCommitment.cpp \
	$(LIBZEROCASH)/Coin.cpp \
	$(LIBZEROCASH)/MintTransaction.cpp \
	$(LIBZEROCASH)/PourInput.cpp \
	$(LIBZEROCASH)/PourOutput.cpp \
	$(LIBZEROCASH)/PourProver.cpp \
	$(LIBZEROCASH)/PourTransaction.cpp \
	$(LIBZEROCASH)/ZerocashParams.cpp \
	$(TESTUTILS)/timer.cpp

EXECUTABLES= \
	zerocash_pour_ppzksnark/tests/test_zerocash_pour_ppzksnark \
	zerocash_pour_ppzksnark/profiling/profile_zerocash_pour_gadget \
	tests/zerocashTest \
	tests/utilTest \
	tests/merkleTest \
	libzerocash/GenerateParamsForFiles

OBJS=$(patsubst %.cpp,%.o,$(SRCS))

ifeq ($(MINDEPS),1)
	CXXFLAGS += -DMINDEPS
else
	LDLIBS += -lboost_program_options
	LDLIBS += -lprocps
endif

ifeq ($(LOWMEM),1)
	CXXFLAGS += -DLOWMEM
endif

ifeq ($(STATIC),1)
	CXXFLAGS += -static -DSTATIC
endif

ifeq ($(PROFILE_CURVE),1)
	CXXFLAGS += -static -DPROFILE_CURVE
endif

ifeq ($(MULTICORE),1)
	# When running ./get-libsnark to prepare for this build, use:
	# $ LIBSNARK_FLAGS='MULTICORE=1 STATIC=1' ./get-libsnark.
	# If you're missing some static libraries, it may help to also add
	# $ NO_PROCPS=1  ...  ./get-libsnark
	# and pass MINDEPS=1 to this makefile
	# and run ./get-cryptopp to build the static cryptopp library.
	CXXFLAGS += -static -fopenmp -DMULTICORE
endif

all: $(EXECUTABLES) libzerocash.a

cppdebug: CXXFLAGS += -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
cppdebug: debug

debug: CXXFLAGS += -DDEBUG -g -ggdb3
debug: all

noasserts: CXXFLAGS += -DNDEBUG -Wno-unused-variable -Wno-unused-but-set-variable
noasserts: all

# In order to detect changes to #include dependencies. -MMD below generates a .d file for .cpp file. Include the .d file.
-include $(SRCS:.cpp=.d)

$(OBJS) ${patsubst %,%.o,${EXECUTABLES}}: %.o: %.cpp
	$(CXX) -o $@ $< -c -MMD $(CXXFLAGS)

$(EXECUTABLES): %: %.o $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

libzerocash: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	$(CXX) -shared -o "libzerocash.so" $(OBJS) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)
	@echo 'Finished building target: $@'
	@echo 'Copying libzerocash.so'
	sudo cp libzerocash.so $(LIBPATH)/libzerocash.so
	sudo ldconfig
	@echo 'Finished copying libzerocash.so'
	@echo ' '

libzerocash.a: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	$(AR) rcvs $@ $(OBJS)
	@echo 'Finished building target: $@'
	#@echo 'Copying libzerocash.a'
	#sudo cp libzerocash.a $(LIBPATH)/libzerocash.a
	#sudo ldconfig
	#@echo 'Finished copying libzerocash.a'
	@echo ' '

test_library: %: tests/zerocashTest.o $(OBJS)
	$(CXX) -o tests/$@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) -lzerocash

banktest_library: %: bankTest.o $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) -lzerocash

merkletest_library: %: merkleTest.o $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) -lzerocash

.PHONY: clean install

clean:
	$(RM) \
		$(OBJS) \
		$(EXECUTABLES) \
		${patsubst %,%.o,${EXECUTABLES}} \
		${patsubst %,%.d,${EXECUTABLES}} \
		${patsubst %.cpp,%.d,${SRCS}} \
		libzerocash.a \
		tests/test_library


HEADERS_SRC=$(shell find . -name '*.hpp' -o -name '*.tcc' -o -name '*.h')
HEADERS_DEST=$(patsubst %,$(PREFIX)/include/libzerocash/%,$(HEADERS_SRC))

$(HEADERS_DEST): $(PREFIX)/include/libzerocash/%: %
	mkdir -p $(shell dirname $@)
	cp $< $@

install: all $(HEADERS_DEST)
	mkdir -p $(PREFIX)/lib
	install -m 0755 libzerocash.a $(PREFIX)/lib/
	mkdir -p $(PREFIX)/bin
	install -m 0755 -t $(PREFIX)/bin/ $(EXECUTABLES)
