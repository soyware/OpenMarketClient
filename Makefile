CXX=g++
CXXFLAGS=-std=c++17 -Wall -D_FORTIFY_SOURCE=2 -fstack-protector-all
LDLIBS=-lwolfssl -lcurl
INC=-I../libs/wolfssl -I../libs/curl/include -I../libs/rapidjson/include

all: main

main:
	$(CXX) $(CXXFLAGS) $(LDLIBS) $(INC) market/Main.cpp -o bin/MarketsBot -lstdc++fs