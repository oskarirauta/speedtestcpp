all: world

CXX?=g++
CXXFLAGS?=--std=c++23 -Wall
LDFLAGS?=-L/lib -L/usr/lib

INCLUDES+= -I./examples

USE_OLD_SERVER_LIST?=0
SPEEDTEST_DIR:=.

include Makefile.inc

world: speedtest minitest

objs/minitest.o: examples/mini.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/main.o: examples/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

speedtest: $(SPEEDTEST_OBJS) objs/main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SPEEDTEST_LIBS) $^ -o $@;

minitest: $(SPEEDTEST_OBJS) objs/minitest.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SPEEDTEST_LIBS) $^ -o $@;

clean:
	rm -f objs/*.o SpeedTest minitest
