SHELL = /bin/sh
CC    = gcc
#CXXFLAGS     = -std=c++11 -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
CFLAGS       = -std=c++11  -I../univalue/include -I../cryptoconditions/include -I../cryptoconditions/src -I../cryptoconditions/src/asn -I.. -I. -fPIC -shared -c
DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

TARGET  = ../cclib.so
SOURCES = cclib.cpp
#HEADERS = $(shell echo ../cryptoconditions/include/*.h)
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(info Building cclib.so to src/)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -rf $(TARGET)

