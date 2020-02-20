CXX = g++
CXXFLAGS = -fPIC -I../..
all: libpricessampleparser.so libnistrandomparser.so

libpricessampleparser.so:
	$(CXX) $(CXXFLAGS) -shared PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c -o $@

libnistrandomparser.so:
	$(CXX) $(CXXFLAGS) -shared $^ NistRandomParser.cpp cjsonpointer.cpp ../../cJSON.c -o $@

clean:
	rm libpricessampleparser.so libnistrandomparser.so