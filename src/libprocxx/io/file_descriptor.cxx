/**
 *  \file
 *  \brief  File descriptor wrapper
 *
 *  \date   2018/04/01
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

#include "file_descriptor.hxx"

#include <stdexcept>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
}


namespace libprocxx {
namespace io {

file_descriptor::file_descriptor(const file_descriptor & orig) {
    close();
    m_fd = ::dup(orig.m_fd);
}


file_descriptor & file_descriptor::operator = (int fd) {
    close();
    m_fd = fd;

    return *this;
}


int file_descriptor::status() const {
    int flags = ::fcntl(m_fd, F_GETFL);
    if (-1 == flags)
        throw std::runtime_error(
            "libprocxx::io::file_descriptor::status: "
            "failed to get status flags");

    return flags;
}


void file_descriptor::status(int flags) {
    if (-1 == ::fcntl(m_fd, F_SETFL, flags))
        throw std::runtime_error(
            "libprocxx::io::file_descriptor::status: "
            "failed to set status flags");
}


void file_descriptor::close() {
    if (closed()) return;

    ::close(m_fd);
    m_fd = -1;
}


std::tuple<size_t, int> file_descriptor::read(void * data, size_t size) {
    const ssize_t rcnt = ::read(m_fd, data, size);

    return -1 == rcnt
        ? std::tuple<size_t, int>(0, errno)
        : std::tuple<size_t, int>((size_t)rcnt, 0);
}


std::tuple<size_t, int> file_descriptor::write(const void * data, size_t size) {
    const ssize_t wcnt = ::write(m_fd, data, size);

    return -1 == wcnt
        ? std::tuple<size_t, int>(0, errno)
        : std::tuple<size_t, int>((size_t)wcnt, 0);
}

}}  // end of namespace libprocxx::io
