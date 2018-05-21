#ifndef libprocxx__io__pipe_hxx
#define libprocxx__io__pipe_hxx

/**
 *  \file
 *  \brief  Pipe (a FIFO file descriptors pair)
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

#include <tuple>
#include <cstddef>

extern "C" {
#include <unistd.h>
#include <fcntl.h>
}


namespace libprocxx {
namespace io {

/**
 *  \brief  Pipe
 */
class pipe {
    public:

    enum flag {
        CLOEXEC  = O_CLOEXEC,   /**< Close on exec flag    */
        DIRECT   = O_DIRECT,    /**< Packet mode flag      */
        NONBLOCK = O_NONBLOCK,  /**< Non-blocking I/O flag */
    };  // end of enum flag

    private:

    /** Pipe file descriptor pair */
    struct fd {
        file_descriptor write;    /**< Pipe write end */
        file_descriptor read;     /**< Pipe read end  */

        fd(std::tuple<int, int> rw):
            write(std::get<1>(rw)),
            read(std::get<0>(rw))
        {}

    };  // end of struct fd

    fd m_fd;  /**< File descriptors */

    /**
     *  \brief  Pipe file descriptors constructor
     *
     *  \param  flags  Construction flags (bitwise OR-ed, see \ref flag)
     *
     *  \return Raw read & write file descriptors
     */
    static std::tuple<int, int> init(int flags);

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  flags  Construction flags (bitwise OR-ed, see \ref flag)
     */
    pipe(int flags = 0): m_fd(init(flags)) {}

    /** Read end getter */
    file_descriptor & read_end() { return m_fd.read; }

    /** Write end getter */
    file_descriptor & write_end() { return m_fd.write; }

    /**
     *  \brief  Write to pipe
     *
     *  \param  data  Data buffer
     *  \param  size  Data buffer size
     *
     *  \return Number of bytes written & errno (0 means no error)
     */
    std::tuple<size_t, int> write(const void * data, size_t size) {
        return m_fd.write.write(data, size);
    }

    /**
     *  \brief  Read from pipe
     *
     *  \param  data  Data buffer
     *  \param  size  Data buffer size
     *
     *  \return Number of bytes read & errno (0 means no error)
     */
    std::tuple<size_t, int> read(void * data, size_t size) {
        return m_fd.read.read(data, size);
    }

};  // end of class pipe

}}  // end of namespace libprocxx::io

#endif  // end of #ifndef libprocxx__io__pipe_hxx
