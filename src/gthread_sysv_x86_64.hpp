#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <gthread.hpp>

namespace gthread::__impl {

    // The gthread class to handle the System V 64 bit C calling convention
    // See more here: https://en.wikipedia.org/wiki/X86_calling_conventions#x86-64_calling_conventions
    class sysv_amd64_gthread : public gthread {
    public:
        sysv_amd64_gthread(Function function, void* user_params, size_t stack_size, bool is_setup) : gthread{function, user_params, stack_size, is_setup} {}

        struct platform_context {
            uint64_t rsp;
            uint64_t rdi;
            uint64_t gp_regs[6];

            // Include extra space as fxsave/fxrstore needs
            // to be used on a 16-byte boundary
            uint8_t fx_state[528];
        };
    
    private:
        platform_context platform_ctx;

#ifdef __GNUC__
        __attribute__((naked))
        static void swap_platform_contexts(platform_context*, platform_context*) {
            asm(
                "movq %rsp,  0(%rdi) \n"
                "movq %rdi,  8(%rdi) \n"
                "movq %rbx, 16(%rdi) \n"
                "movq %rbp, 24(%rdi) \n"
                "movq %r12, 32(%rdi) \n"
                "movq %r13, 40(%rdi) \n"
                "movq %r14, 48(%rdi) \n"
                "movq %r15, 56(%rdi) \n"

                "addq $79, %rdi \n" // 64 + 15
                "andq $~15, %rdi \n"
                "fxsave (%rdi) \n"

                "movq  0(%rsi), %rsp \n"
                "movq  8(%rsi), %rdi \n"
                "movq 16(%rsi), %rbx \n"
                "movq 24(%rsi), %rbp \n"
                "movq 32(%rdi), %r12 \n"
                "movq 40(%rdi), %r13 \n"
                "movq 48(%rdi), %r14 \n"
                "movq 56(%rdi), %r15 \n"

                "addq $79, %rsi \n"
                "andq $~15, %rsi \n"
                "fxrstor (%rsi) \n"

                "ret"
            );
        }
#else
        // swap_platform_context is not implemented on purpose
        static void swap_platform_contexts(platform_context* current, platform_context* next);
#endif

        void platform_setup() override {
            // A hack to get floating operations work on gthreads
            swap_platform_contexts(&platform_ctx, &platform_ctx);

            platform_ctx.rsp = reinterpret_cast<uint64_t>(stack.get()) + static_cast<uint64_t>(stack_size);

            *reinterpret_cast<Function*>(platform_ctx.rsp) = function;

            platform_ctx.rdi = reinterpret_cast<uint64_t>(user_params);
        }

        void platform_swap(std::shared_ptr<gthread> next) override {
            auto next_thread = static_cast<sysv_amd64_gthread*>(next.get());
            swap_platform_contexts(&platform_ctx, &next_thread->platform_ctx);
        }
    };

}

#endif