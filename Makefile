.PHONY: all clean

all: serwer

serwer: serwer.cpp
	g++ -std=c++17 serwer.cpp -o serwer -lstdc++fs

clean:
	rm -f *.o serwer