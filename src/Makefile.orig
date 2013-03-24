CXX=clang++
#CXX=c++
CXXFLAGS=-I ../include -g
ALLOCOBJS=ws_allocate.o ws_util.o
RELEASEOBJS=ws_release.o ws_util.o
BOOSTLIBS=-lboost_program_options -lboost_regex -lboost_filesystem -lboost_system

all: ws_allocate ws_release

release: all
	strip -s ws_allocate ws_release

ws_allocate: $(ALLOCOBJS)
	$(CXX) $(CXXFLAGS) $(ALLOCOBJS) -o ../bin/ws_allocate $(BOOSTLIBS) -lcap -L ../lib -lyaml-cpp

ws_release: $(RELEASEOBJS)
	$(CXX) $(CXXFLAGS) $(RELEASEOBJS) -o ../bin/ws_release $(BOOSTLIBS) -lcap -L ../lib -lyaml-cpp

ws_util.o: ws_util.cpp ws_util.h
ws_allocate.o: ws_allocate.cpp ws_util.h
ws_release.o: ws_release.cpp ws_util.h

clean:
	rm ws_allocate.o ws_util.o ws_release.o ../bin/ws_release ../bin/ws_allocate
