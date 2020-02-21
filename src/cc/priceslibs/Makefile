CXXFLAGS = -fPIC -I../..
CXXFLAGS_WIN = -I../..
all: libpricessampleparser.so libnistrandomparser.so
dll: libpricessampleparser.dll libnistrandomparser.dll

libpricessampleparser.so: PricesResultParserSample.cpp cjsonpointer.cpp cjsonpointer.h ../../cJSON.c ../../cJSON.h priceslibs.h
	$(CXX) $(CXXFLAGS) -shared PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c -o $@ -std=c++11

libpricessampleparser.dll: PricesResultParserSample.cpp cjsonpointer.cpp cjsonpointer.h ../../cJSON.c ../../cJSON.h priceslibs.h
	$(CXX) $(CXXFLAGS_WIN) -shared PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c -o $@ -std=c++11 -static-libgcc -static-libstdc++

libnistrandomparser.so: NistRandomParser.cpp cjsonpointer.cpp cjsonpointer.h ../../cJSON.c ../../cJSON.h priceslibs.h
	$(CXX) $(CXXFLAGS) -shared NistRandomParser.cpp cjsonpointer.cpp ../../cJSON.c -o $@ -std=c++11 

libnistrandomparser.dll: NistRandomParser.cpp cjsonpointer.cpp cjsonpointer.h ../../cJSON.c ../../cJSON.h priceslibs.h
	$(CXX) $(CXXFLAGS_WIN) -shared NistRandomParser.cpp cjsonpointer.cpp ../../cJSON.c -o $@ -std=c++11 -static-libgcc -static-libstdc++

clean:
	rm libpricessampleparser.* libnistrandomparser.*.