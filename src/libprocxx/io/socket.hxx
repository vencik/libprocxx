#ifndef libprocxx__io__socket_hxx
#define libprocxx__io__socket_hxx

/**
 *  \file
 *  \brief  Socket
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

#include "file_descriptor.hxx"

#include <regex>
#include <stdexcept>
#include <cassert>
#include <cstring>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netatalk/at.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <linux/x25.h>
#include <arpa/inet.h>
}


namespace libprocxx {
namespace io {

/** \cond */
namespace {

// struct sockaddr template initialisers

template <typename... Args>
class socket_address_unix {
    public:
    static void init(struct sockaddr_un & addr, Args...) {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "UNIX socket address: invalid initialisation");
    }
};  // end of template class socket_address_unix

template <>
class socket_address_unix<std::string> {
    public:
    static void init(struct sockaddr_un & addr, const std::string & path) {
        if (path.size() > sizeof(addr.sun_path) - 1)
            throw std::runtime_error(
                "libprocxx::io::socket::address: "
                "UNIX socket address: path too long");

        ::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    }
};  // end of template class socket_address_unix

template <>
class socket_address_unix<const char *> {
    public:
    static void init(struct sockaddr_un & addr, const char * path) {
        socket_address_unix<std::string>::init(addr, path);
    }
};  // end of template class socket_address_unix

template <>
class socket_address_unix<> {
    public:
    static void init(struct sockaddr_un & addr) {
        addr.sun_path[0] = '\0';
    }
};  // end of template class socket_address_unix


template <typename... Args>
class socket_address_ipv4 {
    public:
    static void init(struct sockaddr_in & addr, Args...) {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "IPv4 socket address: invalid initialisation");
    }
};  // end of template class socket_address_ipv4

template <>
class socket_address_ipv4<uint32_t, uint16_t> {
    public:
    static void init(
        struct sockaddr_in & addr,
        uint32_t             addr_pack,
        uint16_t             port)
    {
        addr.sin_addr.s_addr  = htonl(addr_pack);
        addr.sin_port         = htons(port);
    }
};  // end of template class socket_address_ipv4

template <>
class socket_address_ipv4<std::string, uint16_t> {
    public:
    static void init(
        struct sockaddr_in & addr,
        const std::string  & addr_str,
        uint16_t             port)
    {
        static const std::regex ipv4_addr(
            "^([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})$");

        std::smatch bref;
        if (!std::regex_match(addr_str, bref, ipv4_addr))
            throw std::runtime_error(
                "libprocxx::io::socket::address: "
                "IPv4 socket address: invalid string address specification");

        const int b4 = ::atoi(bref[1].str().c_str());
        const int b3 = ::atoi(bref[2].str().c_str());
        const int b2 = ::atoi(bref[3].str().c_str());
        const int b1 = ::atoi(bref[4].str().c_str());

        assert(0 <= b1 && 0 <= b2 && 0 <= b3 && 0 <= b4);
        if (!(256 > b1 && 256 > b2 && 256 > b3 && 256 > b4))
            throw std::runtime_error(
                "libprocxx::io::socket::address: "
                "IPv4 socket address: invalid address");

        const uint32_t addr_pack =
            (static_cast<uint32_t>(b1) <<  0) |
            (static_cast<uint32_t>(b2) <<  8) |
            (static_cast<uint32_t>(b3) << 16) |
            (static_cast<uint32_t>(b4) << 24);

        socket_address_ipv4<uint32_t, uint16_t>::init(addr, addr_pack, port);
    }
};  // end of template class socket_address_ipv4

template <>
class socket_address_ipv4<const char *, uint16_t> {
    public:
    static void init(
        struct sockaddr_in & addr,
        const char *         addr_str,
        uint16_t             port)
    {
        socket_address_ipv4<std::string, uint16_t>::init(addr, addr_str, port);
    }
};  // end of template class socket_address_ipv4

template <typename... Args>
class socket_address_ipv6 {
    public:
    static void init(struct sockaddr_in6 & addr, Args...) {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "IPv6 socket address: invalid initialisation");
    }
};  // end of template class socket_address_ipv6

template <>
class socket_address_ipv6<std::string, uint16_t> {
    public:
    static void init(
        struct sockaddr_in6 & addr,
        const std::string   & addr_str,
        uint16_t              port)
    {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "IPV6 socket address: UNIMPLEMENTED YET");
    }
};  // end of template class socket_address_ipv6


