/**
 * @file Channel.hpp
 * @author Aaryaman Sagar
 *
 * This file contains an implementation for channels in C++, these channels
 * work exactly the same as the channels in the Go language, along with the
 * convenience and multiplexing operations supported by channels
 *
 * Channels are essentially
 */

#pragma once

#include <mutex>
#include <condition_variable>
#include <utility>
#include <deque>
#include <memory>
#include <exception>
#include <initializer_list>

#include <sharp/Tags/Tags.hpp>

namespace sharp {

/**
 * An asynchronous channel that can be used for synchronization across
 * multiple threads.  This is an implementation of channels as found in the Go
 * language, with an effort made to maintain the same API as much as possible
 * without losing the expressiveness of the original API.
 *
 * A channel is a logical synchronization utility that can be used like
 * generalized semaphores, in fact a buffered Channel is the same as a
 * generalized semaphore (except for the upper bound on the number of times
 * values can be added without fetching, which seems to be designed to prevent
 * user errors)
 *
 * Send and receive calls block on the channel if the buffer cannot handle
 * another value, the value blocking on will be released as soon as a reader
 * fetches the value with a read() call
 */
template <typename Type>
class Channel {
public:

    /**
     * Default constructor for the channel initializes it with a buffer large
     * enough to fit the passed buffer length parameter.
     *
     * The buffer length parameter is not made a template argument so that
     * users can maintain pointers and references to channels, which is a
     * valuable idiom in concurrent C++ programming
     */
    Channel(int buffer_length = 0);

    /**
     * Channels once created cannot be moved or copied around to other
     * channels.  If the intended use is to have a container of channels, then
     * the way to use a channel would be to wrap it around a unique_ptr and
     * then put it in the container
     *
     * This is similar to std::mutex, std::condition_variable and std::atomic,
     * this is a synchronization primitive and should not be moved around.
     * Note that this behavior is unlike futures, which can be moved
     */
    Channel(Channel&&) = delete;
    Channel(const Channel&) = delete;
    Channel& operator=(Channel&&) = delete;
    Channel& operator=(const Channel&) = delete;

    /**
     * The send function to send a value across the channel
     *
     * Unlike asycnhronous sends, sends on a channel block until they are
     * received on the other end, resulting in a synchronization pattern like
     * with futures and promises, but slightly different since there can be
     * multiple sends on a single channel
     *
     * This function has the following two overloads as opposed to one
     * overload with the following signature
     *
     *      void send(Type value)
     *
     * Because libraries should be as efficient in the general case as
     * possible and should provide APIs to the user that enable them to take
     * control of their types.  If one function was provided and the user
     * passed by value, there would be one copy followed by a move.  The user
     * may or may not have wanted that additional move.  And when a type is
     * not move constructible the above will require two copies.  The
     * following two overloads allow control for the moving/copying to reside
     * with the user, when moving and copying are both too expensive the value
     * can just be constructed in place with the third overload
     */
    void send(const Type& value);
    void send(Type&& value);

    /**
     * Overloads to send a value and have that constructed in place in the
     * channel, this avoids unnecesary moves and copies into the channel
     * storage
     *
     * The second function is for initializer list construction in place.
     * Initializer lists cannot be deduced right from template arguments and
     * therefore need a second overload
     */
    template <typename... Args>
    void send(sharp::emplace_construct::tag_t, Args&&... args);
    template <typename U, typename... Args>
    void send(sharp::emplace_construct::tag_t, std::initializer_list<U> ilist,
              Args&&... args);

    /**
     * Sends an exception through the channel, this exception if send will be
     * received by the receiving end when they read a value out of the
     * channel.
     */
    void send_exception(std::exception_ptr p);

    /**
     * The receive function to receive a value from the channel
     *
     * This call is blocking, it will block until there is a value to be read
     * from the channel, thus this is very similar to a fetch on a future
     *
     * The object that was stored internally in the socket is moved into the
     * receiving code when this is called, as a result there may be a return
     * value optimization into the stored object
     */
    Type read();

    /**
     * An iterator class for the channel to represent a range within the
     * channel.  This enables continuous streaming of objects from one thread
     * to another with a simple generalized reading approach.  See README for
     * the module for more information.
     */
    class Iterator;

    /**
     * Begin and end iterators to represent a range within the channel.  This
     * enables continuous streaming of objects from one thread to another with
     * a simple generalized reading approach.  See README for the module for
     * more information.
     */
    Iterator begin();
    Iterator end();

    /**
     * Make friends with the select function, each select operation waits on a
     * mutex for the channel, and therefore there is some hooking involved
     */
    template <typename... SelectContexts>
    friend void channel_select(SelectContexts&&...);

private:

    /**
     * Utility functions to check if a reader has to wait or if a writer has
     * to wait
     */
    bool should_read_wait() const;
    bool should_write_wait() const;

    /**
     * the buffer length of the channel, defaults to 0 meaning that there is
     * no room for a buffer at all, only one value is stored, and all send()
     * and read() calls are blocking until there is a read() or send() on the
     * other end respectively
     */
    int buffer_length{0};

    /**
     * The number of waiting reads, when there is no element in the buffer a
     * send can only go through when there is a pending read
     */
    int waiting_reads{0};

    /**
     * The number of waiting writes, when there is no space in the buffer then
     * a write can only go through when there is a waiting reader
     */
    int waiting_writes{0};

    /**
     * The monitor for synchronization
     */
    std::mutex mtx;
    std::condition_variable read_cv;
    std::condition_variable write_cv;

    /**
     * The type used to represent either an exception or a value
     */
    std::deque<std::aligned_union<0, std::exception_ptr, Type>> elements;
};


/**
 * The select function to allow multiplexing on I/O through a channel.
 * Based on the type of the function passed in to the function call the
 * select method implicitly decides whether the channel is being used to
 * wait on a read or a write operation, and multiplexes I/O based on that
 */
template <typename Func, typename... Args>
void channel_select(std::pair<Channel&, Func>, Args&&...);

} // namespace sharp

#include <sharp/Channel/Channel.ipp>
