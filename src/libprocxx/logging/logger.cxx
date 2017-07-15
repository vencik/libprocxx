/**
 *  \file
 *  \brief  Logger
 *
 *  \date   2017/07/14
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

#include <libprocxx/logging/logger.hxx>
#include <libprocxx/mt/utils.hxx>

#include <cassert>
#include <iostream>  // TODO: remove


namespace libprocxx {
namespace logging {

static const char * const log_level_str[] = {
    /* ALWAYS */   "ALWAYS",
    /* FATAL */    "FATAL",
    /* ERROR */    "ERROR",
    /* WARNING */  "WARNING",
    /* INFO */     "INFO",
    /* DEBUG */    "DEBUG",
    /* DEBUG1 */   "DEBUG1",
    /* DEBUG2 */   "DEBUG2",
    /* DEBUG3 */   "DEBUG3",
    /* DEBUG4 */   "DEBUG4",
    /* DEBUG5 */   "DEBUG5",
    /* DEBUG6 */   "DEBUG6",
    /* DEBUG7 */   "DEBUG7",
    /* DEBUG8 */   "DEBUG8",
    /* DEBUG9 */   "DEBUG9",
};


const char * log_level2str(log_level::no no) {
    assert(0 <= no && no < sizeof(log_level_str) / sizeof(log_level_str[0]));
    return log_level_str[no];
}


logger worker::get_logger(const std::string & name, log_level::no level) {
    return logger(level, logger_handle(new logger_entry(name)), *this);
}


void worker::enqueue_message(message_ptr msg) {
    std::unique_lock<std::mutex> messages_lock(m_messages_mx);

    // Already shut down; invalid tread joining order
    if (m_shutdown)
        throw std::logic_error(
            "libprocxx::logging::worker: "
            "worker thread already joined; "
            "correct your tread joining order");

    m_messages.emplace(std::move(msg));
    m_messages_ready.notify_one();
}


void worker::routine(worker & self) {
    std::unique_lock<std::mutex> messages_lock(self.m_messages_mx);

    for (;;) {
        bool shutdown = false;

        // Read messages
        while (!self.m_messages.empty()) {
            auto msg_ptr = std::move(self.m_messages.front().msg_ptr);
            self.m_messages.pop();

            unlock4scope(messages_lock);

            auto msg = msg_ptr.get();

            // Shutdown indication; read-out everything and then finish
            if (nullptr == msg) {
                shutdown = true;
                continue;
            }

            // Format & Process message
            std::ostream & ostream = self.m_processor.get_stream();
            self.m_formatter.format(*msg, ostream);
            self.m_processor.process();
        }

        if (shutdown) break;  // might happen during message processing

        self.m_messages_ready.wait(messages_lock);
    }

    assert(self.m_messages.empty());

    self.m_shutdown = true;  // no more messages may be processed
}


worker::~worker() {
    // send the worker thread shutdown indication
    enqueue_message(message_ptr());

    // Join the worker thread
    m_worker.join();
}


void logger::log(
    log_level::no        level,
    const char *         file,
    int                  line,
    const char *         function,
    std::stringstream && msg_ss)
{
    m_worker.enqueue_message(
        worker::message_ptr(new worker::message(
            m_handle, level, file, line, function, std::move(msg_ss))));
}

}}  // end of namespaces logging libprocxx
