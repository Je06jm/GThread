CXX = g++
CXX_FLAGS = -std=c++20 -Wall -Wextra -Iinclude -O2

AR = ar
AR_FLAGS = rcs

LIBRARY_NAME = libgthread.a
EXAMPLE_NAME = gt_example

FILES_TO_REMOVE = $(wildcard $(EXAMPLE_NAME)) $(wildcard $(EXAMPLE_NAME).*) $(wildcard $(LIBRARY_NAME)) $(wildcard src/*.o)

all: example

debug: CXX_FLAGS += -g
debug: example

library:
	$(CXX) $(CXX_FLAGS) src/*.cpp -c -o src/gthread.o
	$(AR) $(AR_FLAGS) $(LIBRARY_NAME) src/*.o
	
example: library
	g++ -DGTHREAD_INIT_ON_START $(CXX_FLAGS) example/*.cpp libgthread.a -o $(EXAMPLE_NAME)

clean:
	@-rm $(FILES_TO_REMOVE)