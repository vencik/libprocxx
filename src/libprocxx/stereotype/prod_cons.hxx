#ifndef libprocxx__stereotype__prod_cons_hxx
#define libprocxx__stereotype__prod_cons_hxx

/**
 *  \file
 *  \brief  Producer--Consumer Stereotype
 *
 *  \date   2017/08/26
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

#include <libprocxx/meta/scope.hxx>
#include <libprocxx/mt/utils.hxx>
#include <libprocxx/stats/controller.hxx>

#include <cassert>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <utility>
#include <functional>


namespace libprocxx {
namespace stereotype {

using namespace std::rel_ops;  // generate relational operators


/** Consumer queue usage statistics */
class cons_queue_stats {
    private:

    /** Queue size constroller */
    using controller_t = stats::pid_controller<double>;

    public:

    /** Job queue statistics attributes */
    struct attributes {
        const size_t size_high_wm;   /**< Queue size high watermark     */
        size_t       sampling_freq;  /**< Sampling frequency^-1         */
        size_t       sampling_cnt;   /**< Runs till next computation    */
        size_t       size;           /**< Last queue size               */
        ssize_t      diff;           /**< Last queue size difference    */
        double       fill_rate;      /**< Queue fill rate (1 means HWM) */
        double       correction;     /**< Correction from controller    */

        /** Constructor */
        attributes(
            size_t size_high_wm_,
            size_t sampling_freq_)
        :
            size_high_wm  ( size_high_wm_  ),
            sampling_freq ( sampling_freq_ ),
            sampling_cnt  ( 1              ),
            size          ( 0              ),
            diff          ( 0              ),
            fill_rate     ( 0.0            )
        {}

    };  // end of struct attributes

    private:

    attributes   m_attrs;       /**< Job queue statistics attributes */
    controller_t m_controller;  /**< Job queue size controller       */

    /** See \ref update */
    ssize_t update_impl(size_t size) {
        m_attrs.size = size;

        double delta_fill_rate = -m_attrs.fill_rate;
        m_attrs.fill_rate = (double)size / (double)m_attrs.size_high_wm;
        delta_fill_rate += m_attrs.fill_rate;

        m_attrs.correction = m_controller(delta_fill_rate);

        return (ssize_t)-m_attrs.correction;
    }

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  sampling_freq  Initial sampling frequency^-1 (>0)
     */
    cons_queue_stats(size_t size_high_wm, size_t sampling_freq):
        m_attrs(size_high_wm, sampling_freq),
        m_controller(30, 0.1, 1, 1, 1)
    {
        if (0 == m_attrs.sampling_freq)
            throw std::logic_error(
                "libprocxx::mt::thread_pool_base::cons_queue_stats: "
                "sampling frequency (inverted) must be non-zero");
    }

    /**
     *  \brief  Update statistics
     *
     *  Updates the queue usage statistics and computes how many
     *  new treads should be created/terminated in order to manage the queue
     *  size trend.
     *
     *  \param  size  Current queue size
     *
     *  \return Suggested thread count difference
     */
    inline ssize_t update(size_t size) {
        if (--m_attrs.sampling_cnt) return 0;

        m_attrs.sampling_cnt = m_attrs.sampling_freq;
        return update_impl(size);
    }

    /** Provide attributes */
    inline const attributes & attrs() const { return m_attrs; }

};  // end of class cons_queue_stats


namespace {

/** Consumer queue wrapper (for MT) */
template <class Q>
struct cons_queue_mt {
    using queue_t = Q;                             /**< Consumer queue type */
    using item_t  = typename queue_t::value_type;  /**< Consumed item type  */

    const size_t            high_wm;  /**< Queue size high watermark */
    queue_t                 q;        /**< Queue implementation      */
    bool                    closed;   /**< Queue was closed          */
    mutable std::mutex      mx;       /**< Operation mutex           */
    std::condition_variable cv_cons;  /**< Item(s) in queue signal   */
    std::condition_variable cv_prod;  /**< Push more items signal    */

