#ifndef MESSAGECONTROLLER_H
#define MESSAGECONTROLLER_H

#include <list>
#include <map>
#include <functional>
#include <tuple>
#include <type_traits>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * every connect-call cause at least once memeroy alocation
 * every emit-call casuse at least once memory allocation
 * I don't know how to improve it, except customizing new and delete
 */

class MessageController {
    using UnifiedFunction = void(*)();
    using UnifiedConstObject = const void*;
    using UnifiedDeleter = void(*)(void*);
    using UnifiedCallback = std::function<void(const void*)>;
    using UnifiedData = std::unique_ptr<void, UnifiedDeleter>;

    // signal must be defined as function
    // it's signature will be used to validate the arguments when emitted
    // it's address will be used to find callbacks
    using Signal = std::pair<UnifiedFunction, UnifiedConstObject>;

    // signal arguments are wrapped in a message
    // in order to avoid memory leak, message uses unique_ptr
    // because arguments lose all type information, message use a custom deleter
    using Message = std::pair<Signal, UnifiedData>;

public:
    MessageController() : done(false) {}
    MessageController(const MessageController&) = delete;
    MessageController(MessageController&&) = delete;

    MessageController& operator = (const MessageController&) = delete;
    MessageController& operator = (MessageController&&) = delete;

    // signal arguments must be value or pointer, not reference
    template <typename ... Args, typename Callback>
    void connect(void(*signal)(Args ...), Callback callback) {
        static_assert(noReference<Args ...>, "signal can only transmit value or pointer");
        table[makeSignal(signal)].push_back(makeCallback<Args ...>(callback));
    }

    template <typename C, typename ... Args, typename Callback>
    void connect(void(C::*signal)(Args ...), const C* object, Callback callback) {
        static_assert(noReference<Args ...>, "signal can only transmit value");
        table[makeSignal(signal, object)].push_back(makeCallback<Args ...>(callback));
    }

    template <typename ... Args, typename ... Parmas, typename T>
    void connect(void(*signal)(Args ...),
                 void(T::*callback)(Parmas ...), T* target) {
        static_assert(noReference<Args ...>, "signal can only transmit value");
        static_assert(std::conjunction_v<std::is_convertible<Args, Parmas> ...>,
                      "the type of callback is not comaptible to signal");

        table[makeSignal(signal)].push_back(makeCallback<Args ...>(callback, target));
    }

    template <typename C, typename ... Args, typename ... Params, typename T>
    void connect(void(C::*signal)(Args ...), const C* object,
                 void(T::*callback)(Params ...), T* target) {
        static_assert(noReference<Args ...>, "signal can only transmit value");
        static_assert(std::conjunction_v<std::is_convertible<Args, Params> ...>,
                      "the type of callback is not comaptible to signal");

        table[makeSignal(signal, object)].push_back(makeCallback<Args ...>(callback, target));
    }

    template <typename ... Args>
    void emit(void(*signal)(Args ...), Args ... args) {
        static_assert(noReference<Args ...>, "signal can only transmit value");
        sequence.push_back(makeMessage(signal, std::move(args) ...));
        ready.notify_one();
    }

    template <typename C, typename ... Args>
    void emit(void(C::*signal)(Args ...), const C* object, Args ... args) {
        static_assert(noReference<Args ...>, "signal can only transmit value");
        sequence.push_back(makeMessage(signal, object, std::move(args) ...));
        ready.notify_one();
    }

    void start() {
        while (!done || !sequence.empty()) {
            if (sequence.empty()) {
                std::unique_lock lock(mutex);
                ready.wait(lock, [&]{ return !sequence.empty(); });
            }

            Message message = std::move(sequence.front());
            sequence.pop_front();

            for (auto& callback : table[message.first]) {
                invoke(callback, message.second.get());
            }
        }
    }

    void stop() { done = true; }

private:
    template <typename ... Args>
    static constexpr bool noReference = !std::disjunction_v<std::is_reference<Args> ...>;

    template <typename ... Args, typename Callback>
    auto makeCallback(Callback callback) {
        return [callback](const void* data) {
            std::apply(callback, *reinterpret_cast<const std::tuple<Args ...>*>(data));
        };
    }

    template <typename ... Args, typename ... Params, typename C>
    auto makeCallback(void(C::*callback)(Params ...), C* object) {
        return [callback, object](const void* data) {
            auto&& curried = [=](auto&& ... args) {
                std::invoke(callback, object, std::forward<decltype(args)>(args) ...);
            };

            std::apply(curried, *reinterpret_cast<const std::tuple<Args ...>*>(data));
        };
    }

    template <typename Signal, typename T = std::nullptr_t>
    auto makeSignal(Signal signal, const T* object = nullptr) {
        return std::make_pair(reinterpret_cast<UnifiedFunction>(signal),
                              reinterpret_cast<UnifiedConstObject>(object));
    }

    template <typename ... Args>
    auto makeMessage(void(*signal)(Args ...), Args&& ... args) {
        auto deleter = [](void* data) {
            delete reinterpret_cast<std::tuple<Args ...>*>(data);
        };

        return std::make_pair(makeSignal(signal),
                              UnifiedData(new std::tuple<Args ...>(std::move(args) ...), deleter));
    }

    template <typename ... Args, typename C>
    auto makeMessage(void(C::*signal)(Args ...), const C* object, Args ... args) {
        // signal arguments lose all type information
        // we need a custom deleter to delete it
        auto deleter = [](void* data) {
            delete reinterpret_cast<std::tuple<Args ...>*>(data);
        };

        return std::make_pair(makeSignal(signal, object),
                              UnifiedData(new std::tuple<Args ...>(std::move(args) ...), deleter));
    }

private:
    std::map<Signal, std::list<UnifiedCallback>> table;
    std::list<Message> sequence;
    std::atomic<bool> done;
    std::mutex mutex;
    std::condition_variable ready;
};

#endif // MESSAGECONTROLLER_H
