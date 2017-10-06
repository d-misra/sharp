#pragma once

#include <sharp/Concurrent/Concurrent.hpp>
#include <sharp/Traits/Traits.hpp>

#include <cassert>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <type_traits>

namespace sharp {

namespace detail {

    /**
     * Concepts(ish)
     */
    /**
     * Enable if a mutex is shared lockable
     */
    template <typename Mutex>
    using EnableIfIsSharedLockable
        = sharp::void_t<decltype(std::declval<Mutex>().lock_shared()),
                        decltype(std::declval<Mutex>().unlock_shared())>;

    /**
     * Tags to determine which locking policy is considered.
     *
     * ReadLockTag inherits from WriteLockTag because if the implementation
     * gets a standard mutex without a lock_shared method then both the const
     * and non const versions of lock on passing both a ReadLockTag and a
     * WriteLockTag to tbe below functions will not get a compile error
     * because one of them will be disabled by SFINAE.
     *
     * So for example given the following case
     *
     *      Concurrent<int, std::mutex> int_locked;
     *
     *      // ... in implementation
     *      lock_mutex(int_locked.mtx, ReadLockTag{}); // (1)
     *
     * this will work even though the ReadLockTag{} dispatches the
     * implementation to the function that calls mtx.lock_shared() on the
     * underlying mutex.  This is because the std::enable_if_t disables that
     * call.  However since the ReadLockTag is inherited from WriteLockTag the
     * function call is still suitable for the write lock implementation.
     */
    struct WriteLockTag {};
    struct ReadLockTag : public WriteLockTag {};

    /**
     * Lock methods that are dispatched via the method described above, shared
     * locking is preferrable when the calling methods says so and exclusive
     * locking is the fallback
     */
    template <typename Mutex, EnableIfIsSharedLockable<Mutex>* = nullptr>
    void lock_mutex(Mutex& mtx, const ReadLockTag& = ReadLockTag{}) {
        mtx.lock_shared();
    }
    template <typename Mutex>
    void lock_mutex(Mutex& mtx, const WriteLockTag& = WriteLockTag{}) {
        mtx.lock();
    }

