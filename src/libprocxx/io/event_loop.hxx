#ifndef libprocxx__io__event_loop_hxx
#define libprocxx__io__event_loop_hxx

/**
 *  \file
 *  \brief  Linux epoll-based (socket) I/O event loop
 *
 *  \date   2018/03/31
 *  \author Vaclav Krpec  <vencik@razdva.cz>
 *
 *
 *  LEGAL NOTICE
 *
 *  Copyright (c) 2017, Vaclav Krpec
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 *  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "libprocxx/io/socket.hxx"
#include "libprocxx/io/pipe.hxx"
#include "libprocxx/io/file_descriptor.hxx"
#include "libprocxx/mt/utils.hxx"

#include <tuple>
#include <memory>
#include <list>
#include <functional>
#include <cstddef>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <stdexcept>

extern "C" {
#include <unistd.h>
#include <sys/epoll.h>
}


namespace libprocxx {
namespace io {

/**
 *  \brief  Linux epoll-based event loop
 *
 *  Can poll on sockets and pipes.
 *
 *  The event loop supports both level and edge triggering (the latter
 *  being the default.
 *  See \c man \c epoll for info on usage implications).
 *
 *  The event loop is thread-safe; you may add/modify/remove
 *  file descriptors from different threads than the one that executes
 *  the loop.
 *
 *  \tparam  EntryData  Type of data carried by event loop entry
 *                      (no data by default)
 */
template <typename EntryData = std::tuple<> >
class event_loop {
    public:

    enum poll_event {
        IN  = EPOLLIN,      /**< Ready for reading   */
        OUT = EPOLLOUT,     /**< Ready for writing   */
        ET  = EPOLLET,      /**< Edge-triggered poll */
    };  // end of enum poll_event

    struct entry;                                /**< File descriptor entry */
    using entries_t = std::list<entry>;          /**< Entries list type     */

    using entry_handle_t = typename entries_t::iterator;  /**< Entry handle */

    /** Event callback */
    using callback_t = std::function<void(entry_handle_t, int)>;

    /**
     *  \brief  User data stored in the entry
     *
     *  Must have default constructor.
     */
    using entry_data_t = EntryData;

    struct entry {
        entry_handle_t    self;      /**< Iterator to the entry itself */
        int               events;    /**< Registered I/O events        */
        int               fd;        /**< File descriptor (raw)        */
        callback_t        callback;  /**< Event callback               */
        entry_data_t      data;      /**< User data                    */

        template <typename... Args>
        entry(int events_, int fd_, callback_t & callback_, Args&&... args):
            events(events_),
            fd(fd_),
            callback(callback_),
            data(std::forward<Args>(args)...)
        {}

    };  // end of struct entry

    private:

    enum loop_signal {
        INTERRUPT = 'I',    /**< Interrupt event loop */
        TERMINATE = 'X',    /**< Terminate event loop */
    };  // end of enum loop_signal

    /** epoll events */
    using events_ptr_t = std::unique_ptr<struct epoll_event[]>;

    const size_t    m_max_events;   /**< Max. amount of events per loop */
    const int       m_defaults;     /**< Default events flags           */
    events_ptr_t    m_events;       /**< I/O events                     */
    pipe            m_sigpipe;      /**< Signal pipe                    */
    file_descriptor m_epoll_fd;     /**< Linux epoll file descriptor    */
    entries_t       m_entries;      /**< Registered entries             */

    mutable std::mutex              m_mutex;     /**< Operation mutex    */
    mutable std::condition_variable m_shutdown;  /**< Shutdown condition */

    /**
     *  \brief  Default event flags initialiser
     *
     *  \param  edge_triggered  Add all file descriptors in edge-triggered mode
     *
     *  \return Default flags
     */
    static int defaults_init(bool edge_triggered) {
        int flags = 0;
        if (edge_triggered) flags |= poll_event::ET;
        return flags;
    }

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  max_events      Max. amount of events per loop
     *  \param  edge_triggered  Add all file descriptors in edge-triggered mode
     *                          (default behaviour)
     */
    event_loop(size_t max_events, bool edge_triggered = true):
        m_max_events(max_events),
        m_defaults(defaults_init(edge_triggered)),
        m_sigpipe(pipe::flag::NONBLOCK),
        m_epoll_fd(::epoll_create1(0))
    {
        if (m_epoll_fd.closed())
            throw std::runtime_error(
                "libprocxx::io::event_loop: "
                "failed to create epoll");

        m_events = std::make_unique<struct epoll_event[]>(m_max_events);
        add_fd(m_sigpipe.read_end(), poll_event::IN, nullptr);
    }

