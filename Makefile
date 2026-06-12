# Makefile

all: world

CXX ?= g++
AR ?= ar
LN ?= ln

CXXFLAGS ?= --std=c++23 -Wall -fPIC
LDFLAGS ?= -L/lib -L/usr/lib

INCLUDES += -I./examples/include

USE_OLD_SERVER_LIST ?= 0
SPEEDTEST_LINK_SHARED ?= no
SPEEDTEST_DIR := .
SPEEDTEST_SHARED_LIB := libspeedtestcpp.so.3.0.0
SPEEDTEST_STATIC_LIB := libspeedtestcpp.a

include Makefile.inc

world: libs examples

libs: $(SPEEDTEST_STATIC_LIB) $(SPEEDTEST_SHARED_LIB) $(SPEEDTEST_DIR)/libspeedtestcpp.so
examples: speedtest minitest bartest sparktest

objs/minitest.o: examples/mini.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

objs/main.o: examples/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

objs/bartest.o: examples/bartest.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

objs/sparktest.o: examples/sparktest.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB): $(SPEEDTEST_OBJS)
	$(CXX) -shared $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)
	$(LN) -sf $(SPEEDTEST_SHARED_LIB) $(SPEEDTEST_DIR)/libspeedtestcpp.so

$(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB): $(SPEEDTEST_OBJS)
	$(AR) rcs $@ $+

$(SPEEDTEST_DIR)/libspeedtestcpp.so: $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)

ifeq ($(SPEEDTEST_LINK_SHARED),yes)

speedtest: objs/main.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) -L. $(LDFLAGS) objs/main.o -o $@ $(SPEEDTEST_LIBS) -lspeedtestcpp

minitest: objs/minitest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) -L. $(LDFLAGS) objs/minitest.o -o $@ $(SPEEDTEST_LIBS) -lspeedtestcpp

bartest: objs/bartest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) -L. $(LDFLAGS) objs/bartest.o -o $@ $(SPEEDTEST_LIBS) -lspeedtestcpp

sparktest: objs/sparktest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) -L. $(LDFLAGS) objs/sparktest.o -o $@ $(SPEEDTEST_LIBS) -lspeedtestcpp

else

speedtest: objs/main.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)

minitest: objs/minitest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)

bartest: objs/bartest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)

sparktest: objs/sparktest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)

endif

.PHONY: clean
clean:
	rm -f objs/*.o speedtest minitest bartest sparktest $(SPEEDTEST_STATIC_LIB) $(SPEEDTEST_SHARED_LIB) libspeedtestcpp.so
