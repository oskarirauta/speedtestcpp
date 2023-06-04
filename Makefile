all: world

CXX?=g++
CXXFLAGS?=--std=c++23 -Wall -fPIC
LDFLAGS?=-L/lib -L/usr/lib

INCLUDES+= -I./examples/include

USE_OLD_SERVER_LIST?=0
SPEEDTEST_LINK_SHARED?=yes
SPEEDTEST_DIR:=.
SPEEDTEST_SHARED_LIB:=libspeedtestcpp.so.1.0.0
SPEEDTEST_STATIC_LIB:=libspeedtestcpp.a

include Makefile.inc

world: $(SPEEDTEST_STATIC_LIB) $(SPEEDTEST_SHARED_LIB) speedtest minitest

objs/minitest.o: examples/mini.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/main.o: examples/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

$(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB): $(SPEEDTEST_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(SPEEDTEST_LIBS) $^ -o $@;
	$(LN) -s $(SPEEDTEST_SHARED_LIB) $(SPEEDTEST_DIR)/libspeedtestcpp.so

$(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB): $(SPEEDTEST_OBJS)
	$(AR) rcs $@ $+

.PHONY: $(SPEEDTEST_DIR)/libspeedtestcpp.so
$(SPEEDTEST_DIR)/libspeedtestcpp.so: $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)

ifeq ($(SPEEDTEST_LINK_SHARED),yes)

speedtest: objs/main.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -L. $(SPEEDTEST_LIBS) -lspeedtestcpp objs/main.o -o $@;

minitest: objs/minitest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_SHARED_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -L. $(SPEEDTEST_LIBS) -lspeedtestcpp objs/minitest.o -o $@;

else

speedtest: objs/main.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SPEEDTEST_LIBS) $^ -o $@;

minitest: objs/minitest.o $(SPEEDTEST_DIR)/$(SPEEDTEST_STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SPEEDTEST_LIBS) $^ -o $@;

endif

.PHONY: clean
clean:
	rm -f objs/*.o speedtest minitest $(SPEEDTEST_STATIC_LIB) $(SPEEDTEST_SHARED_LIB) libspeedtestcpp.so
