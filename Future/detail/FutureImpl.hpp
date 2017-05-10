/**
 * @file FutureImpl.hpp
 * @author Aaryaman Sagar
 *
 * This file contains the main code for the FutureImpl class.  Which is the
 * implementation of the futures code, this contains all the synchronization
 * for when a value is set and stuff like that
 *
 * The Promise and Future classes are simply wrappers around this.  Both hook
 * into methods in this when one wants to set state and when the other waits
 * for the promise to be fulfilled.
 *
 * The Promise class is responsible for creating this shared state but the
 * Future class which in most cases extends beyond the Promise class is
 * responsible for destroying it
 */

#pragma once

#include <exception>
#include <condition_variable>
#include <mutex>
#include <initializer_list>

namespace sharp {

namespace detail {

    /**
     * A future impl represents the shared state that a future/promise pair
     * share among them.  This contains all the main code for the futures
     * library
     */
    template <typename Type>
    class FutureImpl {

        /**
         * The wait function blocks until there is a value or an exception in
         * the shared state, this uses a condition variable to wait internally
         */
        void wait() const;

        /**
         * wait_and_get() first calls wait() to wait for shared state to be
         * ready and then returns the object in the shared state as if by
         * std::move(instance)
         */
        Type get() const;

        /**
         * Forwarding reference implementations of the set_value function
         */
        template <typename... Args>
        void set_value(Args&&...);
        template <typename U, typename... Args>
        void set_value(std::initializer_list<U> il, Args&&...);

        /**
         * Set an exception in the shared state that will be rethrown on a get
         */
        void set_exception(std::exception_ptr ptr);

    private:

        /**
         * Synchronizy locking things
         */
        mutable std::atomic<bool> is_set{false};
        bool contains_exception{false};
        mutable std::mutex mtx;
        mutable std::condition_variable cv;

        /**
         * A union containing either an exception_ptr or a value, this should
         * be replaced with a better std::variant once that has been
         * standardized and most compilers support that
         */
        std::aligned_union_t<0, std::exception_ptr, Type> storage;
    };

} // namespace detail

} // namespace sharp

#include <sharp/Future/detail/FutureImpl.ipp>