    /** Constructor */
    cons_queue_mt(size_t high_wm_):
        high_wm ( high_wm_ ),
        closed  ( false    )
    {}

    /** Queue size */
    inline size_t size() const { return q.size(); }

    /** Push an \c item to queue */
    inline void push(item_t && item) {
        q.emplace(std::move(item));
    }

    /** Pop an item from queue */
    inline item_t pop() {
        when_leaving_scope([this]() { q.pop(); });
        return std::move(q.front());
    }

};  // end of struct cons_queue_mt

}  // end of anonymous namespace


/**
 *  \brief  Consumer
 *
 *  The consumer consumes items from a queue.
 *  If the queue is not empty, a consumer therad is woken, pops an item
 *  from the queue and consumes it.
 *
 *  Consumer (optionally) implements a mechanism for dynamic allocation and
 *  release of more threads when the queue starts to grow or shrink,
 *  respectively.
 *
 *  \tparam  Q  Consumer queue
 */
template <class Q>
class consumer {
    private:

    using queue_mt_t = cons_queue_mt<Q>;  /**< Queue MT wrapper */

    public:

    /** Timeout type (us) */
    using timeout_t = std::chrono::duration<long, std::micro>;

    using item_t  = typename queue_mt_t::item_t;   /**< Item type  */
    using queue_t = typename queue_mt_t::queue_t;  /**< Queue type */

    /** Routine type */
    using routine_t = std::function<void()>;

    /** Start thread function type */
    using start_thread_t = std::function<bool(routine_t)>;

    /** Consume queue item function type */
    using consume_t = std::function<void(item_t &&)>;

    /** Consumer attributes */
    struct attributes {
        const size_t    tmax;        /**< Max. number of threads           */
        size_t          tcnt;        /**< Started threads count            */
        size_t          treserved;   /**< Reserved ready threads           */
        const size_t    keepalive4;  /**< Thread keep alive min. item cnt. */
        const timeout_t idle_tout;   /**< Max. thread idle time            */
        bool            terminated;  /**< The pool was terminated          */

        /** Constructor */
        attributes(
            size_t max_,
            size_t reserved_,
            size_t keepalive4_,
            long   idle_tout_us)
        :
            tmax       ( max_         ),
            tcnt       ( 0            ),
            treserved  ( reserved_    ),
            keepalive4 ( keepalive4_  ),
            idle_tout  ( idle_tout_us ),
            terminated ( false        )
        {}

    };  // end of struct attributes

    /** Consumer info */
    struct info {
        friend class consumer;

        /** Consumer queue info */
        struct queue_info {
            const size_t high_wm;  /**< Size high watermark */
            const size_t size;     /**< Size                */
            const bool   closed;   /**< Status              */

            /** Constructor */
            queue_info(const queue_mt_t & q_mt):
                high_wm ( q_mt.high_wm ),
                size    ( q_mt.size()  ),
                closed  ( q_mt.closed  )
            {}

        };  // end of struct queue_info

        const attributes attrs;  /**< Consumer attributes */
        const queue_info queue;  /**< Consumer queue info */

        private:

        /** Constructor */
        info(const consumer & cons):
            attrs ( cons.m_attrs ),
            queue ( cons.m_queue )
        {}

    };  // end of struct info

    private:

    start_thread_t                  m_start_thread;   /**< Thread starter    */
    consume_t                       m_consume;        /**< Q. item consumer  */
    attributes                      m_attrs;          /**< Attributes        */
    queue_mt_t                      m_queue;          /**< Item queue        */
    mutable std::mutex              m_mx;             /**< Operation mutex   */
    mutable std::condition_variable m_cv_terminated;  /**< Terminated signal */

    public:

