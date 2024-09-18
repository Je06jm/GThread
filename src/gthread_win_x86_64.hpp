#ifdef _WIN32
#include <gthread.hpp>

namespace gthread::__impl {

    // The gthread class to handle the Windows 64 bit C calling convention
    // See more here: https://en.wikipedia.org/wiki/X86_calling_conventions#x86-64_calling_conventions
    class win_amd64_gthread : public gthread {
    public:
        win_amd64_gthread(Function function, void* user_params, size_t stack_size, bool is_setup) : gthread{function, user_params, stack_size, is_setup} {}

        struct platform_context {
            uint64_t rsp;
            uint64_t rcx;
            uint64_t gp_regs[8];

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
                "movq %rsp,  0(%rcx) \n"
                "movq %rcx,  8(%rcx) \n"
                "movq %rbx, 16(%rcx) \n"
                "movq %rbp, 24(%rcx) \n"
                "movq %rdi, 32(%rcx) \n"
                "movq %rsi, 40(%rcx) \n"
                "movq %r12, 48(%rcx) \n"
                "movq %r13, 56(%rcx) \n"
                "movq %r14, 64(%rcx) \n"
                "movq %r15, 72(%rcx) \n"

                "addq $95, %rcx \n" // 80 + 15
                "andq $~15, %rcx \n"
                "fxsave (%rcx) \n"

                "movq  0(%rdx), %rsp \n"
                "movq  8(%rdx), %rcx \n"
                "movq 16(%rdx), %rbx \n"
                "movq 24(%rdx), %rbp \n"
                "movq 32(%rdx), %rdi \n"
                "movq 40(%rdx), %rsi \n"
                "movq 48(%rdx), %r12 \n"
                "movq 56(%rdx), %r13 \n"
                "movq 64(%rdx), %r14 \n"
                "movq 72(%rdx), %r15 \n"

                "addq $95, %rdx \n"
                "andq $~15, %rdx \n"
                "fxrstor (%rdx) \n"

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
            platform_ctx.rsp -= 32;

            *reinterpret_cast<Function*>(platform_ctx.rsp) = function;

            platform_ctx.rcx = reinterpret_cast<uint64_t>(user_params);
        }

        void platform_swap(std::shared_ptr<gthread> next) override {
            auto next_thread = static_cast<win_amd64_gthread*>(next.get());
            swap_platform_contexts(&platform_ctx, &next_thread->platform_ctx);
        }
    };
}
#endif