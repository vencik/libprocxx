/**
 *  \file
 *  \brief  Simple event loop based TCP server unit test
 *
 *  \date   2018/05/29
 *  \author Vaclav Krpec  <vencik@razdva.cz>
 *
 *
 *  LEGAL NOTICE
 *
 *  Copyright (c) 2018, Vaclav Krpec
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

#include <libprocxx/io/event_loop.hxx>
#include <libprocxx/io/socket.hxx>
#include <libprocxx/meta/scope.hxx>

#include <iostream>
#include <stdexcept>
#include <exception>
#include <mutex>
#include <thread>
#include <functional>
#include <chrono>
#include <list>
#include <sstream>
#include <cassert>


using namespace libprocxx;
using namespace std::literals::chrono_literals;


/** \cond */
// Message logging

static std::mutex log_mutex;  // log output stream mutex

inline void log_assemble(std::stringstream & log_line) {}

template <typename T, typename... Args>
inline void log_assemble(
    std::stringstream & log_line,
    const T &           arg1,
    const Args &...     args)
{
    log_line << arg1;
    log_assemble(log_line, args...);
}

template <typename... Args>
void log(const Args &... args) {
    std::stringstream log_line;
    log_assemble(log_line, args...);

    lock4scope(log_mutex);
    std::cout << log_line.str() << std::endl;
}

/** \endcond */


/**
 *  \brief  TCP server
 */
class tcp_server {
    private:

    /** Event loop data */
    struct el_data {
        io::socket  socket;     /**< Socket                 */
        std::string message;    /**< Message (for writing)  */
        size_t      offset;     /**< Message offset         */

        /** Constructor */
        el_data(io::socket && sock):
            socket(std::move(sock)),
            offset(0)
        {}

    };  // end of struct el_data

    using event_loop_t      = io::event_loop<el_data>;
    using el_entry_handle_t = event_loop_t::entry_handle_t;

    const std::string   m_addr_str;     /**< Listen address             */
    const uint16_t      m_port;         /**< Listen port                */
    const unsigned      m_backlog;      /**< Backlog size               */
    event_loop_t        m_eloop;        /**< Event loop                 */
    char                m_buffer[32];   /**< Data buffer                */
    std::thread         m_worker;       /**< Worker thread              */

    public:

    /**
     *  \brief  TCP server constructor
     *
     *  \param  addr_str  Listen address
     *  \param  port      Listen port
     *  \param  backlog   Backlog size
     *  \param  maxev     Events per iteration limit
     */
    tcp_server(
        const std::string & addr_str,
        uint16_t            port,
        unsigned            backlog = 20,
        unsigned            maxev   = 10)
    :
        m_addr_str(addr_str),
        m_port(port),
        m_backlog(backlog),
        m_eloop(maxev)
    {}

    /** Start server */
    void start() {
        io::socket listen_sock(
            io::socket::domain::IPv4, io::socket::type::STREAM);
        io::socket::address addr(listen_sock.family(), m_addr_str, m_port);

        m_worker = std::thread(std::bind(&event_loop_t::run, &m_eloop));

        listen_sock.bind(addr);
        listen_sock.listen(m_backlog);

        listen_sock.status(io::file_descriptor::flag::NONBLOCK);
        m_eloop.add_socket(listen_sock, event_loop_t::poll_event::IN,
            std::bind(&tcp_server::accept, this,
                std::placeholders::_1, std::placeholders::_2),
            std::move(listen_sock));

        log("TCP server started, listening");
    }

    /** Stop server */
    void stop() {
        if (!m_worker.joinable()) return;

        m_eloop.shutdown();
        m_worker.join();

        log("TCP server stopped");
    }

    ~tcp_server() {
        stop();
    }

    private:

    /** Connecton acceptor */
    void accept(el_entry_handle_t entry_handle, int events) {
        auto & listen_sock = entry_handle->data.socket;

        for (;;) {
            auto sock_err = listen_sock.accept();
            auto & sock   = std::get<0>(sock_err);
            auto & err    = std::get<1>(sock_err);

            switch (err) {
                case 0:      break;   // connection accepted
                case EAGAIN: return;  // no more connections to accept

                default:
                    log("WARNING: Got error ", err,
                    " on connection accept, continuing");

                    assert(sock.closed());
                    continue;
            }

            log("Connection established");

            sock.status(io::file_descriptor::flag::NONBLOCK);
            m_eloop.add_socket(sock, event_loop_t::poll_event::IN,
                std::bind(&tcp_server::io, this,
                    std::placeholders::_1, std::placeholders::_2),
                std::move(sock));
        }
    }

    /** Data I/O */
    void io(el_entry_handle_t entry_handle, int events) {
        if (event_loop_t::poll_event::IN  & events) read(entry_handle);
        if (event_loop_t::poll_event::OUT & events) write(entry_handle);
    }

    /** Data read */
    void read(el_entry_handle_t entry_handle) {
        log("Async data read");

        auto & sock = entry_handle->data.socket;
        auto & msg  = entry_handle->data.message;

        for (;;) {
            const auto rcnt_err = sock.read(m_buffer, sizeof(m_buffer));
            const auto rcnt     = std::get<0>(rcnt_err);
            const auto err      = std::get<1>(rcnt_err);

            // Read error (including EAGAIN)
            if (err) {
                log("Socket read ended with code ", err);

                return;
            }

            // EoF
            if (0 == rcnt) {
                log("Connection closed by remote party");

                m_eloop.remove(entry_handle);
                return;
            }

            // Read data (and collect message)
            const std::string rdata(m_buffer, rcnt);
            log("Read data (", rcnt, " B): \"", rdata, "\"");
            msg += rdata;

            // Initiate writing
            if ('>' == msg[msg.size() - 1]) {
                log("Initialising async data write");

                m_eloop.modify(entry_handle,
                    event_loop_t::poll_event::IN |
                    event_loop_t::poll_event::OUT);
            }
        }
    }