    /**
     *  \brief  Constructor
     *
     *  TODO: review this and move it to the consumer documentation section
     *
     *  The consumer uses at least 1 running thread.
     *  The threads pop items from \ref m_queue and consumes them (in parallel).
     *  If the queue is empty, the thread will eiter suspend itself till more
     *  items are available or it terminates if it consumed too few items
     *  (so it's probably not necessary to keep it running).
     *
     *  Pre-set limits on the consumer thread count are preserved.
     *  Note that, depending on (system) resources, the required amount of
     *  threads may not be available at the time; this will not cause an error.
     *  Thread will also terminate if it was waiting idle for too long on empty
     *  consumer queue.
     *
     *  Consumer allows to limit the number of threads that run (at a time).
     *  Setting the limit to an arbitrarily high number (much higher
     *  than the possible amount of parallel threads the system will allow
     *  you to create) effectively disables it.
     *  Although you may choose not to, you are STRONGLY ENCOURAGED
     *  to set a sane limit, namely if you intend to use auto-scaling.
     *  Having too many parallel threads is unhealthy; it will force your system
     *  to constantly deal with context switching and cache misses.
     *  Just note that at any time, the max. thread number must be greater or
     *  equal to the number of reserved threads (see \ref reserve).
     *
     *  Consumer also allows you to set its queue size high watermark.
     *  Normally, consumer queue will not be allowed to grow past the high
     *  watermark.
     *  Unless the producer forces the queue push operation, the \ref push
     *  funcion will either block untill the queue size drops or refuse
     *  to push the item to the queue.
     *  This mechanism allows for propagation of overloaded state
     *  of the consumer to the producer (e.g. for congestion control).
     *  Again, setting it to an arbitrarily high number means that
     *  it won't take effect.
     *
     *  The function will attempt to acquire \c reserved threads.
     *  However, if the system has not enough resources, the amount of
     *  acquired threads may be lower.
     *
     *  The constructor requires passing thread acquisition function/functor.
     *  The function is expected to start one detached thread that will execute
     *  the supplied routine on the supplied argument.
     *  If the thread can't be started, it will return false.
     *
     *  The constructor also requires implementation of the item consuming
     *  function/functor passed.
     *  The consuming callable type is \c void(item_t&&) .
     *
     *  \param  start_thread  Threads acquisition function
     *  \param  consume       Queue item consuming function
     *  \param  high_wm       Consumer queue high watermark
     *  \param  smpl_freq     Sampling frequency for queue statistics
     *  \param  max           Limit on running threads
     *  \param  reserved      Initial number of reserved threads (>0)
     *  \param  keepalive4    Min. number of items consumed in a row
     *                        to keep thread processing item queue
     *  \param  idle_tout_us  Max. thread idle time in us
     *                        before it ceases to process item queue
     */
    consumer(
        start_thread_t start_thread,
        consume_t      consume,
        size_t         high_wm,
        size_t         max,
        size_t         reserved,
        size_t         keepalive4,
        long           idle_tout_us)
    :
        m_start_thread(start_thread),
        m_consume(consume),
        m_attrs(max, reserved, keepalive4, idle_tout_us),
        m_queue(high_wm)
    {
        if (!m_attrs.treserved)
            throw std::logic_error(
                "libprocxx::stereotype::consumer: "
                "at least 1 thread must be reserved at all times");

        if (m_attrs.treserved > m_attrs.tmax)
            throw std::logic_error(
                "libprocxx::stereotype::consumer: "
                "number of reserved threads must not exceed the limit");

        reserve();
    }

    /** Copying is forbidden */
    consumer(const consumer & ) = delete;

    /**
     *  \brief  Run consumer routine in a new thread
     *
     *  \return 0 on success, 1 on failure
     */
    inline size_t run_routine() {
        return m_start_thread(std::bind(&consumer::routine, this)) ? 0 : 1;
    }

    /**
     *  \brief  Start threads
     *
     *  The function increases the number of threads by \c n
     *  and attempts to acquire more threads (if necessary) to make sure that
     *  that amount is available.
     *  If that amount of threads is not available, however, the function
     *  can't force their creation.
     *
     *  Note that the function will honour the thread count limit
     *  (i.e. won't start more threads then the limit allows).
     *
     *  \param  n  Number of threads to start
     *
     *  \return Number currently running treads
     */
    size_t start(size_t n = 1) {
        size_t tcnt;

        {
            lock4scope(m_mx);

            if (m_attrs.tcnt + n > m_attrs.tmax)
                n = m_attrs.tmax - m_attrs.tcnt;

            tcnt = m_attrs.tcnt += n;  // pre-increase thread count
        }

        size_t failed = 0;
        for (size_t i = 0; i < n; ++i)
            failed += run_routine();

        if (failed) {
            lock4scope(m_mx);

            tcnt = m_attrs.tcnt -= failed;  // fix thread count
        }

        return tcnt;
    }

