# Don't set "user variables" such as CXXFLAGS, LDFLAGS, etc... Instead,
# incorporate those user variables into *_ACTUAL variables with internally
# dictated flags. This is an attempt to emulate autotools and to allow
# ./configure to customize the build:
CXXFLAGS_ACTUAL = $(CXXFLAGS)
LDFLAGS_ACTUAL = $(LDFLAGS)
LDLIBS_ACTUAL = $(LDLIBS)


CXXFLAGS_ACTUAL += -g -Wall -Wextra -Werror -Wfatal-errors -Wno-unused-parameter -march=native -mtune=native -std=c++11 -fPIC
LDFLAGS_ACTUAL += -flto

#TESTUTILS=tests # BUG: Re-enable tests in the appropriate location.

LDLIBS_ACTUAL += -L . -lsnark -lzm -lgmpxx -lgmp
LDLIBS_ACTUAL += -lboost_system -lcrypto -lcryptopp
CXXFLAGS_ACTUAL += -I . -DUSE_ASM -DCURVE_ALT_BN128

LIBPATH = /usr/local/lib

SRCS= \
	./utils/sha256.cpp \
	./utils/util.cpp \
	Node.cpp \
	IncrementalMerkleTree.cpp \
	MerkleTree.cpp \
	Address.cpp \
	CoinCommitment.cpp \
	Coin.cpp \
	MintTransaction.cpp \
	PourTransaction.cpp \
	ZerocashParams.cpp

#	$(TESTUTILS)/timer.cpp # BUG: Re-enable tests in the appropriate location.

TEMPLATES= \
	$(wildcard pour_ppzksnark/*.hpp) \
	$(wildcard pour_ppzksnark/*.tcc) \
	$(wildcard pour_ppzksnark/hashes/*.hpp) \
	$(wildcard pour_ppzksnark/hashes/*.tcc)

EXECUTABLES= \
	tests/test_hash \
	tests/test_pour_ppzksnark \
	tests/profile_pour_gadget \
	tests/zerocashTest \
	tests/merkleTest \
	libzerocash/GenerateParamsForFiles

OBJS=$(patsubst %.cpp,%.o,$(SRCS))

ifeq ($(MINDEPS),1)
	CXXFLAGS_ACTUAL += -DMINDEPS
else
	LDLIBS_ACTUAL += -lboost_program_options
	LDLIBS_ACTUAL += -lprocps
endif

ifeq ($(LOWMEM),1)
	CXXFLAGS_ACTUAL += -DLOWMEM
endif

ifeq ($(STATIC),1)
	CXXFLAGS_ACTUAL += -static -DSTATIC
endif

ifeq ($(PROFILE_CURVE),1)
	CXXFLAGS_ACTUAL += -static -DPROFILE_CURVE
endif

ifeq ($(MULTICORE),1)
	CXXFLAGS_ACTUAL += -static -DMULTICORE
endif

all: $(EXECUTABLES) libzerocash.a

cppdebug: CXXFLAGS_ACTUAL += -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
cppdebug: debug

debug: CXXFLAGS_ACTUAL += -DDEBUG -ggdb3
debug: all

noasserts: CXXFLAGS_ACTUAL += -DNDEBUG -Wno-unused-variable -Wno-unused-but-set-variable
noasserts: all

$(EXECUTABLES): %: %.o $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS_ACTUAL) $(LDFLAGS_ACTUAL) $(LDLIBS_ACTUAL)

libzerocash: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross G++ Linker'
	$(CXX) -shared -o "libzerocash.so" $(OBJS) $(CXXFLAGS_ACTUAL) $(LDFLAGS_ACTUAL) $(LDLIBS_ACTUAL)
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
	$(CXX) -o tests/$@ $^ $(CXXFLAGS_ACTUAL) $(LDFLAGS_ACTUAL) $(LDLIBS_ACTUAL) -lzerocash

banktest_library: %: bankTest.o $(OBJS) 
	$(CXX) -o $@ $^ $(CXXFLAGS_ACTUAL) $(LDFLAGS_ACTUAL) $(LDLIBS_ACTUAL) -lzerocash

merkletest_library: %: merkleTest.o $(OBJS) 
	$(CXX) -o $@ $^ $(CXXFLAGS_ACTUAL) $(LDFLAGS_ACTUAL) $(LDLIBS_ACTUAL) -lzerocash

utils/sha256.o: utils/sha256.cpp utils/sha256.h $(TEMPLATES)
utils/util.o: utils/util.cpp utils/util.h $(TEMPLATES)
Node.o: Node.cpp Node.h $(TEMPLATES)
IncrementalMerkleTree.o: IncrementalMerkleTree.cpp IncrementalMerkleTree.h $(TEMPLATES)
MerkleTree.o: MerkleTree.cpp MerkleTree.h $(TEMPLATES)
Address.o: Address.cpp Address.h $(TEMPLATES)
CoinCommitment.o: CoinCommitment.cpp CoinCommitment.h $(TEMPLATES)
Coin.o: Coin.cpp Coin.h $(TEMPLATES)
MintTransaction.o: MintTransaction.cpp MintTransaction.h $(TEMPLATES)
PourTransaction.o: PourTransaction.cpp PourTransaction.h $(TEMPLATES)
ZerocashParams.o: ZerocashParams.cpp ZerocashParams.h $(TEMPLATES)

.PHONY: clean

clean:
	$(RM) \
		$(OBJS) \
		$(EXECUTABLES) \
		${patsubst %,%.o,${EXECUTABLES}} \
		${patsubst %,%.o,${SRCS}} \
		libzerocash.a \
		tests/test_library