    /** Copying is forbidden */
    event_loop(const event_loop & orig) = delete;

    /**
     *  \brief  Add socket to the event loop
     *
     *  \param  sock      Socket
     *  \param  events    Registered events (see \ref poll_event)
     *  \param  callback  Event callback
     *  \param  args      Entry data constructor arguments
     *
     *  \return Entry handle
     */
    template <typename... Args>
    entry_handle_t add_socket(
        const socket &  sock,
        int             events,
        callback_t      callback,
        Args&&...       args)
    {
        return register_fd(sock, events, callback, std::forward<Args>(args)...);
    }

    /**
     *  \brief  Add pipe to the event loop
     *
     *  \param  pipe_   Pipe
     *  \param  events  Registered events (see \ref poll_event)
     *
     *  \return Entry handle
     */
    entry_handle_t add_pipe(
        pipe &     pipe_,
        int        events,
        callback_t callback)
    {
        if (poll_event::IN & events)
            return register_fd(pipe_.read_end(), events, callback);

        if (poll_event::OUT & events)
            return register_fd(pipe_.write_end(), events, callback);
    }

    /**
     *  \brief  Modify I/O events of an entry
     *
     *  \param  handle  Entry handle
     *  \param  events  New events
     */
    void modify(entry_handle_t handle, int events) {
        mod_fd(handle->fd, events, &*handle);
        handle->events = events;
    }

    /**
     *  \brief  Remove entry from event loop
     *
     *  \param  handle  Event loop entry handle
     */
    void remove(entry_handle_t handle) {
        del_fd(handle->fd);

        lock4scope(m_mutex);
        m_entries.erase(handle);
    }

    /**
     *  \brief  Move entry from \c this event loop to \c other
     *
     *  \param  handle    Entry handle
     *  \param  other     Target loop
     */
    void move(entry_handle_t handle, event_loop & other) {
        del_fd(handle->fd);

        {
            lock4scope(m_mutex);
            lock4scope(other.m_mutex);

            other.m_entries.splice(other.m_entries.end(), m_entries, handle);
        }

        other.add_fd(handle->fd, handle->events, &*handle);
    }

    /**
     *  \brief  Run one loop
     *
     *  \return \c true iff the event loop was shut down
     */
    bool run_one() { return routine(); }

    /**
     *  \brief  Interrupt loop
     *
     *  Used to interrupt one loop, typically the \ref run_one function.
     *  The function will NOT block until the loop terminates, though.
     */
    void interrupt() { signalise(INTERRUPT); }

    /**
     *  \brief  Run loop until shut down
     */
    void run() {
        while(!routine());

        lock4scope(m_mutex);

        m_sigpipe.read_end().close();
        m_shutdown.notify_one();
    }

    /**
     *  \brief  Event loop shutdown (don't wait for completion)
     *
     *  Used to interrupt the \ref run and \ref run_once functions and shut
     *  the event loop down.
     *  Unlike \ref shutdown, The function will NOT wait for the loop
     *  termination.
     */
    void shutdown_async() {
        {
            lock4scope(m_mutex);
            if (m_sigpipe.write_end().closed()) return;  // already shut down
        }

        signalise(TERMINATE);
    }

    /**
     *  \brief  Event loop shutdown
     *
     *  Used to interrupt the \ref run function and shut the event loop down.
     *  The function will block until \ref run terminates.
     *
     *  NOTE: The \ref run_one function doesn't signalise to \c shutdown.
     *  Calling \c shutdown will block indefinitely if \ref run is not running.
     */
    void shutdown() {
        auto lock = get_lock4scope(m_mutex);

        if (m_sigpipe.write_end().closed()) return;  // already shut down

        lock.unlock();
        signalise(TERMINATE);
        lock.lock();

        while (!m_sigpipe.read_end().closed())
            m_shutdown.wait(lock);

        m_sigpipe.write_end().close();
    }

