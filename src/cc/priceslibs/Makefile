CXX = g++
CXXFLAGS = -fPIC -I../..

libpricessampleparser.so: PricesResultParserSample.cpp cjsonpointer.c ../../cJSON.c
	$(CXX) $(CXXFLAGS) -shared $^ -o $@