template <typename... Args>
class socket_address_appletalk {
    public:
    static void init(struct sockaddr_at & addr, Args...) {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "AppleTalk socket address: invalid initialisation");
    }
};  // end of template class socket_address_appletalk


template <typename... Args>
class socket_address_packet {
    public:
    static void init(struct sockaddr_ll & addr, Args...) {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "low-level packet socket address: invalid initialisation");
    }
};  // end of template class socket_address_packet


template <typename... Args>
class socket_address_x25 {
    public:
    static void init(struct sockaddr_x25 & addr, Args...) {
        throw std::logic_error(
            "libprocxx::io::socket::address: "
            "X25 socket address: invalid initialisation");
    }
};  // end of template class socket_address_x25

}  // end of anonymous namespace
/** \endcond */


class socket: public file_descriptor {
    public:

    /** Socket domain (aka family) */
    enum domain {
        NONE      = -1,              /**< Invalid socket                 */
        UNIX      = AF_UNIX,         /**< Unix socket (local socket)     */
        LOCAL     = AF_LOCAL,        /**< Local socket                   */
        INET      = AF_INET,         /**< IPv4 Internet protocols        */
        IPv4      = AF_INET,         /**< IPv4 Internet protocols        */
        INET6     = AF_INET6,        /**< IPv6 Internet protocols        */
        IPv6      = AF_INET6,        /**< IPv6 Internet protocols        */
        IPX       = AF_IPX,          /**< IPX - Novell protocols         */
        NETLINK   = AF_NETLINK,      /**< Kernel user interface device   */
        X25       = AF_X25,          /**< ITU-T X.25 / ISO-8208 protocol */
        AX25      = AF_AX25,         /**< Amateur radio AX.25 protocol   */
        ATMPVC    = AF_ATMPVC,       /**< Access to raw ATM PVCsi        */
        APPLETALK = AF_APPLETALK,    /**< AppleTalk                      */
        PACKET    = AF_PACKET,       /**< Low level packet interface     */
        ALG       = AF_ALG,          /**< Interface to kernel crypto API */
    };  // end of enum domain

    /** Socket type */
    enum type {
        STREAM    = SOCK_STREAM,     /**< Reliable, two-way stream         */
        DGRAM     = SOCK_DGRAM,      /**< Datagrams, unreliable            */
        SEQPACKET = SOCK_SEQPACKET,  /**< Datagrams, reliable, ordered     */
        RAW       = SOCK_RAW,        /**< Raw network protocol             */
        RDM       = SOCK_RDM,        /**< Datagrams, reliable, no ordering */
        NONBLOCK  = SOCK_NONBLOCK,   /**< Non-blocking (async.) I/O flag   */
        CLOEXEC   = SOCK_CLOEXEC,    /**< Close on exec. flag              */
    };  // end of enum type

    /** Socket protocol */
    enum protocol {
        DEFAULT = 0,    /**< Default protocol domain/type */
    };  // end of enum protocol

    /** Socket address */
    class address {
        friend class socket;

        private:

        /** Underlying struct sockaddr implementation */
        union impl {
            struct sockaddr_un    ux;           /**< UNIX socket address    */
            struct sockaddr_in    ipv4;         /**< IPv4 address           */
            struct sockaddr_in6   ipv6;         /**< IPv6 address           */
            struct sockaddr_at    appletalk;    /**< AppleTalk address      */
            struct sockaddr_ll    packet;       /**< Low-level device addr. */
            struct sockaddr_x25   x25;          /**< X25 socket address     */
        };  // end of union impl

        impl              m_impl;       /**< Space for address               */
        struct sockaddr * m_sockaddr;   /**< Polymorphism (C level)          */
        size_t            m_size;       /**< Real address implemenation size */

        /**
         *  \brief  Initialise implementation
         *
         *  \param  family  Socket address family
         */
        void init(socket::domain family) {
            switch (family) {
                case socket::domain::UNIX:
                    m_sockaddr = (struct sockaddr *)&m_impl.ux;
                    m_size     = sizeof(m_impl.ux);
                    break;

                case socket::domain::IPv4:
                    m_sockaddr = (struct sockaddr *)&m_impl.ipv4;
                    m_size     = sizeof(m_impl.ipv4);
                    break;

                case socket::domain::IPv6:
                    m_sockaddr = (struct sockaddr *)&m_impl.ipv6;
                    m_size     = sizeof(m_impl.ipv6);
                    break;

                case socket::domain::APPLETALK:
                    m_sockaddr = (struct sockaddr *)&m_impl.appletalk;
                    m_size     = sizeof(m_impl.appletalk);
                    break;

                case socket::domain::PACKET:
                    m_sockaddr = (struct sockaddr *)&m_impl.packet;
                    m_size     = sizeof(m_impl.packet);
                    break;

                case socket::domain::X25:
                    m_sockaddr = (struct sockaddr *)&m_impl.x25;
                    m_size     = sizeof(m_impl.x25);
                    break;

                default:
                    throw std::runtime_error(
                        "libprocxx::io::socket::address: "
                        "failed to create socket address: "
                        "family unimplemented");
            }

            ::memset(m_sockaddr, 0, m_size);
            m_sockaddr->sa_family = family;
        }

