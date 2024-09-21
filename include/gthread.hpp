#ifndef GTHREAD_HPP
#define GTHREAD_HPP

#include <memory>
#include <list>
#include <tuple>
#include <mutex>
#include <thread>
#include <functional>
#include <unordered_map>

namespace gthread {

    // This is only used when creating new gthreads. All stack sizes are aligned to the next 16 byte boundary
    inline size_t default_stack_size = 2*1024*1024;

    namespace __impl {

        // Base class that all platform specific green thread classes inherits from.
        // GThreads are lazily setup when it's time to switch to them
        class gthread {
        public:
            using Function = void (*)(void*);

        private:
            uint32_t flag_is_setup : 1;
            uint32_t flag_is_stopped : 1;
            
        protected:
            Function function;
            void* user_params;
            std::unique_ptr<uint64_t[]> stack;
            size_t stack_size;
            
            // No work is actually done here, just data needed for setup
            inline gthread(Function function, void* user_params, size_t stack_size, bool is_setup) : function{function}, user_params{user_params}, stack_size{stack_size} {
                flag_is_setup = is_setup ? 1 : 0;
                flag_is_stopped = 0;
            }

            // Platform specific setup happens here. This is called after the stack is allocated
            virtual void platform_setup() = 0;

            // Platform specific context switch happens here
            virtual void platform_swap(std::shared_ptr<gthread> next) = 0;

        public:
            virtual ~gthread() {}

            // A helper function that setups up the gthread if it's not already. Also allocates the stack if needed. Then platform_swap is called
            inline void swap(std::shared_ptr<gthread> next) {
                if (!next->flag_is_setup) {
                    next->stack = std::unique_ptr<uint64_t[]>(new uint64_t[next->stack_size / 8]);
                    next->platform_setup();
                    next->flag_is_setup = 1;
                }
                
                platform_swap(next);
            }

            // Return true if the green thread is stopped and needs to be cleaned up
            inline bool is_stopped() const {
                return flag_is_stopped;
            }

            // Stops the green thread
            inline void stop() {
                flag_is_stopped = 1;
            }

            // Creates a regular green thread
            static std::shared_ptr<gthread> create_default(Function function, void* user_params, size_t stack_size);

            // Creates a special green thread to represent a kernel thread. This is used for scheduling purposes
            static std::shared_ptr<gthread> create_scheduling();
        };

        // A helper class that holds the scheduling and current threads. Each kernel thread has exactly one of these
        class context {
            friend struct kernel_threads_manager;

        public:
            std::shared_ptr<gthread> scheduling;
            std::shared_ptr<gthread> current;

        private:
            context(std::shared_ptr<gthread>& scheduling) : scheduling{scheduling} {}

        public:
            context() = default;
            context(context&& other) : scheduling{std::move(other.scheduling)}, current{std::move(other.current)} {}
            context(const context&) = delete;

            context& operator=(context&& other) {
                scheduling = std::move(other.scheduling);
                current = std::move(other.current);
                return *this;
            }

            context& operator=(const context&) = delete;
        };

        // Handles the creation and destruction of kernel threads. Also has functions to manage the current gthread and houses the scheduler
        struct kernel_threads_manager {
            std::unordered_map<std::thread::id, context> contexts;
            std::list<std::shared_ptr<gthread>> green_threads;
            std::mutex lock;

            bool running = true;

            std::list<std::thread> threads;

            // Creates the kernel threads and sets up all kernel threads
            void init();

            // Cleans up kernel threads
            void finish();

            // Calls finish after main() returns
            ~kernel_threads_manager() {
                finish();
            }

            // Sets up the scheduling green thread and a context for the kernel thread
            void setup_kernel_thread_context();

            // Runs all the green threads, only returning when all are processed
            void process_green_threads();

            // Yields the current gthread. If this is called without a current gthread, the scheduler is ran
            void yield_current_green_thread();

            // Exits the current gthread. If this is called without a current gthread, an exception is thrown
            void exit_current_green_thread();
        };

        inline kernel_threads_manager kernel_threads;

#ifdef GTHREAD_INIT_ON_START
        struct gthread_init_on_start {
            gthread_init_on_start() {
                kernel_threads.init();
            }
        };

        inline gthread_init_on_start init_on_start;
#endif

        // A helper class to manage the shared state of any promise future pair
        template <typename Type>
        class shared_state {
        private:
            struct State {
                std::unique_ptr<Type> data;
                std::exception_ptr exception;
            };

            std::shared_ptr<State> state;
        
        public:
            inline shared_state() {
                state = std::make_shared<State>();
                state->exception = nullptr;
            }

            inline shared_state(shared_state&& other) noexcept : state{std::move(other.state)} {}
            inline shared_state(const shared_state& other) noexcept : state{other.state} {}

            shared_state& operator=(shared_state&& other) noexcept {
                state = std::move(other.state);
                return *this;
            }

            shared_state& operator=(const shared_state& other) noexcept {
                state = other.state;
                return *this;
            }

            bool has_data() const noexcept {
                return state != nullptr && state->data != nullptr;
            }

            bool has_exception() const noexcept {
                return state != nullptr && state->exception != nullptr;
            }

            const Type& get_data() const {
                return *state->data;
            }

            Type& get_data() {
                return *state->data;
            }

            void set_data(Type&& value) {
                if (!has_data())
                    state->data = std::make_unique<Type>(std::move(value));
                
                else
                    *state->data = std::move(value);
            }

            void set_data(const Type& value) {
                if (!has_data())
                    state->data = std::make_unique<Type>(value);
                
                else
                    *state->data = value;
            }

            const std::exception_ptr& get_exception() const {
                return state->exception;
            }

            std::exception_ptr& get_exception() {
                return state->exception;
            }

