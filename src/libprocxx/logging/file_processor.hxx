#ifndef libprocxx__logging__file_processor_hxx
#define libprocxx__logging__file_processor_hxx

/**
 *  \file
 *  \brief  Asynchronous output file log message processor
 *
 *  Using aio library.
 *
 *  \date   2017/07/15
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
#include <libprocxx/container/slots.hxx>

#include <sstream>
#include <chrono>

extern "C" {
#include <aio.h>
}


namespace libprocxx {
namespace logging {

/** Simple processor providing \c std::ostream instance */
class file_processor: public processor {
    private:

    /** AIO slot */
    struct aio_slot {
        struct aiocb cb;      /**< AIO control block */
        char *       buffer;  /**< Buffer            */

        /** Constructor */
        aio_slot(size_t buffer_size): buffer(new char[buffer_size]) {}

        /** Destructor */
        ~aio_slot() { delete[] buffer; }

    };  // end of struct aio_slot

    std::stringstream m_msg_ss;  /**< Message stream      */
    int               m_fd;      /**< Log file descriptor */

    /** Async. I/O slots */
    using aio_slots = libprocxx::container::slots<aio_slot>;

    const size_t m_buffer_size;    /**< Async I/O slot buffer size   */
    aio_slots    m_slots;          /**< All async. I/O slots         */
    aio_slot **  m_active_slots;   /**< Active async. I/O slots      */
    size_t       m_eldest_slot;    /**< Eldest active I/O slot index */
    size_t       m_1st_free_slot;  /**< 1st free I/O slot index      */

    /** Sleep delay between flush attempts */
    const std::chrono::microseconds m_flush_delay;

    /**
     *  \brief  Attempt to finalise pending async. writes
     *
     *  \return \c true iff all pending writes were finalised
     */
    bool finalise_writes();

    public:

    /**
     *  \brief  Constructor
     *
     *  Pre-allocates \c slot_cnt times \c buffer_size bytes
     *  for the async. operations.
     *  Opens log file.
     *
     *  \param  file         Log file path (or /dev/std{out|err})
     *  \param  slot_cnt     Number of async. I/O slots
     *  \param  buffer_size  Size of async. I/O buffers
     *  \param  flush_delay  Flush delay in microseconds
     */
    file_processor(
        const std::string & file,
        size_t              slot_cnt    = 32,
        size_t              buffer_size = 512,
        unsigned            flush_delay = 50);

    /** Log stream provider */
    std::ostream & get_stream() { return m_msg_ss; }

    /** Process stream (no need to do anything) */
    void process();

    /**
     *  \brief  Finalise pending async. operations
     *
     *  Waits for finalisation of all pending async. operations.
     *  Closes log file descriptor.
     */
    void flush();

    /** Cleans up */
    ~file_processor();

};  // end of class file_processor

}}  // end of namespaces logging libprocxx

#endif  // end of #ifndef libprocxx__logging__file_processor_hxx
