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

#include <vector>
#include <list>
#include <functional>
#include <cstddef>
#include <mutex>
#include <condition_variable>

extern "C" {
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
 */
class event_loop {
    public:

    enum poll_event {
        IN  = EPOLLIN,      /**< Ready for reading   */
        OUT = EPOLLOUT,     /**< Ready for writing   */
        ET  = EPOLLET,      /**< Edge-triggered poll */
    };  // end of enum poll_event

    /** Event callback */
    using callback_t = std::function<void(file_descriptor &, int)>;

    /** Event loop file descriptor entry */
    struct entry {
        file_descriptor & fd;        /**< File descriptor */
        callback_t        callback;  /**< Event callback  */

        entry(file_descriptor & fd_, callback_t & callback_):
            fd(fd_),
            callback(callback_)
        {}

    };  // end of struct entry

    using entries_t = std::list<entry>;  /**< Entries list type */

    private:

    enum loop_signal {
        INTERRUPT = 'I',    /**< Interrupt event loop */
        TERMINATE = 'X',    /**< Terminate event loop */
    };  // end of enum loop_signal

    /** epoll events */
    using events_t = std::vector<struct epoll_event>;

    const size_t    m_max_events;   /**< Max. amount of events per loop */
    const int       m_defaults;     /**< Default events flags           */
    events_t        m_events;       /**< Event vector                   */
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
    event_loop(size_t max_events, bool edge_triggered = true);

    /** Copying is forbidden */
    event_loop(const event_loop & orig) = delete;

    /**
     *  \brief  Add socket to the event loop
     *
     *  \param  sock    Socket
     *  \param  events  Registered events (see \ref poll_event)
     *
     *  \return Entry handle
     */
    entries_t::iterator add_socket(
        socket &   sock,
        int        events,
        callback_t callback)
    {
        return register_fd(sock, events, callback);
    }

    /**
     *  \brief  Add pipe to the event loop
     *
     *  \param  pipe_   Pipe
     *  \param  events  Registered events (see \ref poll_event)
     *
     *  \return Entry handle
     */
    entries_t::iterator add_pipe(
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
    void shutdown_async();

    /**
     *  \brief  Event loop shutdown
     *
     *  Used to interrupt the \ref run function and shut the event loop down.
     *  The function will block until \ref run terminates.
     *
     *  NOTE: The \ref run_one function doesn't signalise to \c shutdown.
     *  Calling \c shutdown will block indefinitely if \ref run is not running.
     */
    void shutdown();

    private:

    /**
     *  \brief  Register file descriptor
     *
     *  \param  fd        File descriptor
     *  \param  events    Registered events (see \ref poll_event)
     *  \param  callback  Event callback
     *
     *  \return Entry handle
     */
    entries_t::iterator register_fd(
        file_descriptor & fd,
        int               events,
        callback_t        callback);

    /**
     *  \brief  Poll on file descriptor
     *
     *  \param  fd        File descriptor
     *  \param  events    Registered events (see \ref poll_event)
     *  \param  data_ptr  Custom data pointer
     */
    void add_fd(
        file_descriptor & fd,
        int               events,
        void *            data_ptr);

    /**
     *  \brief  Event loop routine
     *
     *  Runs one loop.
     *
     *  \return \c true iff the event loop was shut down
     */
    bool routine();

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