    /**
     *  \brief  Reserve more threads
     *
     *  The function increases the number of reserved threads by \c n
     *  and attempts to acquire more threads (if necessary) to make sure that
     *  the required amount is available.
     *  If that amount of threads is not available, however, the function
     *  can't force their creation.
     *
     *  Note that with \c n of 0 (the default), the function effectively tries
     *  to make sure the currently reserved threads are acquired.
     *  Also note that the function will honour the thread count limit
     *  (i.e. won't reserve more threads then the limit allows to start).
     *
     *  \param  n  Number of required additional reserved threads
     *
     *  \return Number of currently reserved threads
     */
    size_t reserve(size_t n = 0) {
        size_t treserved;

        {
            lock4scope(m_mx);

            if (m_attrs.treserved + n > m_attrs.tmax)
                n = m_attrs.tmax - m_attrs.treserved;

            treserved = m_attrs.treserved += n;
            n = m_attrs.treserved > m_attrs.tcnt
                ? m_attrs.treserved - m_attrs.tcnt
                : 0;

            m_attrs.tcnt += n;  // pre-increase thread count
        }

        size_t failed = 0;
        for (size_t i = 0; i < n; ++i)
            failed += run_routine();

        if (failed) {
            lock4scope(m_mx);

            m_attrs.tcnt -= failed;  // fix thread count
        }

        return treserved;
    }

    /**
     *  \brief  Unreserve \c n threads
     *
     *  The threads will not immediately terminate, but they will eventually,
     *  as soon as they become idle for long enough or it's decided
     *  that they are no longer required.
     *
     *  \return Number of currently reserved threads
     */
    size_t unreserve(size_t n) {
        lock4scope(m_mx);

        return m_attrs.treserved -= n;
    }

    /**
     *  \brief  Push an item to consumer queue
     *
     *  The \c item is normally pushed only if the queue size is below the high
     *  watermark.
     *  If the queue size high watermark is reached then, unless the \c force
     *  flag is set, the function call shall be suspended for \c tout_us us.
     *  0 means no blocking, negative value (the default) means no time limit.
     *
     *  This mechanism allows for propagation of the situation
     *  when the consumer queue processing threads are overloaded.
     *  In such a situation, the producer(s) may be slowed down (willingly
     *  or by blocking) unless they choose to force item pushing.
     *
     *  Note that the function will throw an exception if the queue
     *  was already closed.
     *  Otherwise, if in \c force mode or negative \c tout_us is set,
     *  the function guarantees that the \c item will be scheduled
     *  (i.e. the function will return \c true ).
     *
     *  Also note that, unless \c force mode is used, the function quarantees
     *  that the queue shall not grow over the high watermark.
     *
     *  \param  item     Pushed item
     *  \param  force    Push \c item despite the queue high WM was reached
     *  \param  tout_us  Wait timeout [us] (not meaningful if \c force is set)
     *
     *  \return \c true iff \c item was pushed
     */
    bool push(item_t && item, bool force = false, long tout_us = -1) {
        auto lock = get_lock4scope(m_queue.mx);

        do {  // pragmatic do ... while (0) loop allowing for breaks
            if (force) break;  // push the item no matter what

            if (m_queue.size() < m_queue.high_wm) break;         // go ahead
            if (0 == tout_us)                     return false;  // don't wait

            // Wait indefinitely
            if (tout_us < 0) {
                do m_queue.cv_prod.wait(lock);
                while (m_queue.size() >= m_queue.high_wm);

                break;  // push the item now
            }

            // Wait for at most tout_us us
            std::cv_status cv_status =
                m_queue.cv_prod.wait_for(lock, timeout_t(tout_us));

            // Timeout or queue still too long
            if (std::cv_status::timeout == cv_status ||
                m_queue.size()          >= m_queue.high_wm)
            {
                return false;
            }

        } while (0);  // end of pragmatic loop

        if (m_queue.closed)
            throw std::logic_error(
                "libprocxx::stereotype::consumer::push: "
                "attempt to push to closed queue");

        m_queue.push(std::move(item));
        m_queue.cv_cons.notify_one();

        return true;  // item pushed
    }