        /** Interim constructor flag */
        enum interim_constructor_t { interim_constructor };

        /**
         *  \brief  Interim constructor
         *
         *  Initialises correct implementation based on address family.
         *  However, the implementation stays uninitilised.
         *
         *  \param  family   Socket address family
         *  \param  interim  Interim constructor flag
         */
        address(socket::domain family, interim_constructor_t) { init(family); }

        public:

        /** Default address constructor */
        address(): m_sockaddr(nullptr), m_size(0) {}

        /**
         *  \brief  Common address constructor
         *
         *  Based on required address type, either constructs the address
         *  (if applicable arguments were supplied) or throws an exception.
         *
         *  \param  family  Socket address family
         *  \param  args    Address-specific constructor arguments
         */
        template <typename... Args>
        address(socket::domain family, Args... args) {
            init(family);

            switch (family) {
                case socket::domain::UNIX:
                    socket_address_unix<Args...>::init(
                        m_impl.ux, std::forward<Args>(args)...);
                    break;

                case socket::domain::IPv4:
                    socket_address_ipv4<Args...>::init(
                        m_impl.ipv4, std::forward<Args>(args)...);
                    break;

                case socket::domain::IPv6:
                    socket_address_ipv6<Args...>::init(
                        m_impl.ipv6, std::forward<Args>(args)...);
                    break;

                case socket::domain::APPLETALK:
                    socket_address_appletalk<Args...>::init(
                        m_impl.appletalk, std::forward<Args>(args)...);
                    break;

                case socket::domain::PACKET:
                    socket_address_packet<Args...>::init(
                        m_impl.packet, std::forward<Args>(args)...);
                    break;

                case socket::domain::X25:
                    socket_address_x25<Args...>::init(
                        m_impl.x25, std::forward<Args>(args)...);
                    break;

                default:
                    throw std::runtime_error(
                        "libprocxx::io::socket::address: "
                        "failed to create socket address: "
                        "family unimplemented");
            }
        }

        /** Address validity check */
        bool valid() const { return nullptr != m_sockaddr; }

        /** Address family */
        domain family() const {
            if (m_sockaddr) return (domain)m_sockaddr->sa_family;
            return domain::NONE;
        }

        /** Address real size getter */
        size_t size() const { return m_size; }

        private:

        /** C \c struct \c sockaddr pointer getter */
        struct sockaddr * sockaddr() const { return m_sockaddr; }

    };  // end of class address

    private:

    domain m_domain;

    /** Constructor (raw FD & domain initialisation) */
    socket(int fd, domain domain_):
        file_descriptor(fd),
        m_domain(domain_)
    {}

    public:

    /** Constructor (invalid socket) */
    socket(): m_domain(domain::NONE) {}

    /**
     *  \brief  Constructor
     *
     *  \param  domain_    Socket domain (see \ref domain)
     *  \param  type_      Socket type (see \ref type, possibly with flags OR)
     *  \param  protocol_  Socket protocol (see \ref protocol)
     */
    socket(
        domain   domain_,
        int      type_,
        protocol protocol_ = protocol::DEFAULT)
    :
        file_descriptor(::socket(domain_, type_, protocol_)),
        m_domain(domain_)
    {
        if (-1 == *this)
            throw std::runtime_error(
                "libprocxx::io::socket: "
                "failed to create socket");
    }

    /** Move constructor */
    socket(socket && orig):
        file_descriptor(std::move(orig)),
        m_domain(orig.m_domain)
    {
        orig.m_domain = domain::NONE;
    }

    /**
     *  \brief  Swap sockets
     *
     *  \param  arg  The other socket
     *
     *  \return \c *this (after swap)
     */
    void swap(socket & arg) {
        static_cast<file_descriptor *>(this)->swap(arg);

        const domain d = m_domain;
        m_domain = arg.m_domain;
        arg.m_domain = d;
    }

