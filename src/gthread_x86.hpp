#ifdef __i386__
#include <gthread.hpp>

namespace gthread::__impl {

    // The gthread class to handle the 32 bit C calling convention
    // See more here:
    // https://en.wikipedia.org/wiki/X86_calling_conventions#Caller_clean-up
    class x86_gthread : public gthread {
    public:
        x86_gthread(Function function, void* user_params, size_t stack_size,
                    bool is_setup)
            : gthread{function, user_params, stack_size, is_setup} {}

        struct platform_context {
            uint32_t esp;
            uint32_t gp_regs[4];  // ebp, esi, edi, ebx

            // Include extra space as fxsave/fxrstore needs
            // to be used on a 16-byte boundary
            uint8_t fx_state[528];
        };

    private:
        platform_context platform_ctx;

#ifdef __GNUC__
        __attribute__((naked)) static void swap_platform_contexts(
            platform_context*, platform_context*) {
            asm("movl 4(%esp), %ecx \n"
                "movl 8(%esp), %edx \n"

                "movl %esp,  0(%ecx) \n"
                "movl %ebp,  4(%ecx) \n"
                "movl %ebx,  8(%ecx) \n"
                "movl %edi, 12(%ecx) \n"
                "movl %esi, 16(%ecx) \n"

                "addl $35, %ecx \n"  // 20 + 15
                "andl $~15, %ecx \n"
                "fxsave (%ecx) \n"

                "movl  0(%edx), %esp \n"
                "movl  4(%edx), %ebp \n"
                "movl  8(%edx), %ebx \n"
                "movl 12(%edx), %edi \n"
                "movl 16(%edx), %esi \n"

                "addl $35, %edx \n"
                "andl $~15, %edx \n"
                "fxrstor (%edx) \n"

                "ret");
        }
#else
        // swap_platform_context is not implemented on purpose
        static void swap_platform_contexts(platform_context* current,
                                           platform_context* next);
#endif

        void platform_setup() override {
            // A hack to get floating operations work on gthreads
            swap_platform_contexts(&platform_ctx, &platform_ctx);

            platform_ctx.esp = reinterpret_cast<uint32_t>(stack.get()) +
                               static_cast<uint32_t>(stack_size);
            platform_ctx.esp -= 12;

            auto s = reinterpret_cast<uint32_t*>(platform_ctx.esp);
            s[2] = reinterpret_cast<uint32_t>(user_params);
            s[0] = reinterpret_cast<uint32_t>(function);
        }

        void platform_swap(std::shared_ptr<gthread> next) override {
            auto next_thread = static_cast<x86_gthread*>(next.get());
            swap_platform_contexts(&platform_ctx, &next_thread->platform_ctx);
        }
    };
}  // namespace gthread::__impl

#endif