    /** Data write */
    void write(el_entry_handle_t entry_handle) {
        log("Async data write");

        auto & sock   = entry_handle->data.socket;
        auto & msg    = entry_handle->data.message;
        auto & offset = entry_handle->data.offset;

        for (;;) {
            // Simulate data chunking
            size_t max_wcnt = msg.size() - offset;
            if (max_wcnt > 8) max_wcnt = 8;

            const auto   wcnt_err = sock.write(msg.data() + offset, max_wcnt);
            const auto   wcnt     = std::get<0>(wcnt_err);
            const auto   err      = std::get<1>(wcnt_err);

            // Write error (including EAGAIN)
            if (err) {
                log("Socket write ended with code ", err);

                return;
            }

            const std::string wdata(msg.data() + offset, wcnt);
            log("Wrote data (", wcnt, " B): \"", wdata, "\"");

            offset += wcnt;
            if (offset >= msg.size()) {
                log("Roundtrip done");

                m_eloop.modify(entry_handle, event_loop_t::poll_event::IN);
                return;
            }
        }
    }

};  // end of class tcp_server


/** Client **/
class tcp_client {
    private:

    /** Common client message part */
    static const std::string s_msg;

    const std::string   m_id;           /**< Client ID                  */
    const std::string   m_addr_str;     /**< Server listen address      */
    const uint16_t      m_port;         /**< Server listen port         */
    std::thread         m_thread;       /**< Client thread              */

    public:

    /**
     *  \brief  Client
     *
     *  \param  id        Client ID
     *  \param  addr_str  Server listen address
     *  \param  port      Server listen port
     */
    tcp_client(
        const std::string & id,
        const std::string & addr_str,
        uint16_t            port)
    :
        m_id(id),
        m_addr_str(addr_str),
        m_port(port)
    {}

    void routine() {
        log("Client ", m_id, " starts");

        const std::string my_msg = m_id + ": " + s_msg + " <" + m_id + '>';

        io::socket sock(io::socket::domain::IPv4, io::socket::type::STREAM);
        io::socket::address addr(sock.family(), m_addr_str, m_port);
        if (!sock.connect(addr))
            throw std::runtime_error(
                "Failed to connect to IPv4 listen socket");

        log("Client ", m_id, " sends data: \"", my_msg, '"');

        const auto * stop = my_msg.data() + my_msg.size();
        for (auto data = my_msg.data(); data < stop; ) {
            size_t size = 7;
            if (data + size > stop) size = stop - data;

            sock.write(data, size);
            data += size;

            std::this_thread::sleep_for(100us);
        }

        log("Client ", m_id, " receives data");

        char buffer[my_msg.size()];
        std::string copy_msg;
        copy_msg.reserve(my_msg.size());

        while (copy_msg.size() < my_msg.size()) {
            const auto rcnt_err = sock.read(buffer, sizeof(buffer));
            const auto rcnt     = std::get<0>(rcnt_err);
            const auto err      = std::get<1>(rcnt_err);

            // Read error
            if (err) {
                log("Client ", m_id, ": socket read ended with code ", err);
                throw std::runtime_error(
                    "Failed to read data from server");
            }

            const std::string rdata(buffer, rcnt);
            log("Client ", m_id, ": read data (", rcnt, " B): \"", rdata, "\"");

            copy_msg += rdata;
        }

        if (my_msg != copy_msg) {
            log("Client ", m_id, ": ping-pong mismatch: \"", my_msg,
                "\" vs \"", copy_msg, '"');
            throw std::runtime_error(
                "Roundtrip message check failed");
        }

        log("Client ", m_id, " ends");
    }

    /** Execute client session (async) */
    void run() {
        m_thread = std::thread(std::bind(&tcp_client::routine, this));
    }

    /** Wait for client session to end */
    void wait() {
        if (!m_thread.joinable()) return;

        m_thread.join();
    }

    ~tcp_client() {
        wait();
    }

};  // end of class tcp_client

const std::string tcp_client::s_msg("Hello world!");


// Main implementation
static int main_impl(int argc, char * const argv[]) {
    static const std::string addr      = "127.0.0.1";
    static const uint16_t    port      = 8080;
    static const size_t      client_no = 10;

    // Create server
    tcp_server server(addr, port);

    // Create clients
    std::list<tcp_client> clients;
    for (size_t i = 0; i < client_no; ++i) {
        std::stringstream client_id;
        client_id << '#' << i;

        clients.emplace_back(client_id.str(), addr, port);
    }

    server.start();  // start server

    // Run client sessions and wait for them to finish
    for (auto & client: clients) client.run();
    for (auto & client: clients) client.wait();

    server.stop();  // stop server
    return 0;
}

// Main exception-safe wrapper
int main(int argc, char * const argv[]) {
    int exit_code = 128;

    try {
        exit_code = main_impl(argc, argv);
    }
    catch (const std::exception & x) {
        std::cerr
            << "Standard exception caught: "
            << x.what()
            << std::endl;
    }
    catch (...) {
        std::cerr
            << "Unhandled non-standard exception caught"
            << std::endl;
    }

    std::cerr << "Exit code: " << exit_code << std::endl;
    return exit_code;
}
