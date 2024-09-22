#include <atomic>
#include <gthread.hpp>
#include <iostream>
#include <stdexcept>

#include "gthread_sysv_x86_64.hpp"
#include "gthread_win_x86_64.hpp"
#include "gthread_x86.hpp"

namespace gthread::__impl {

    std::shared_ptr<gthread> gthread::create_default(Function function,
                                                     void* user_params,
                                                     size_t stack_size) {
        stack_size = (stack_size + 15) & ~15;
#ifdef __x86_64__
#ifdef _WIN32
        return std::make_shared<win_x86_64_gthread>(function, user_params,
                                                    stack_size, false);
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
        return std::make_shared<sysv_x86_64_gthread>(function, user_params,
                                                     stack_size, false);
#endif
#elif __i386__
        return std::make_shared<x86_gthread>(function, user_params, stack_size,
                                             false);
#endif
    }

    std::shared_ptr<gthread> gthread::create_scheduling() {
#ifdef __x86_64__
#ifdef _WIN32
        auto gthread =
            std::make_shared<win_x86_64_gthread>(nullptr, nullptr, 0, true);
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
        auto gthread =
            std::make_shared<sysv_x86_64_gthread>(nullptr, nullptr, 0, true);
#endif
#elif __i386__
        auto gthread = std::make_shared<x86_gthread>(nullptr, nullptr, 0, true);
#endif
        gthread->swap(gthread);
        return gthread;
    }

    void kernel_threads_manager::setup_kernel_thread_context() {
        auto scheduling = gthread::create_scheduling();
        lock.lock();
        contexts[std::this_thread::get_id()] = {scheduling};
        lock.unlock();
    }

    void kernel_threads_manager::process_green_threads() {
        auto& ctx = contexts[std::this_thread::get_id()];

        while (true) {
            lock.lock();

            if (green_threads.empty()) {
                lock.unlock();
                break;
            }

            ctx.current = green_threads.front();
            green_threads.pop_front();

            lock.unlock();

            ctx.scheduling->swap(ctx.current);

            if (!ctx.current->is_stopped()) {
                lock.lock();
                green_threads.push_back(ctx.current);
                lock.unlock();
            }

            ctx.current = nullptr;
        }
    }

    void kernel_threads_manager::yield_current_green_thread() {
        auto& ctx = contexts[std::this_thread::get_id()];

        // If the context does not have a current gthread, then this has been
        // called from the main kernel thread
        if (!ctx.current)
            process_green_threads();

        else
            ctx.current->swap(ctx.scheduling);
    }

    void kernel_threads_manager::exit_current_green_thread() {
        auto& ctx = contexts[std::this_thread::get_id()];

        // If the context does not have a current gthread, then this has been
        // called from the main kernel thread and an exception is thrown
        if (!ctx.current)
            throw std::runtime_error(
                "Cannot exit a gthread without a current gthread to exit");

        else {
            ctx.current->stop();
            ctx.current->swap(ctx.scheduling);
        }
    }

    void kernel_threads_manager::init() {
        setup_kernel_thread_context();

        auto thread_count = std::thread::hardware_concurrency() - 1;

        // All kernel thread context objects are held in the same unordered_map
        // object. All kernel threads must go into a lock loop until all threads
        // have finished writing to the unordered_map. inited indicates how many
        // kernel threads are in the lock loop. ack indicates how many threads
        // have acknowledged that they are free to continue. All this is done to
        // ensure that the unordered_map only needs to be locked during kernel
        // thread setup. In addition, ack is used to make sure that inited does
        // not go out of scope while kernel threads are using it.
        std::atomic<size_t> inited = 0;
        std::atomic<size_t> ack = 0;
        for (size_t i = 0; i < thread_count; i++) {
            threads.emplace_back(std::thread(
                [&](bool* running) {
                    setup_kernel_thread_context();
                    inited++;

                    while (inited != thread_count) {
                    }

                    ack++;

                    while (*running) {
                        process_green_threads();

                        std::this_thread::yield();
                    }
                },
                &running));
        }

        // Wait for all kernel threads to acknowledge that it's done with inited
        // so it can go out of scope
        while (ack != thread_count) {
        }
    }

    void kernel_threads_manager::finish() {
        running = false;

        for (auto& thread : threads) thread.join();

        threads.clear();
    }

}  // namespace gthread::__impl