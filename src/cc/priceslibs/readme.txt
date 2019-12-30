This is a directory for the placement of 'Prices' module custom result json parsers.
A custom parser is a shared library that implements a single C function defined in priceslibs.h file.
To use it, a custom parser library should be specified in -ac_feeds command line komodod parameter.
An example of custom parser is PricesResultParserSample.cpp source (there is also a Makefile for building this sample).

For more information see 'Prices' module documentation on http://developers.komodoplatform.com web site.
