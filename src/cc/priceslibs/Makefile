CXX = g++
CXXFLAGS = -fPIC -I../..

libpricessampleparser.so: PricesResultParserSample.cpp cjsonpointer.cpp ../../cJSON.c
	$(CXX) $(CXXFLAGS) -shared $^ -o $@
