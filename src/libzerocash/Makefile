CXXFLAGS += -g -Wall -Wextra -Werror -Wfatal-errors -Wno-unused-parameter -march=native -mtune=native -std=c++11 -fPIC
LDFLAGS += -flto

ZCDEP = ../../zcdep
DEPSRC = $(ZCDEP)/src
DEPINST = $(ZCDEP)/inst

#TESTUTILS=tests # BUG: Re-enable tests in the appropriate location.

LDLIBS += -L $(DEPINST)/lib -Wl,-rpath $(DEPINST)/lib -L . -lsnark -lzm -lgmpxx -lgmp
LDLIBS += -lboost_system -lcrypto -lcryptopp
CXXFLAGS += -I $(DEPINST)/include -I $(DEPINST)/include/libsnark -I . -DUSE_ASM -DCURVE_ALT_BN128

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

EXECUTABLES= \
	tests/test_hash \
	tests/test_pour_ppzksnark \
	tests/profile_pour_gadget \
	tests/zerocashTest \
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
	CXXFLAGS += -static -DMULTICORE
endif

all: $(EXECUTABLES) libzerocash.a

cppdebug: CXXFLAGS += -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
cppdebug: debug

debug: CXXFLAGS += -DDEBUG -ggdb3
debug: all

noasserts: CXXFLAGS += -DNDEBUG -Wno-unused-variable -Wno-unused-but-set-variable
noasserts: all

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

.PHONY: clean

clean:
	$(RM) \
		$(OBJS) \
		$(EXECUTABLES) \
		${patsubst %,%.o,${EXECUTABLES}} \
		${patsubst %,%.o,${SRCS}} \
		libzerocash.a \
		tests/test_library
