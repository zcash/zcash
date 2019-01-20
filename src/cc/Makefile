SHELL		= /bin/sh
CC		= gcc
CC_WIN		= x86_64-w64-mingw32-gcc-posix
CFLAGS_DARWIN	= -std=c++11 -I../../depends/$(shell echo `../..//depends/config.guess`/include) -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
CFLAGS		= -std=c++11 -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
CFLAGS_WIN	= -std=c++11 -I../../depends/$(shell echo `../..//depends/config.guess`/include) -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
DEBUGFLAGS	= -O0 -D _DEBUG
RELEASEFLAGS	= -O2 -D NDEBUG -combine -fwhole-program
$(info $(OS))
OS := $(shell uname -s)
$(info $(OS))
TARGET  = ../cclib.so
TARGET_WIN = ../cclib.dll
SOURCES = cclib.cpp
#HEADERS = $(shell echo ../cryptoconditions/include/*.h)

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(info Building cclib.so to src/)
ifeq ($(OS),Darwin)
	$(CC) $(CFLAGS_DARWIN) $(DEBUGFLAGS) -o $(TARGET) $(SOURCES)
else ifeq ($(OS),Linux)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(SOURCES)
#else ifeq ($(WIN_HOST),True) - todo: pass ENV var from build.sh if WIN host
else
	$(info else)
	#$(CC)_WIN $(CFLAGS_WIN) $(DEBUGFLAGS) -o $(TARGET_WIN) $(SOURCES)
endif

clean:
	rm -rf $(TARGET)
