CXXFLAGS = -fPIC -I../..
CXXFLAGS_WIN = -I../..
all: libpricessampleparser.so libnistrandomparser.so
dll: libpricessampleparser.dll libnistrandomparser.dll

libpricessampleparser.so:
	$(CXX) $(CXXFLAGS) -shared PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c -o $@ --std=c++11

libpricessampleparser.dll:
	$(CXX) $(CXXFLAGS_WIN) -shared PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c -o $@ --std=c++11

libnistrandomparser.so:
	$(CXX) $(CXXFLAGS) -shared $^ NistRandomParser.cpp cjsonpointer.cpp ../../cJSON.c -o $@ --std=c++11

libnistrandomparser.dll:
	$(CXX) $(CXXFLAGS_WIN) -shared $^ NistRandomParser.cpp cjsonpointer.cpp ../../cJSON.c -o $@ --std=c++11

clean:
	rm libpricessampleparser.* libnistrandomparser.*.