    /**
     * Unlock methods that are dispatched via the method described above,
     * in the documentation for ReadLockTag and WriteLockTag, shared locking
     * is preferred when specified and exclusive locking is the fallback
     */
    template <typename Mutex, EnableIfIsSharedLockable<Mutex>* = nullptr>
    void unlock_mutex(Mutex& mtx, const ReadLockTag& = ReadLockTag{}) {
        mtx.unlock_shared();
    }
    template <typename Mutex>
    void unlock_mutex(Mutex& mtx, const WriteLockTag& = WriteLockTag{}) {
        mtx.unlock();
    }

} // namespace detail

/**
 * Implementations for the lock proxy methods
 */
template <typename Type, typename Mutex>
template <typename C, typename LockTag>
Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::LockProxy(C& c)
        : instance_ptr{&c} {
    detail::lock_mutex(this->instance_ptr->mtx, LockTag{});
}

template <typename Type, typename Mutex>
template <typename C, typename LockTag>
Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::LockProxy(
        LockProxy&& other) : instance_ptr{other.instance_ptr} {
    // make the other null
    other.instance_ptr = nullptr;
}

template <typename Type, typename Mutex>
template <typename C, typename LockTag>
void Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::unlock()
        noexcept {
    // unlock the mutex and go into a null state
    if (this->instance_ptr) {
        detail::unlock_mutex(this->instance_ptr->mtx, LockTag{});
        this->instance_ptr = nullptr;
    }
}

template <typename Type, typename Mutex>
template <typename C, typename LockTag>
Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::~LockProxy() {
    // unlock and go into a null state
    this->unlock();
}

template <typename Type, typename Mutex>
template <typename C, typename LockTag>
typename Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::value_type&
Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::operator*() {
    return this->instance_ptr->datum;
}

template <typename Type, typename Mutex>
template <typename C, typename LockTag>
typename Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::value_type*
Concurrent<Type, Mutex>::template LockProxy<C, LockTag>::operator->() {
    return std::addressof(this->instance_ptr->datum);
}

/**
 * Implementations for the Concurrent<> methods
 */
template <typename Type, typename Mutex>
template <typename Func>
auto Concurrent<Type, Mutex>::synchronized(Func&& func)
        -> decltype(std::declval<Func>()(std::declval<Type&>())) {

    // acquire the locked exclusively by constructing an object of type
    // UniqueLockedProxy
    auto lock = this->lock();
    return std::forward<Func>(func)(*lock);
}

template <typename Type, typename Mutex>
template <typename Func>
auto Concurrent<Type, Mutex>::synchronized(Func&& func) const
        -> decltype(std::declval<Func>()(std::declval<Type&>())) {

    // acquire the locked exclusively by constructing an object of type
    // UniqueLockedProxy
    auto lock = this->lock();
    return std::forward<Func>(func)(*lock);
}

template <typename Type, typename Mutex>
auto Concurrent<Type, Mutex>::lock() {
    return LockProxy<Concurrent, detail::WriteLockTag>{*this};
}

template <typename Type, typename Mutex>
auto Concurrent<Type, Mutex>::lock() const {
    return LockProxy<const Concurrent, detail::ReadLockTag>{*this};
}

/**
 * RAII based constructor decoration implementation
 *
 * This function accepts an action that is used to perform some action before
 * the constructor implementation is ran and clean up afterwards.  The before
 * and cleanup are done through construction and destruction of the Action
 * object.
 */
template <typename Type, typename Mutex>
template <typename Action, typename... Args>
Concurrent<Type, Mutex>::Concurrent(
        sharp::delegate_constructor::tag_t,
        Action,
        Args&&... args) : Concurrent<Type, Mutex>{
    sharp::implementation::tag, std::forward<Args>(args)...} {}

/**
 * the implementation for the constructors, accepts a forwarding reference to
 * any type of Concurrent object and then forwards it's data into our data
 */
template <typename Type, typename Mutex>
template <typename OtherConcurrent>
Concurrent<Type, Mutex>::Concurrent(
        sharp::implementation::tag_t, OtherConcurrent&& other)
    : datum{std::forward<OtherConcurrent>(other).datum} {}

/**
 * Copy constructor forwards with an action to the decorated delegate
 * constructor, which in turn forwards to the implementation constructor
 */
template <typename Type, typename Mutex>
Concurrent<Type, Mutex>::Concurrent(const Concurrent<Type, Mutex>& other)
    : Concurrent<Type, Mutex>{sharp::delegate_constructor::tag, other.lock(),
        other} {}

/**
 * Same as the copy constructor
 */
template <typename Type, typename Mutex>
Concurrent<Type, Mutex>::Concurrent(Concurrent&& other)
    : Concurrent{sharp::delegate_constructor::tag, other.lock(),
        std::move(other)} {}

template <typename Type, typename Mutex>
template <typename... Args>
Concurrent<Type, Mutex>::Concurrent(std::in_place_t, Args&&... args)
    : datum{std::forward<Args>(args)...} {}
template <typename Type, typename Mutex>
template <typename U, typename... Args>
Concurrent<Type, Mutex>::Concurrent(std::in_place_t,
                                    std::initializer_list<U> il, Args&&... args)
    : datum{il, std::forward<Args>(args)...} {}

/**
 * Implementation for the copy assignment operator.
 *
 * The algorithm used to lock the mutexes is simple.  Lock the one that comes
 * earlier in memory first and then lock the other.
 */
template <typename Type, typename Mutex>
Concurrent<Type, Mutex>& Concurrent<Type, Mutex>::operator=(
        const Concurrent& other) {

    // check which one comes first in memory
    if (reinterpret_cast<uintptr_t>(std::addressof(other.mtx)) <
            reinterpret_cast<uintptr_t>(std::addressof(this->mtx))) {

        // lock the two RAII style
        auto other_locked_proxy = other.lock();
        auto this_locked_proxy = this->lock();

        // now do the assignment
        this->datum = other.datum;

    } else /* if this mutex comes first */ {

        // lock in reverse order
        auto this_locked_proxy = this->lock();
        auto other_locked_proxy = other.lock();

        // do the assignment
        this->datum = other.datum;
    }

    return *this;
}

} // namespace sharp