    /** Push an item copy */
    inline bool push(
        const item_t & item,
        bool           force   = false,
        long           tout_us = -1)
    {
        return push(item_t(item), force, tout_us);
    }

    /**
     *  \brief  Get consumer info
     *
     *  Note that the info is collected under lock (in order to be consistent).
     *  Gathering the info too often may (and will) negatively influence
     *  the consumer efficiency.
     */
    inline info get_info() const {
        lock4scope(m_mx);
        lock4scope(m_queue.mx);

        return info(*this);
    }

    /**
     *  \brief  Shut the consumer down
     *
     *  After this, any attempt to push another item to the consumer queue
     *  will fail with an exception.
     */
    inline void shutdown() {
        lock4scope(m_queue.mx);
        m_queue.closed = true;
        m_queue.cv_cons.notify_all();
    }

    /** Destructor (waits for termination of all threads) */
    ~consumer() {
        shutdown();

        auto lock = get_lock4scope(m_mx);
        while (!m_attrs.terminated)
            m_cv_terminated.wait(lock);
    }

    private:

    /**
     *  \brief  Consumer queue processing routine
     *
     *  The function implements the consumer queue processing thread routine.
     *  It returns when the thread decides that it should terminate.
     *  This may be because consumer shutdown was signalised
     *  or because it is used too sparsely (see \c keepalive4).
     *
     *  \param  keepalive4  Minimal number of items consumed in a row
     *                      to keep the thread running
     *  \param  idle_tout   Max. thread idle time in us
     *                      before it ceases to process item queue
     *
     *  \return \c true iff shutdown was signalised
     */
    bool process_queue(const size_t keepalive4, const timeout_t & idle_tout) {
        std::cv_status cv_status;

        auto lock = get_lock4scope(m_queue.mx);

        do {
            size_t item_cnt = 0;  // items consumed in a row (without waiting)

            size_t q_size;
            while ((q_size = m_queue.size())) {
                // Pop an item from queue
                item_t item(m_queue.pop());

                // Signalise to producer (one item was already popped)
                if (q_size <= m_queue.high_wm) m_queue.cv_prod.notify_one();

                // Consume the item
                unlock4scope(lock);
                m_consume(std::move(item));
                ++item_cnt;
            }

            if (m_queue.closed) return true;  // shutdown

            // The thread is used sparsely, terminate preemptively
            if (item_cnt < keepalive4) return false;

            cv_status = m_queue.cv_cons.wait_for(lock, idle_tout);

        } while (std::cv_status::no_timeout == cv_status);

        return false;  // idle for too long
    }

    /**
     *  \brief  Consumer thread main routine
     *
     *  The thread routine runs the queue processing routine and deals with
     *  it's return status.
     *  Should the thread be kept running, the queue processing routine
     *  is restarted; otherwise, the thread terminates.
     */
    void routine() {
        auto lock = get_lock4scope(m_mx);

        size_t keepalive4 = m_attrs.keepalive4;
        auto   idle_tout  = m_attrs.idle_tout;

        // Process queue
        do {
            unlock4scope(lock);

            if (process_queue(keepalive4, idle_tout)) break;  // shutdown

        // Prevent the thread from terminating lest there's not enough treads
        } while (m_attrs.tcnt <= m_attrs.treserved);

        // Last thread terminates
        if (0 == --m_attrs.tcnt) {
            m_attrs.terminated = true;
            m_cv_terminated.notify_one();
        }
    }

};  // end of template class consumer

}}  // end of namespace stereotype libprocxx

#endif  // end of #ifndef libprocxx__stereotype__prod_cons_hxx
