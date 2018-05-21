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

#include "libprocxx/io/event_loop.hxx"

#include <cassert>
#include <stdexcept>

extern "C" {
#include <unistd.h>
}


namespace libprocxx {
namespace io {

event_loop::event_loop(size_t max_events, bool edge_triggered):
    m_max_events(max_events),
    m_defaults(defaults_init(edge_triggered)),
    m_sigpipe(pipe::flag::NONBLOCK),
    m_epoll_fd(::epoll_create1(0))
{
    if (m_epoll_fd.closed())
        throw std::runtime_error(
            "libprocxx::io::event_loop: "
            "failed to create epoll");

    m_events.reserve(m_max_events);
    add_fd(m_sigpipe.read_end(), poll_event::IN, nullptr);
}


event_loop::entries_t::iterator event_loop::register_fd(
    file_descriptor &      fd,
    int                    events,
    event_loop::callback_t callback)
{
    entries_t::iterator entry_iter;
    entries_t::iterator entries_end;

    {
        lock4scope(m_mutex);

        entry_iter  = m_entries.emplace(m_entries.end(), fd, callback);
        entries_end = m_entries.end();
    }

    if (entries_end != entry_iter)
        add_fd(fd, events, &*entry_iter);

    return entry_iter;
}


void event_loop::add_fd(
    file_descriptor & fd,
    int               events,
    void *            data_ptr)
{
    struct epoll_event ev;
    ev.events   = events | m_defaults;
    ev.data.ptr = data_ptr;

    if (-1 == ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev))
        throw std::runtime_error(
            "libprocxx::io::event_loop: "
            "failed to add file descriptor");
}


bool event_loop::routine() {
    const int ev_cnt = ::epoll_wait(m_epoll_fd, &m_events[0], m_max_events, -1);
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
        ent.callback(ent.fd, event.events);
    }

    return false;
}


void event_loop::shutdown_async() {
    {
        lock4scope(m_mutex);
        if (m_sigpipe.write_end().closed()) return;  // already shut down
    }

    signalise(TERMINATE);
}


void event_loop::shutdown() {
    auto lock = get_lock4scope(m_mutex);

    if (m_sigpipe.write_end().closed()) return;  // already shut down

    lock.unlock();
    signalise(TERMINATE);
    lock.lock();

    while (!m_sigpipe.read_end().closed())
        m_shutdown.wait(lock);

    m_sigpipe.write_end().close();
}

}}  // end of namespace libprocxx::io
