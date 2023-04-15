USE_OLD_SERVER_LIST?=0
SPEEDTEST_DIR?=speedtestcpp

CXXFLAGS?=--std=c++23
LDFLAGS?=-L/lib -L/usr/lib

SPEEDTEST_GIT_COMMIT:=xxx

ifeq ($(shell test -e "$(SPEEDTEST_DIR)/.git/refs/heads/master" && echo -n yes),yes)
	SPEEDTEST_GIT_COMMIT=$(shell cat $(SPEEDTEST_DIR)/.git/refs/heads/master | cut -c 1-8)
	CXXFLAGS+= -DSPEEDTEST_GIT_COMMIT="\"$(SPEEDTEST_GIT_COMMIT)\""
endif

INCLUDES:=-I./$(SPEEDTEST_DIR)/include
SPEEDTEST_LIBS:=-lcurl

SPEEDTEST_OBJS:= \
	objs/sp_md5.o \
	objs/sp_profile.o \
	objs/sp_client.o \
	objs/sp_speedtest.o

ifneq ($(USE_OLD_SERVER_LIST),0)
	SPEEDTEST_OBJS += objs/sp_xml_servers_parser.o
	CXXFLAGS+= -DOLD_SERVER_LIST=1
else
	SPEEDTEST_OBJS += objs/sp_json_servers_parser.o
endif

objs/sp_md5.o: $(SPEEDTEST_DIR)/src/md5.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/sp_utils.o: $(SPEEDTEST_DIR)/src/utils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/sp_profile.o: $(SPEEDTEST_DIR)/src/profile.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/sp_client.o: $(SPEEDTEST_DIR)/src/client.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/sp_json_servers_parser.o: $(SPEEDTEST_DIR)/src/json_servers_parser.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/sp_xml_servers_parser.o: $(SPEEDTEST_DIR)/src/xml_servers_parser.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/sp_speedtest.o: $(SPEEDTEST_DIR)/src/speedtest.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;
