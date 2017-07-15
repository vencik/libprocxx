/**
 *  \file
 *  \brief  Asynchronous output file log message processor
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

#include <libprocxx/logging/file_processor.hxx>

#include <cassert>

extern "C" {
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
}


namespace libprocxx {
namespace logging {

file_processor::file_processor(
    const std::string & file,
    size_t              slot_cnt,
    size_t              buffer_size,
    unsigned            flush_delay)
:
    m_fd(-1),
    m_buffer_size(buffer_size),
    m_slots(slot_cnt, buffer_size),
    m_active_slots(nullptr),
    m_eldest_slot(0),
    m_1st_free_slot(0),
    m_flush_delay(flush_delay)
{
    // Standard file descriptors
    if ("/dev/stdout" == file)
        m_fd = STDOUT_FILENO;
    else if ("/dev/stderr" == file)
        m_fd = STDERR_FILENO;

    // Open log file
    else {
        m_fd = ::open(file.c_str(),
            O_WRONLY | O_CREAT | O_APPEND,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

        if (m_fd < 0)
            throw std::runtime_error(
                "libprocxx::logging::file_processor: "
                "failed to open log file");
    }

    // Create active AIO slots cyclic buffer
    m_active_slots = new aio_slot *[m_slots.size()];
}


/**
 *  \brief  Finalise async. writing if possible
 *
 *  \param  cb  Control block
 *
 *  \return \c true iff the operation was finalised
 */
static bool finalise_write(struct aiocb * cb) {
    const int status = aio_error(cb);

    switch (status) {
        case EINPROGRESS: // not done yet
            return false;

        case 0:          // done
        case ECANCELED:  // canceled
            return true;

        default:  // error
            throw std::runtime_error(
                "libprocxx::logging::file_processor: "
                "async. write failed");
    }
}


bool file_processor::finalise_writes() {
    while (m_eldest_slot != m_1st_free_slot) {
        aio_slot * slot = m_active_slots[m_eldest_slot];
        if (!finalise_write(&slot->cb)) return false;

        m_slots.release(*slot);
        m_eldest_slot = (m_eldest_slot + 1) % m_slots.size();
    }

    return true;
}


void file_processor::process() {
    finalise_writes();

    if (!m_slots.available()) return;  // no available I/O slot

    // Prepare control block
    aio_slot & slot   = m_slots.acquire();
    struct aiocb * cb = &slot.cb;

    ::memset(cb, 0, sizeof(*cb));

    cb->aio_fildes = m_fd;
    cb->aio_buf    = slot.buffer;
    cb->aio_nbytes = m_msg_ss.readsome((char *)cb->aio_buf, m_buffer_size);

    cb->aio_sigevent.sigev_notify = SIGEV_NONE;

    // Register slot as active
    m_active_slots[m_1st_free_slot] = &slot;
    m_1st_free_slot = (m_1st_free_slot + 1) % m_slots.size();

    // Initiate writing operation
    if (aio_write(cb))
        throw std::runtime_error(
            "libprocxx::logging::file_processor: "
            "failed to enqueue buffer");
}


void file_processor::flush() {
    // Finalise all pending writes
    while (!finalise_writes())
        std::this_thread::sleep_for(m_flush_delay);

    // Won't close standard file descriptors
    if (STDOUT_FILENO == m_fd) return;
    if (STDERR_FILENO == m_fd) return;

    // Flush buffered data
    if (::fsync(m_fd))
        throw std::runtime_error(
            "libprocxx::logging::file_processor: "
            "failed to flush log file");

    // Close log file
    if (::close(m_fd))
        throw std::runtime_error(
            "libprocxx::logging::file_processor: "
            "failed to close log file");

    m_fd = -1;
}


file_processor::~file_processor() {
    assert(m_fd == -1);  // check that flush was done

    delete[] m_active_slots;
}

}}  // end of namespaces logging libprocxx
