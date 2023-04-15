USE_OLD_SERVER_LIST?=0
SPEEDTEST_DIR?=speedtestcpp

CXXFLAGS?=--std=c++23
LDFLAGS?=-L/lib -L/usr/lib

INCLUDES:=-I./$(SPEEDTEST_DIR)/include
SPEEDTEST_LIBS:=-lcrypto -lcurl

SPEEDTEST_OBJS:= \
	objs/sp_utils.o \
	objs/sp_profile.o \
	objs/sp_client.o \
	objs/sp_speedtest.o

ifneq ($(USE_OLD_SERVER_LIST),0)
	SPEEDTEST_OBJS += objs/sp_xml_servers_parser.o
	CXXFLAGS+= -DOLD_SERVER_LIST=1
else
	SPEEDTEST_OBJS += objs/sp_json_servers_parser.o
endif

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
