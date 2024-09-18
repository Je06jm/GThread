# GThread
A simple to use green thread library for c++17

## Description
GThreads is a userland thread library designed to be easy to use and add to any project. Each gthread allocates it's stack only when it first runs to minimize the memory footprint. Additionally, each gthread stack size can be changed by changing gthread::default_stack_size during runtime to allow fine tuning of the stack size. At the moment only x86_64 build targets are supported

## Getting Started
### Dependencies
* GCC/clang compiler that supports c++17

### Compiling
Make is used as the build system. There are four targets
* all (Builds static library)
* debug (Builds static library with debug symbols)
* example (Builds static library and example program)
* example_debug (Builds static library and example program with debug symbols)

### Using
To have the gthreads initialize itself automatically, pass ```-DGTHREAD_INIT_ON_START``` to the compiler when compiling your source files. Alternatively, you can manually initialize gthreads by calling ```GTHREAD_INIT()```
Note that trying to create any threads before gthreads is initialize will cause the program to crash
```c++
int main() {
    GTHREAD_INIT();

    gthread::execute(range, 0, 10); // OK
}
```
No matter how the library is initialized, it will clean itself up after main() returns