    domain family() const { return m_domain; }

    /**
     *  \brief  Bind socket to address
     *
     *  \param  addr  Socket address
     */
    void bind(const address & addr) {
        if (-1 == ::bind(*this, addr.sockaddr(), addr.size()))
            throw std::runtime_error(
                "libprocxx::io::socket::bind: "
                "failed to bind socket");
    }

    /**
     *  \brief  Connect to remote listen socket
     *
     *  Establish connection with remote party.
     *  Will only work for sockets of types \ref STREAM or \ref SEQPACKET.
     *
     *  \param  addr  Remote listen address
     *
     *  \return \c true iff connection was established
     */
    bool connect(const address & addr) {
        if (-1 == ::connect(*this, addr.sockaddr(), addr.size()))
            switch (errno) {
                case ECONNREFUSED:  // connection refused
                    return false;

                default:
                    throw std::runtime_error(
                        "libprocxx::io::socket::connect: "
                        "failed to connect to remote listen socket");
            }

        return true;
    }

    /**
     *  \brief  Listen for incomming connections on socket
     *
     *  Turns the connection socket into passive listen socket.
     *  Will only work for sockets of types \ref STREAM or \ref SEQPACKET.
     *
     *  \c backlog is the size of waiting inbound connections buffer.
     *  If more than \c backlog unaccepted connections are attempted,
     *  the connection may be refused (depending on the protocol).
     *
     *  \param  backlog  Listen backlog
     */
    void listen(int backlog) {
        if (-1 == ::listen(*this, backlog))
            throw std::runtime_error(
                "libprocxx::io::socket::listen: "
                "failed to listen on socket");
    }

    /**
     *  \brief  Accept connection on socket
     *
     *  Establishes connection on listen socket (see \ref listen).
     *  Will only work for sockets of types \ref STREAM or \ref SEQPACKET
     *  on which \ref listen was called first.
     *
     *  \return Remote party connection socket or closed socket in case
     *          connection was not accepted but the state is not erroneous
     *          (e.g. async accepting finished etc).
     *          The other returned value is \c errno (in case of error).
     */
    std::tuple<socket, int> accept() {
        // TODO: Use _GNU_SOURCE accept4 extension (see man 2 accept) if avail.
        int err = 0;

        const int fd = ::accept(*this, nullptr, nullptr);
        if (-1 == fd) {
            err = errno;

            switch (errno) {
                // Non-erroneous states (but no socket created)
                case EAGAIN:
                case ECONNABORTED:
                    break;

                default:
                    throw std::runtime_error(
                        "libprocxx::io::socket::accept: "
                        "failed to accept connection");
            }
        }

        return std::tuple<socket, int>(socket(fd, m_domain), err);
    }

    /**
     *  \brief  Accept connection on socket
     *
     *  Establishes connection on listen socket (see \ref listen).
     *  Will only work for sockets of types \ref STREAM or \ref SEQPACKET
     *  on which \ref listen was called first.
     *
     *  \return Remote party connection socket & address or closed socket
     *          and nvalid address in case connection was not accepted
     *          but the state is not erroneous
     *          (e.g. async accepting finished etc).
     *          The third returned value is \c errno (in case of error).
     */
    std::tuple<socket, address, int> accept_addr() {
        std::tuple<socket, address, int> sock_addr(
            socket(), address(m_domain, address::interim_constructor), 0);
        auto & sock = std::get<0>(sock_addr);
        auto & addr = std::get<1>(sock_addr);
        auto & err  = std::get<2>(sock_addr);

        sock.m_domain = m_domain;

        socklen_t addr_size = addr.size();
        sock.m_fd = ::accept(*this, addr.m_sockaddr, &addr_size);
        if (-1 == sock.m_fd) {
            err = errno;

            switch (err) {
                // Non-erroneous states (but no socket created)
                case EAGAIN:
                case ECONNABORTED:
                    break;

                default:
                    throw std::runtime_error(
                        "libprocxx::io::socket::accept: "
                        "failed to accept connection");
            }
        }

        if (addr_size > addr.size())
            throw std::runtime_error(
                "libprocxx::io::socket::accept: "
                "failed to set remote address");

        return sock_addr;
    }

};  // end of class socket

}}  // end of namespace libprocxx::io

#endif  // end of #ifndef libprocxx__io__socket_hxx
