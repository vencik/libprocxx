#ifndef libprocxx__io__file_descriptor_hxx
#define libprocxx__io__file_descriptor_hxx

/**
 *  \file
 *  \brief  File descriptor wrapper
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

#include <tuple>
#include <cstddef>

extern "C" {
#include <fcntl.h>
}


namespace libprocxx {
namespace io {

class file_descriptor {
    public:

    /** File descriptor flags */
    enum flag {
        APPEND      = O_APPEND,     /**< File opened in append mode         */
        ASYNC       = O_ASYNC,      /**< Asynchronous, signal-driven I/O    */
        CLOEXEC     = O_CLOEXEC,    /**< Close-on-exec flag                 */
        CREATE      = O_CREAT,      /**< Create file if doesn't exist       */
        DIRECT      = O_DIRECT,     /**< Avoid caching (if possible)        */
        DIRECTORY   = O_DIRECTORY,  /**< The file is a directory            */
        DSYNC       = O_DSYNC,      /**< Synced data integrity I/O          */
        EXCL        = O_EXCL,       /**< Fail if file already exists        */
        LARGEFILE   = O_LARGEFILE,  /**< LFS support                        */
        NOATIME     = O_NOATIME,    /**< Don't update access time           */
        NOCTTY      = O_NOCTTY,     /**< Don't become process ctrl terminal */
        NOFOLLOW    = O_NOFOLLOW,   /**< Fail if the file is a symlink      */
        NONBLOCK    = O_NONBLOCK,   /**< Non-blocking mode                  */
        PATH        = O_PATH,       /**< FS path-related operations only    */
        SYNC        = O_SYNC,       /**< Synced file integrity I/O          */
        TMPFILE     = O_TMPFILE,    /**< Unnamed temporary file             */
        TRUNC       = O_TRUNC,      /**< Truncate file on open              */
    };  // end of enum flag

    protected:

    int m_fd;

    public:

    /** Constructor (closed FD) */
    file_descriptor(): m_fd(-1) {}

    /**
     *  \brief  Constructor
     *
     *  \param  fd  Raw file descripor
     */
    file_descriptor(int fd): m_fd(fd) {}

    /** Copy constructor */
    file_descriptor(const file_descriptor & orig);

    /** Move constructor */
    file_descriptor(file_descriptor && orig):
        m_fd(orig.m_fd)
    {
        orig.m_fd = -1;
    }

    /**
     *  \brief  Swap file descriptors
     *
     *  \param  arg  The other FD
     */
    void swap(file_descriptor & arg) {
        const int fd = m_fd;
        m_fd = arg.m_fd;
        arg.m_fd = fd;
    }

    /** Raw file descriptor getter */
    operator int () { return m_fd; }

    /** Raw file descriptor getter (const) */
    operator int () const { return m_fd; }

    /** Raw file descriptor setter */
    file_descriptor & operator = (int fd);

    /**
     *  \brief  Get file descriptor status flags
     *
     *  \return Status flags (bitwise OR)
     */
    int status() const;

    /**
     *  \brief  Set file descriptor status flags
     *
     *  \param  flags  Status flags (bitwise OR)
     */
    void status(int flags);

    /**
     *  \brief  Read from file descriptor
     *
     *  \param  data  Data buffer
     *  \param  size  Data buffer size
     *
     *  \return Amount of data read & errno (0 means no error)
     */
    std::tuple<size_t, int> read(void * data, size_t size);

    /**
     *  \brief  Write to file descriptor
     *
     *  \param  data  Data buffer
     *  \param  size  Data buffer size
     *
     *  \return Amount of data written & errno (0 means no error)
     */
    std::tuple<size_t, int> write(const void * data, size_t size);

    bool closed() const { return -1 == m_fd; }

    /** Close file descriptor */
    void close();

    /** Destructor (closes file descriptor) */
    ~file_descriptor() { close(); }

};  // end of class file_descriptor

}}  // end of namespace libprocxx::io

#endif  // end of #ifndef libprocxx__io__file_descriptor_hxx
