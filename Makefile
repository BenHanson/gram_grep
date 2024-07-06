CXX = g++
CXXFLAGS = -O -std=c++20 -Wall -I ../lexertl17/include -I ../parsertl17/include -I ../wildcardtl/include

LDFLAGS = -O

LIBS = 

all: gram_grep

gram_grep: args.o main.o parser.o search.o types.o
	$(CXX) $(LDFLAGS) -o gram_grep args.o main.o parser.o search.o types.o $(LIBS)

args.o: args.cpp
	$(CXX) $(CXXFLAGS) -o args.o -c args.cpp

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -o main.o -c main.cpp

parser.o: parser.cpp
	$(CXX) $(CXXFLAGS) -o parser.o -c parser.cpp

search.o: search.cpp
	$(CXX) $(CXXFLAGS) -o search.o -c search.cpp

types.o: types.cpp
	$(CXX) $(CXXFLAGS) -o types.o -c types.cpp

library:

binary:

clean:
	- rm *.o
	- rm gram_grep
