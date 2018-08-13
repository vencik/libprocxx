/**
 *  \file
 *  \brief  Event loop unit test
 *
 *  \date   2018/04/03
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
#include <thread>
#include <functional>
#include <chrono>

extern "C" {
#include <unistd.h>
}


using namespace libprocxx;
using namespace std::literals::chrono_literals;

using event_loop_t = io::event_loop<>;


/**
 *  \brief  Event loop template UT function
 *
 *  \return \c 0 in case of success, non-zero on failure
 */
template <class Fn>
int event_loop_ut(Fn fn) {
    event_loop_t eloop(10);

    when_leaving_scope([&eloop]() {
        eloop.shutdown_async();

        const bool shutdown = eloop.run_one();
        if (!shutdown) {
            std::cerr
                << __PRETTY_FUNCTION__
                << ": event loop not shut down when it should"
                << std::endl;

            throw std::logic_error(
                "libprocxx::io::event_pool: "
                "failed to shut down event pool");
        }
    });

    return fn(eloop);
}


/**
 *  \brief  Event loop \c run_one UT
 *
 *  \param  eloop  Event loop
 *
 *  \return \c 0 in case of success, non-zero on failure
 */
static int event_loop_run_one_ut(event_loop_t & eloop) {
    static const std::string msg = "Hello world!";

#if (1)
    std::thread client([]() {
        io::socket conn_sock(
            io::socket::domain::UNIX,
            io::socket::type::STREAM);

        std::this_thread::sleep_for(200ms);
        io::socket::address addr(conn_sock.family(), "unix_socket");
        if (!conn_sock.connect(addr)) {
            std::cerr
                << "Failed to connect to UNIS listen socket"
                << std::endl;
            return;
        }

        const auto * stop = msg.data() + msg.size();
        for (auto data = msg.data(); data < stop; ) {
            size_t size = 7;
            if (data + size > stop) size = stop - data;

            conn_sock.write(data, size);
            data += size;

            std::this_thread::sleep_for(1ms);
        }

        std::cout
            << "Client done"
            << std::endl;
    });
    when_leaving_scope([&client]() { client.join(); });
#endif

    io::socket listen_sock(
        io::socket::domain::UNIX,
        io::socket::type::STREAM);
    io::socket::address addr(listen_sock.family(), "unix_socket");
    when_leaving_scope([]() { ::unlink("unix_socket"); });

    listen_sock.bind(addr);
    listen_sock.listen(20);

    std::cout << "Listening for connections..." << std::endl;

    auto sock_err = listen_sock.accept();
    auto & sock   = std::get<0>(sock_err);

    std::cout << "Connection established" << std::endl;

    sock.status(io::file_descriptor::flag::NONBLOCK);
    eloop.add_socket(sock, event_loop_t::poll_event::IN,
    [&sock](auto handle, int events) {
        static std::string rmsg;

        char buffer[10];
        for (;;) {
            const auto rcnt_err = sock.read(buffer, sizeof(buffer));
            const auto rcnt     = std::get<0>(rcnt_err);
            const auto err      = std::get<1>(rcnt_err);

            // Read error (including EAGAIN)
            if (err) {
                std::cout
                    << "Socket read ended with code " << err
                    << std::endl;

                break;
            }

            // EoF
            else if (0 == rcnt) {
                std::cout
                    << "Socket closed"
                    << std::endl;

                if (rmsg != msg) {
                    std::cerr
                        << __PRETTY_FUNCTION__
                        << ": received \"" << rmsg
                        << "\", expected \"" << msg << "\""
                        << std::endl;

                    throw std::logic_error(
                        "libprocxx::io::event_pool: "
                        "received message doesn't match expectation");
                }

                break;
            }

            // Data read
            else {
                const std::string rdata(buffer, rcnt);

                std::cout
                    << "Socket data (" << rcnt << " B): \"" << rdata << "\""
                    << std::endl;

                rmsg += rdata;
            }
        }
    });

    for (size_t i = 0; i < 3; ++i) {
        bool shutdown = eloop.run_one();
        if (shutdown) {
            std::cerr
                << __PRETTY_FUNCTION__
                << ": event loop shut down prematurely"
                << std::endl;

            throw std::logic_error(
                "libprocxx::io::event_pool: "
                "premature event loop shut down");
        }
    }

    return 0;
}


// Main implementation
static int main_impl(int argc, char * const argv[]) {
    //unsigned rng_seed = std::time(nullptr);
    //std::srand(rng_seed);
    //std::cerr << "RNG seeded by " << rng_seed << std::endl;

    int exit_code = event_loop_ut(event_loop_run_one_ut);

    return exit_code;
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
