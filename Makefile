CXX = g++
CXX_FLAGS = -std=c++17 -Wall -Wextra -Iinclude

AR = ar
AR_FLAGS = rcs

LIBRARY_NAME = libgthread.a
EXAMPLE_NAME = gt_example

FORMAT = clang-format
FORMAT_FLAGS = --style=file -i
FILES_TO_FORMAT = $(wildcard src/*) $(wildcard include/*) $(wildcard example/*)

FILES_TO_REMOVE = $(wildcard $(EXAMPLE_NAME)) $(wildcard $(EXAMPLE_NAME).*) $(wildcard $(LIBRARY_NAME)) $(wildcard src/*.o)

all: CXX_FLAGS += -O2
all: library

debug: CXX_FLAGS += -g
debug: library

library:
	$(CXX) $(CXX_FLAGS) src/*.cpp -c -o src/gthread.o
	$(AR) $(AR_FLAGS) $(LIBRARY_NAME) src/*.o

example: library
	g++ -DGTHREAD_INIT_ON_START $(CXX_FLAGS) example/*.cpp libgthread.a -o $(EXAMPLE_NAME)

example_debug: CXX_FLAGS += -g
example_debug: library
	g++ -DGTHREAD_INIT_ON_START $(CXX_FLAGS) example/*.cpp libgthread.a -o $(EXAMPLE_NAME)

format:
	$(FORMAT) $(FORMAT_FLAGS) $(FILES_TO_FORMAT)

clean:
	@-rm $(FILES_TO_REMOVE)