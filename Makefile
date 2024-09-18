lib:
	g++ -DGTHREAD_INIT_ON_START -std=c++20 -Wall -Wextra -Iinclude src/*.cpp -g -c -o src/gthread.o
	ar rcs libgthread.a src/gthread.o

example: lib
	g++ -DGTHREAD_INIT_ON_START -std=c++20 -Wall -Wextra -Iinclude example/*.cpp -g libgthread.a -o example.exe

clean:
	@-rm example.exe libgthread.a src/gthread.o