#ifndef libprocxx__logging__logger_hxx
#define libprocxx__logging__logger_hxx

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

#include <libprocxx/meta/macro.hxx>

#include <sstream>
#include <string>
#include <list>
#include <memory>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>


/**
 *  \brief  Logging macro
 *
 *  The macro checks if the log level is enough for the message to be logged
 *  in the 1st place.
 *  If not, then logging is abandoned (so even the arguments are not computed),
 *  ensuring maximal logging efficiency.
 *  Otherwise, the log message is composed via \c std::stringstream and sent
 *  to the logging back-end for the actual logging.
 *
 *  \param  logger  Logging front-end
 *  \param  lvl     Message log level
 *  \param  args    \c << concatenation of log messagge components
 */
#define libprocxx__logging__log(logger, lvl, args) \
    do { \
        std::stringstream msg; msg << args; \
        if ((logger).level >= (lvl)) { \
            (logger).log((lvl), __SRCFILE__, __LINE__, __PRETTY_FUNCTION__, \
                std::move(msg)); \
        } \
    } while (0)

// Convenience logging macros, using \c LIBPROCXX__LOGGING__LOGGER macro
/** \cond */
#define always(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::ALWAYS, args)
#define fatal(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::FATAL, args)
#define error(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::ERROR, args)
#define warn(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::WARNING, args)
#define info(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::INFO, args)
#define debug(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG, args)
#define debug0(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG0, args)
#define debug1(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG1, args)
#define debug2(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG2, args)
#define debug3(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG3, args)
#define debug4(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG4, args)
#define debug5(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG5, args)
#define debug6(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG6, args)
#define debug7(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG7, args)
#define debug8(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG8, args)
#define debug9(args) \
    libprocxx__logging__log((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::DEBUG9, args)
/** \endcond */

/**
 *  \brief  Log & throw an exception
 *
 *  \param  logger  Logging front-end
 *  \param  level   Message log level
 *  \param  xtype   Exception type
 *  \param  what    Exception message (\c const \c char \c * )
 */
#define libprocxx__logging__xthrow(logger, level, xtype, what) \
    do { \
        libprocxx__logging__log((logger), (level), \
            "Throwing exception " STR(xtype) ": " << what); \
        throw xtype(what); \
    } while (0)

// Throwing macros
/** \cond */
#define xthrow_fatal(xtype, what) \
    libprocxx__logging__xthrow((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::FATAL, (xtype), (what))
#define xthrow_error(xtype, what) \
    libprocxx__logging__xthrow((LIBPROCXX__LOGGING__LOGGER), \
        libprocxx::logging::log_level::ERROR, (xtype), (what))

// By default, throwing an exception is considered to be a fatal event
#define xthrow(xtype, what) xthrow_fatal((xtype), (what))
/** \endcond */


namespace libprocxx {
namespace logging {

/** Log levels */
struct log_level {
    enum no {
        ALWAYS = 0,  /**< Log no matter what                       */
        FATAL,       /**< Fatal error, process shall go down       */
        ERROR,       /**< Error, process might recover             */
        WARNING,     /**< Suspicious, but not necessarily an error */
        INFO,        /**< Info message                             */
        DEBUG,       /**< Debugging message, high priority         */

        DEBUG0 = DEBUG,  /**< Debugging message level 0 */
        DEBUG1,          /**< Debugging message level 1 */
        DEBUG2,          /**< Debugging message level 2 */
        DEBUG3,          /**< Debugging message level 3 */
        DEBUG4,          /**< Debugging message level 4 */
        DEBUG5,          /**< Debugging message level 5 */
        DEBUG6,          /**< Debugging message level 6 */
        DEBUG7,          /**< Debugging message level 7 */
        DEBUG8,          /**< Debugging message level 8 */
        DEBUG9,          /**< Debugging message level 9 */
    };  // end of enum no
};  // end of struct log_level


/** Get log level string */
const char * log_level2str(log_level::no no);


// Forward declarations
/** \cond */
class formatter;
class processor;
class logger;
/** \endcond */


/**
 *  \brief  Logging worker (abstract base)
 *
 *  The logging itself is done by a logging worker thread.
 *  The worker handles registration of loggers, manages the log message queue
 *  and its implementations define the actual message logging.
 *  Message formatting is injected.
 */
class worker {
    public:

    /** Logger entry */
    struct logger_entry {
        const std::string name;  /**< Logger name */

        logger_entry(const std::string & name_):
            name(name_)
        {}

    };  // end of struct logger_entry

    /** Logger handle */
    using logger_handle  = std::shared_ptr<logger_entry>;

    /** Time point */
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    /** Log message (as passed via the log message queue to worker) */
    struct message {
        logger_handle           logger;         /**< Logger handle    */
        const log_level::no     level;          /**< Log level        */
        const time_point        time;           /**< Time of creation */
        const char *            file;           /**< File path        */
        const int               line;           /**< Line number      */
        const char *            function;       /**< Function         */
        std::stringstream       msg_ss;         /**< Message stream   */

        message(
            logger_handle        logger_,
            log_level::no        level_,
            const char *         file_,
            int                  line_,
            const char *         function_,
            std::stringstream && msg_ss_)
        :
            logger   ( logger_                          ),
            level    ( level_                           ),
            time     ( std::chrono::system_clock::now() ),
            file     ( file_                            ),
            line     ( line_                            ),
            function ( function_                        ),
            msg_ss   ( std::move(msg_ss_)               )
        {}

    };  // end of struct message

    using message_ptr = std::unique_ptr<message>;  /**< Message pointer */

    /** Message handle (wraps over \c message_ptr to make it queueabe) */
    struct message_handle {
        message_ptr msg_ptr;  /**< Message pointer */

        /** Constructor */
        message_handle(message_ptr msg_ptr_):
            msg_ptr(std::move(msg_ptr_))
        {}

    };  // end of struct nessage_handle

    using message_queue = std::queue<message_handle>;  /**< Massage queue  */

    private:

    /**
     *  \brief  Worker thread routine
     *
     *  \param  self  Worker
     */
    static void routine(worker & self);

    formatter &              m_formatter;       /**< Log message formatter */
    processor &              m_processor;       /**< Log message processor */
    bool                     m_shutdown;        /**< Shut down flag        */
    message_queue            m_messages;        /**< Log message queue     */
    std::mutex               m_messages_mx;     /**< Queue op. mutex       */
    std::condition_variable  m_messages_ready;  /**< Queue action required */
    std::thread              m_worker;          /**< Worker thread         */

    public:

    /**
     *  \brief  Constructor
     *
     *  Starts the worker thread.
     *
     *  \param  formatter_  Log message formatter
     *  \param  processor_  Log message processor
     */
    worker(
        formatter & formatter_,
        processor & processor_)
    :
        m_formatter ( formatter_ ),
        m_processor ( processor_ ),
        m_shutdown  ( false      ),

        m_worker(routine, std::ref(*this))  // start worker thread
    {}

    /** Copying is prohibited */
    worker(const worker & ) = delete;

    /**
     *  \brief  Create new logger
     *
     *  \param  name   Logger name
     *  \param  level  Log level
     *
     *  \return New logger registered with the worker
     */
    logger get_logger(const std::string & name, log_level::no level);

    /**
     *  \brief  Enqueue log message
     *
     *  \param  msg  Message
     */
    void enqueue_message(message_ptr msg);

    /** Destructor (joins the worker thread) */
    virtual ~worker();

};  // end of class worker


/** Logger */
class logger {
    friend class worker;

    public:

    log_level::no level;  /**< Log level */

    private:

    worker::logger_handle m_handle;  /**< The logger's handle */
    worker &              m_worker;  /**< Logging worker      */

    /**
     *  \brief  Constructor
     *
     *  \param  level   Log level
     *  \param  handle  The logger's handle
     *  \param  worker  Worker
     */
    logger(
        log_level::no         level_,
        worker::logger_handle handle,
        worker &              worker)
    :
        level    ( level_ ),
        m_handle ( handle ),
        m_worker ( worker )
    {}

    public:

    /**
     *  \brief  Log message
     *
     *  \param  level     Message log level
     *  \param  file      Current file name
     *  \param  line      Current line number
     *  \param  function  Current function
     *  \param  msg_ss    Message string stream
     */
    void log(
        log_level::no        level,
        const char *         file,
        int                  line,
        const char *         function,
        std::stringstream && msg_ss);

};  // end of class logger


/** Log message formatter */
class formatter {
    public:

    /**
     *  \brief  Format log message
     *
     *  \param  msg      Log message
     *  \param  ostream  output stream
     */
    virtual void format(worker::message & msg, std::ostream & ostream) = 0;

};  // end of class formatter


/** Log message processor */
class processor {
    public:

    /**
     *  \brief  Provide output stream (for formatter)
     *
     *  \return Output stream
     */
    virtual std::ostream & get_stream() = 0;

    /**
     *  \brief  Process stream
     *
     *  \param  msg  Log message
     */
    virtual void process() = 0;

};  // end of class processor

}}  // end of namespaces logging libprocxx

#endif  // end of #ifndef libprocxx__logging__logger_hxx
