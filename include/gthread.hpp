#ifndef GTHREAD_HPP
#define GTHREAD_HPP

#include <memory>
#include <list>
#include <tuple>
#include <mutex>
#include <thread>
#include <functional>

namespace gthread {

    namespace __impl {
        class thread {
        public:
            using thread_function = void(*)(void*);

        protected:
            const thread_function function;

            std::unique_ptr<uint64_t[]> stack;
            const size_t stack_size;

            thread(thread_function function, size_t stack_size) : function{function}, stack_size{stack_size} {}

            virtual void platform_swap(std::shared_ptr<thread> thread) = 0;

        public:
            virtual ~thread() = default;

            void Swap(std::shared_ptr<thread> thread);
            
            virtual void Stop() = 0;

            std::shared_ptr<thread> Create(thread_function function, size_t stack_size = 2*1024*1024);
        };

        class context {
        private:
            std::mutex lock;
            std::list<std::shared_ptr<thread>> threads;

            std::list<std::shared_ptr<context>> other_ctxs;

            std::shared_ptr<thread> scheduling;

            void steal_thread_from_other_contexts();
            void run_one_thread();
        
        public:
            context();
            ~context();

            inline void add_other_context(std::shared_ptr<context> ctx) {
                other_ctxs.push_back(ctx);
            }

            void run_until_all_empty();

            void yield();
            void exit();
        };

        thread_local std::shared_ptr<context> ctx;

        template <typename Type>
        struct shared_state {
            std::shared_ptr<std::unique_ptr<Type>> data;

            inline shared_state() {
                data = std::make_shared<std::unique_ptr<Type>>(nullptr);
            };

            inline shared_state(shared_state&& other) : data{std::move(other.data)} {}
            inline shared_state(const shared_state& other) noexcept : data{other.data} {}
            
            inline shared_state& operator=(shared_state&& other) noexcept {
                data = std::move(other.data);
                return *this;
            }

            inline shared_state& operator=(const shared_state& other) noexcept {
                data = other.data;
                return *this;
            }

            inline void swap(shared_state& other) noexcept {
                std::swap(data, other.data);
            }

            inline bool valid() const noexcept {
                return *data != nullptr;
            }

            inline const Type& get_data() const noexcept {
                wait();

                return *(*data);
            }

            inline Type& get_data() const noexcept {
                wait();

                return *(*data);
            }

            inline void set_data(Type&& value) {
                *data = std::make_unique<Type>(std::move(value));
            }

            inline void set_data(const Type& value) {
                *data = std::make_unique<Type>(value);
            }
        };
    }

    template <typename Type>
    class future {
        friend class promise<Type>;
    
    private:
        __impl::shared_state<Type> state;
        __impl::shared_state<std::exception> exception;
    
        inline future(__impl::shared_state<Type>& state, __impl::shared_state<std::exception>& exception) : state{state}, exception{exception} {}
    
    public:
        future() = default;
        inline future(future&& other) noexcept : state{std::move(other.state)}, exception{std::move(other.exception)} {}
        future(const future&) = delete;

        inline future& operator=(future&& other) noexcept {
            state = std::move(other.state);
            exception = std::move(other.exception);
            return *this;
        }

        future& operator=(const future&) = delete;

        inline void swap(future& other) noexcept {
            state.swap(other.state);
        }

        inline const Type& get() const {
            return state.get();
        }

        inline Type& get() const {
            return state.get();
        }
        
        inline const std::exception& err() const {
            return exception.get_data();
        }

        inline std::exception& err() const {
            return exception.get_data();
        }

        inline bool valid() const noexcept {
            return state.valid() || exception.valid();
        }

        inline void wait() const noexcept {
            while (!valid()) __impl::ctx->yield();
        }

        inline bool has_err() const noexcept {
            return exception.valid();
        }
    };

    template <typename Type>
    class promise {
    private:
        __impl::shared_state<Type> state;
        __impl::shared_state<std::exception> exception;
    
    public:
        promise() = default;
        inline promise(promise&& other) noexcept : state{std::move(other.state)}, exception{std::move(other.exception)} {}
        promise(const promise&) = delete;

        promise& operator=(promise&& other) noexcept {
            state = std::move(other.state);
            exception = std::move(other.exception);
            return *this;
        }

        promise& operator=(const promise&) = delete;

        inline future<Type> get_future() {
            return future<Type>(state);
        }

        inline void set_value(Type&& value) {
            state.set_data(std::move(value));
        }

        inline void set_value(const Type& value) {
            state.set_data(value);
        }

        inline void set_exception(std::exception&& e) {
            exception.set_data(std::move(e));
        }

        inline void set_exception(const std::exception& e) {
            exception.set_data(e);
        }

        inline void swap(promise& other) {
            state.swap(other.state);
        }
    };

    template <>
    class future<void> {
        friend class promise<void>;
    
    private:
        __impl::shared_state<bool> state;
        __impl::shared_state<std::exception> exception;

        inline future(__impl::shared_state<bool>& state, __impl::shared_state<std::exception>& exception) : state{state}, exception{exception} {}
    
    public:
        future() = default;
        inline future(future&& other) noexcept : state{std::move(other.state)}, exception{std::move(other.exception)} {}
        future(const future&) = delete;

        inline future& operator=(future&& other) noexcept {
            state = std::move(other.state);
            exception = std::move(other.exception);
            return *this;
        }

        future& operator=(const future& other) = delete;

        inline void swap(future& other) noexcept {
            state.swap(other.state);
        }

        inline const void get() const {
            while (!valid()) __impl::ctx->yield();
        }

        inline bool valid() const noexcept {
            return state.valid() || exception.valid();
        }

        inline void wait() const noexcept {
            while (!valid()) __impl::ctx->yield();
        }
    };

    template <>
    class promise<void> {
    private:
        __impl::shared_state<bool> state;
        __impl::shared_state<std::exception> exception;
    
    public:
        promise() = default;
        inline promise(promise&& other) noexcept : state{std::move(other.state)}, exception{std::move(other.exception)} {}
        promise(const promise&) = delete;

        promise& operator=(promise&& other) noexcept {
            state = std::move(other.state);
            exception = std::move(other.exception);
            return *this;
        }

        promise& operator=(const promise&) = delete;

        inline future<void> get_future() {
            return future<void>(state, exception);
        }

        inline void set_value() {
            state.set_data(true);
        }

        inline void set_exception(std::exception&& e) {
            exception.set_data(std::move(e));
        }

        inline void set_exception(const std::exception& e) {
            exception.set_data(e);
        }

        inline void swap(promise& other) {
            state.swap(other.state);
        }
    };

    template <typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) -> promise<decltype(func(args...))> {
        using Ret = decltype(func(args...));
        using BindType = decltype(std::bind(func, args...));

        using UserParams = std::tuple<BindType, promise<Ret>>;

        promise<Ret> p;
        auto f = p.get_future();

        auto user_params = new UserParams{std::bind(func, args...), std::move(p)};

        auto calling_lambda = +[](void* params_pointer) -> void {
            auto user_params = reinterpret_cast<UserParams*>(params_pointer);

            auto& [bind, p] = *user_params;

            try {
                if constexpr (std::is_same_v<Ret, void>) {
                    bind();
                    p.set_value();
                }
                else
                    p.set_value(bind());
            }
            catch (std::exception& e) {
                p.set_exception(e);
            }

            delete user_params;

            __impl::ctx->exit();
        };
    }

    inline void yield() {
        __impl::ctx->yield();
    }

    inline void exit() {
        __impl::ctx->exit();
    }

}

#endif