Debug = -g -Wall --std=c++11

ALL:test

test:test.o ThreadPool.o
	g++ $^ -o test -lpthread

test.o:test.cpp
	g++ $(Debug) -c $< -o $@

ThreadPool.o:ThreadPool.cpp
	g++ $(Debug) -c $< -o $@

clean:
	-rm -rf test.o \
	ThreadPool.o \
	test

.PHONY:ALL