            void set_exception(const std::exception_ptr& e) {
                state->exception = e;
            }

            friend bool operator==(const shared_state& lhs, const shared_state& rhs) {
                return lhs.state == rhs.state;
            }

            friend bool operator!=(const shared_state& lhs, const shared_state& rhs) {
                return lhs.state != rhs.state;
            }
        };
    }

    template <typename Type>
    class promise;

    // A custom version of std::future that yields the current gthread instead of blocking the current
    template <typename Type>
    class future {
        friend promise<Type>;

    private:
        __impl::shared_state<Type> state;
    
        future(const __impl::shared_state<Type>& state) : state{state} {}
    public:
        future() = default;
        future(future&& other) noexcept : state{std::move(other.state)} {}
        future(const future&) = delete;

        future& operator=(future&& other) noexcept {
            state = std::move(other.state);
            return *this;
        }

        future& operator=(const future&) = delete;

        // Yields if data has not been set by the corrsponding promise object
        void wait() const {
            while (!state.has_data() && !state.has_exception()) __impl::kernel_threads.yield_current_green_thread();
        }

        const Type& get() const {
            wait();

            if (state.has_exception())
                std::rethrow_exception(state.get_exception());
            
            return state.get_data();
        }

        Type& get() {
            wait();

            if (state.has_exception())
                std::rethrow_exception(state.get_exception());
            
            return state.get_data();
        }

        bool has_data() const {
            return state.has_data();
        }

        bool has_exception() const {
            return state.has_exception();
        }

        const std::exception_ptr& exception() const {
            return state.get_exception();
        }

        std::exception_ptr& exception() {
            return state.get_exception();
        }

        operator bool() const {
            return state.has_data();
        }
    };

    // A custom version of std::future that yields the current gthread instead of blocking the current
    template <>
    class future<void> {
        friend promise<void>;
        
    private:
        __impl::shared_state<bool> state;
    
        future(const __impl::shared_state<bool>& state) : state{state} {}
    public:
        future() = default;
        future(future&& other) noexcept : state{std::move(other.state)} {}
        future(const future&) = delete;

        future& operator=(future&& other) noexcept {
            state = std::move(other.state);
            return *this;
        }

        future& operator=(const future&) = delete;

        // Yields if data has not been set by the corrsponding promise object
        void wait() const {
            while (!state.has_data() && !state.has_exception()) __impl::kernel_threads.yield_current_green_thread();
        }

        void get() const {
            wait();

            if (state.has_exception())
                std::rethrow_exception(state.get_exception());
        }

        bool has_data() const {
            return state.has_data();
        }

        bool has_exception() const {
            return state.has_exception();
        }

        const std::exception_ptr& exception() const {
            return state.get_exception();
        }

        std::exception_ptr& exception() {
            return state.get_exception();
        }

        operator bool() const {
            return state.has_data();
        }
    };

    // A custom version of std::promise
    template <typename Type>
    class promise {
    private:
        __impl::shared_state<Type> state;

    public:
        promise() = default;
        promise(promise&& other) noexcept : state{std::move(other.state)} {}
        promise(const promise&) = delete;

        promise& operator=(promise&& other) noexcept {
            state = std::move(other.state);
            return *this;
        }

        promise& operator=(const promise) = delete;

        void set(Type&& value) {
            state.set_data(std::move(value));
        }

        void set(const Type& value) {
            state.set_data(value);
        }

        void raise(std::exception_ptr e) {
            state.set_exception(e);
        }

        future<Type> get_future() const {
            return future<Type>(state);
        }
    };

    // A custom version of std::promise
    template <>
    class promise<void> {
    private:
        __impl::shared_state<bool> state;
    
    public:
        promise() = default;
        promise(promise&& other) noexcept : state{std::move(other.state)} {}
        promise(const promise&) = delete;
    
        promise& operator=(promise&& other) noexcept {
            state = std::move(other.state);
            return *this;
        }

        promise& operator=(const promise) = delete;

        void set() {
            state.set_data(true);
        }

        void raise(std::exception_ptr e) {
            state.set_exception(e);
        }

        future<void> get_future() const {
            return future<void>(state);
        }
    };

    // Creates a new gthread that executes func(args...) and returns a future.
    // The return value of func is used to set the corrsponding future object
    template <typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) -> future<decltype(func(args...))> {
        using RetType = decltype(func(args...));
        using BindingType = decltype(std::bind(func, args...));

        using UserParams = std::tuple<BindingType, promise<RetType>>;

        auto p = promise<RetType>();
        auto f = p.get_future();

        auto user_params = new UserParams{std::bind(func, args...), std::move(p)};

        auto calling_lambda = +[](void* params_pointer) {
            auto user_params = static_cast<UserParams*>(params_pointer);

            auto& [b, p] = *user_params;

            try {
                if constexpr (std::is_same_v<RetType, void>) {
                    b();
                    p.set();
                }
                else {
                    p.set(b());
                }
            }
            catch (...) {
                p.raise(std::current_exception());
            }

            delete user_params;

            __impl::kernel_threads.exit_current_green_thread();
        };

        auto thread = __impl::gthread::create_default(calling_lambda, user_params, default_stack_size);

        __impl::kernel_threads.lock.lock();
        __impl::kernel_threads.green_threads.push_back(thread);
        __impl::kernel_threads.lock.unlock();

        return f;
    }

    inline void yield() {
        __impl::kernel_threads.yield_current_green_thread();
    }

    inline void exit() {
        __impl::kernel_threads.exit_current_green_thread();
    }

}

// If GTHREAD_INIT_ON_START is not defined, this must be used before any gthread is created
#define GTHREAD_INIT() gthread::__impl::kernel_threads.init()

#endif