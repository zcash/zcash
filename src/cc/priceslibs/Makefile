CXX = g++
CXXFLAGS = -fPIC -I../..

libpricessampleparser.so: PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c
	$(CXX) $(CXXFLAGS) -shared $^ -o $@

libnistrandomparser.so: NistRandomParser.cpp cjsonpointer.cpp ../../cJSON.c
	$(CXX) $(CXXFLAGS) -shared $^ -o $@