    private:

    /**
     *  \brief  Register file descriptor
     *
     *  \param  fd        File descriptor (raw)
     *  \param  events    Registered events (see \ref poll_event)
     *  \param  callback  Event callback
     *  \param  args      Entry data constructor arguments
     *
     *  \return Entry handle
     */
    template <typename... Args>
    entry_handle_t register_fd(
        int        fd,
        int        events,
        callback_t callback,
        Args&&...  args)
    {
        entry_handle_t handle;
        entry_handle_t entries_end;

        {
            lock4scope(m_mutex);

            handle = m_entries.emplace(m_entries.end(),
                events, fd, callback, std::forward<Args>(args)...);
            entries_end = m_entries.end();
        }

        if (entries_end != handle) {
            handle->self = handle;
            add_fd(fd, events, &*handle);
        }

        return handle;
    }

    /**
     *  \brief  Poll on file descriptor
     *
     *  \param  fd        File descriptor (raw)
     *  \param  events    Registered events (see \ref poll_event)
     *  \param  data_ptr  Custom data pointer
     */
    void add_fd(
        int    fd,
        int    events,
        void * data_ptr)
    {
        struct epoll_event ev;
        ev.events   = events | m_defaults;
        ev.data.ptr = data_ptr;

        if (-1 == ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev))
            throw std::runtime_error(
                "libprocxx::io::event_loop: "
                "failed to add file descriptor");
    }

    /**
     *  \brief  Modify file descriptor polling events
     *
     *  \param  fd        File descriptor (raw)
     *  \param  events    Changed events (see \ref poll_event)
     *  \param  data_ptr  Custom data pointer
     */
    void mod_fd(
        int    fd,
        int    events,
        void * data_ptr)
    {
        struct epoll_event ev;
        ev.events   = events | m_defaults;
        ev.data.ptr = data_ptr;

        if (-1 == ::epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &ev))
            throw std::runtime_error(
                "libprocxx::io::event_loop: "
                "failed to modify file descriptor events");
    }

    /**
     *  \brief  Don't poll on file descriptor any more
     *
     *  \param  fd  File descriptor (raw)
     */
    void del_fd(int fd) {
        if (-1 == ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr))
            throw std::runtime_error(
                "libprocxx::io::event_loop: "
                "failed to remove file descriptor");
    }

    /**
     *  \brief  Event loop routine
     *
     *  Runs one loop.
     *
     *  \return \c true iff the event loop was shut down
     */
    bool routine() {
        const int ev_cnt = ::epoll_wait(
            m_epoll_fd, m_events.get(), m_max_events, -1);
        if (-1 == ev_cnt)
            throw std::runtime_error(
            "libprocxx::io::event_loop: "
            "epoll_wait failed");

        for (size_t i = 0; i < (size_t)ev_cnt; ++i) {
            auto & event = m_events[i];

            // Signal pipe message
            if (nullptr == event.data.ptr) {
                for (;;) {
                    unsigned char msg;
                    auto rcnt_err   = m_sigpipe.read(&msg, 1);
                    auto & read_cnt = std::get<0>(rcnt_err);
                    auto & error    = std::get<1>(rcnt_err);

                    if (EAGAIN == error) break;  // done reading for now
                    assert(1 == read_cnt);

                    switch (msg) {
                        case INTERRUPT: break;
                        case TERMINATE: return true;
                    }
                }

                continue;  // get on with other events
            }

            // I/O event
            const entry & ent = *static_cast<entry *>(event.data.ptr);
            ent.callback(ent.self, event.events);
        }

        return false;
    }

    /**
     *  \brief  Signalise on the signal pipe
     *
     *  \param  signal  Signal sent to the event loop routine
     */
    void signalise(loop_signal signal) {
        const unsigned char msg = signal;
        m_sigpipe.write(&msg, sizeof(msg));
    }

};  // end of class event_loop

}}  // end of namespace libprocxx::io

#endif  // end of #ifndef libprocxx__io__event_loop_hxx
