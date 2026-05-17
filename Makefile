# Makefile

all: world

CXX ?= g++
AR ?= ar
LN ?= ln

CXXFLAGS ?= --std=c++23 -Wall -fPIC
LDFLAGS ?= -L/lib -L/usr/lib

INCLUDES += -I./examples/include

USE_OLD_SERVER_LIST ?= 0
SPEEDTEST_LINK_SHARED ?= yes
SPEEDTEST_DIR := .
SPEEDTEST_SHARED_LIB := libspeedtestcpp.so.1.0.0
SPEEDTEST_STATIC_LIB := libspeedtestcpp.a

include Makefile.inc

world: libs examples

libs: $(SPEEDTEST_STATIC_LIB) $(SPEEDTEST_SHARED_LIB) $(SPEEDTEST_DIR)/libspeedtestcpp.so
examples: speedtest minitest

objs/minitest.o: examples/mini.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

objs/main.o: examples/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB): $(SPEEDTEST_OBJS)
	$(CXX) -shared $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)
	$(LN) -sf $(SPEEDTEST_SHARED_LIB) $(SPEEDTEST_DIR)/libspeedtestcpp.so

$(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB): $(SPEEDTEST_OBJS)
	$(AR) rcs $@ $+

$(SPEEDTEST_DIR)/libspeedtestcpp.so: $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)

ifeq ($(SPEEDTEST_LINK_SHARED),yes)

speedtest: objs/main.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) $(LDFLAGS) -L. objs/main.o -o $@ $(SPEEDTEST_LIBS) -lspeedtestcpp

minitest: objs/minitest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) $(LDFLAGS) -L. objs/minitest.o -o $@ $(SPEEDTEST_LIBS) -lspeedtestcpp

else

speedtest: objs/main.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)

minitest: objs/minitest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(SPEEDTEST_LIBS)

endif

.PHONY: clean
clean:
	rm -f objs/*.o speedtest minitest $(SPEEDTEST_STATIC_LIB) $(SPEEDTEST_SHARED_LIB) libspeedtestcpp.so
