SHELL		= /bin/sh
CC		= gcc
CC_DARWIN	= g++-6
CC_WIN		= x86_64-w64-mingw32-gcc-posix
CFLAGS = -arch x86_64
CXXFLAGS_DARWIN	= -std=c++11 -arch x86_64 -I/usr/local/Cellar/gcc\@6/6.4.0_2/include/c++/6.4.0/ -I../../depends/$(shell echo `../..//depends/config.guess`/include) -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -c -Wl,-undefined -Wl,dynamic_lookup -dynamiclib
CXXFLAGS		= -std=c++11 -I../../depends/$(shell echo `../..//depends/config.guess`/include) -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
CXXFLAGS_WIN	= -std=c++11 -I../../depends/$(shell echo `../..//depends/config.guess`/include) -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
DEBUGFLAGS	= -O0 -D _DEBUG
RELEASEFLAGS	= -O2 -D NDEBUG -combine -fwhole-program
$(info $(OS))
OS := $(shell uname -s)
$(info $(OS))
TARGET  = ../libcc.so
TARGET_DARWIN = ../libcc.dylib
TARGET_WIN = ../libcc.dll
SOURCES = cclib.cpp ../cJSON.c
OBJS = cclib.o ../cJSON.o

all: $(TARGET)

%.o: %.c
	$(CC) -o $@ $< $(CFLAGS) $(DEBUGFLAGS)

%.o: %.cpp
	$(CC) -o $@ $< $(CXXFLAGS) $(DEBUGFLAGS)

$(TARGET): $(OBJS)
	$(info Building cclib to src/)
ifeq ($(OS),Darwin)
	$(CC_DARWIN) $(CXXFLAGS_DARWIN) $(DEBUGFLAGS) -o $(TARGET_DARWIN) $(OBJS)
else ifeq ($(OS),Linux)
	$(CC) $(CXXFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(OBJS)
#else ifeq ($(WIN_HOST),True) - todo: pass ENV var from build.sh if WIN host
else
	$(info WINDOWS)
	$(CC_WIN) $(CXXFLAGS_WIN) $(DEBUGFLAGS) -o $(TARGET_WIN) $(OBJS)
endif

clean:
	rm -rf $(TARGET)
