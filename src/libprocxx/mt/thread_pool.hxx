#ifndef libprocxx__mt__thread_pool_hxx
#define libprocxx__mt__thread_pool_hxx

/**
 *  \file
 *  \brief  Thread pool
 *
 *  \date   2017/07/26
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

#include <libprocxx/mt/utils.hxx>

#include <cassert>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <utility>


namespace libprocxx {
namespace mt {

namespace {

using namespace std::rel_ops;  // generate relational operators


/**
 *  \brief  Thread pool
 *
 *  The pool keeps ready threads and job queue.
 *  Scheduling a job means to push a REFERENCE to it to the queue.
 *  Consequently, a therad is woken, pops a job ref. from the queue
 *  and executes it.
 *  Note that the job object MUST EXIST during the execution, otherwise
 *  the reference would become invalid.
 *
 *  The pool implements a mechanism for dynamic creation and destruction
 *  of more threads when the job queue starts to grow or shrink, respectively.
 *
 *  \tparam  Job  The job executed by threaded pools (a void() callable)
 *  \tparam  Q    Queue implementation
 */
template <class Q>
class thread_pool_base {
    public:

    /** Timeout type (us) */
    using timeout_t = std::chrono::duration<double, std::micro>;

    private:

    using job_queue_t = Q;                           /**< Job queue type */
    using job_t = typename job_queue_t::value_type;  /**< Job type       */

    /** Job queue statistics */
    class queue_stats {
        public:

        /** Job queue statistics attributes */
        struct attributes {
            const size_t size_high_wm;   /**< Queue size high watermark  */
            size_t       sampling_freq;  /**< Sampling frequency^-1      */
            size_t       sampling_cnt;   /**< Runs till next computation */
            size_t       size;           /**< Last queue size            */
            ssize_t      diff;           /**< Last queue size difference */

            /** Constructor */
            attributes(
                size_t size_high_wm_,
                size_t sampling_freq_)
            :
                size_high_wm  ( size_high_wm_  ),
                sampling_freq ( sampling_freq_ ),
                sampling_cnt  ( 1              ),
                size          ( 0              ),
                diff          ( 0              )
            {}

        };  // end of struct attributes

        private:

        attributes m_attrs;  /**< Job queue statistics attributes */

        /** See \ref update */
        ssize_t update_impl(size_t size) {
            ssize_t diff  = size - m_attrs.size;
            ssize_t diff2 = diff - m_attrs.diff;

            m_attrs.size = size;
            m_attrs.diff = diff;

            // Queue size is over high watermark
            if (size > m_attrs.size_high_wm) return 5;

            // Queue is shrinking and it seems to be a trend
            if (diff < 0 && diff2 <= 0) return -1;

            // Queue is short and doesn't grow much
            if (size < m_attrs.sampling_freq &&
                diff < (ssize_t)m_attrs.sampling_freq)
            {
                return 0;
            }

            // Queue is growing but the growth is slowing down
            if (diff > 0 && diff2 < 0) return 1;

            // Queue growth is increasing rapidly
            if (diff2 > 2) return 3;

            return 0;  // let's postpone the decision
        }

        public:

        /**
         *  \brief  Constructor
         *
         *  \param  sampling_freq  Initial sampling frequency^-1 (>0)
         */
        queue_stats(size_t size_high_wm, size_t sampling_freq):
            m_attrs(size_high_wm, sampling_freq)
        {
            if (0 == m_attrs.sampling_freq)
                throw std::logic_error(
                    "libprocxx::mt::thread_pool_base::queue_stats: "
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

    };  // end of class queue_stats

    /** Job queue wrapper for MT */
    struct job_queue_mt {
        job_queue_t             q;           /**< Queue implementation   */
        bool                    closed;      /**< Queue was closed       */
        mutable std::mutex      mx;          /**< Operation mutex        */
        std::condition_variable cv_consume;  /**< Job(s) in queue signal */
        std::condition_variable cv_produce;  /**< Push more jobs signal  */
        queue_stats             stats;       /**< Queue usage statistics */

        /** Constructor */
        job_queue_mt(size_t high_wm, size_t sampling_freq):
            closed(false),
            stats(high_wm, sampling_freq)
        {}

        /** Queue size */
        inline size_t size() const { return q.size(); }

        /** Push a \c job to queue */
        inline void push(job_t && job, size_t priority = 1) {
            q.emplace(std::move(job));
        }

        /** Pop a job from queue */
        inline job_t pop() {
            job_t job(std::move(q.front()));
            q.pop();
            return std::move(job);
        }

    };  // end of struct job_queue_mt

    public:

    /** Thread pool attributes */
    struct attributes {
        const size_t    tmax;        /**< Max. number of threads  */
        size_t          tcnt;        /**< Started threads count   */
        size_t          treserved;   /**< Reserved ready threads  */
        const timeout_t idle_tout;   /**< Max. thread idle time   */
        bool            terminated;  /**< The pool was terminated */

        /** Constructor */
        attributes(
            size_t max_,
            size_t reserved_,
            double idle_tout_)
        :
            tmax       ( max_       ),
            tcnt       ( 0          ),
            treserved  ( reserved_  ),
            idle_tout  ( idle_tout_ ),
            terminated ( false      )
        {}

    };  // end of struct attributes

    /** Thread pool info */
    struct info {
        friend class thread_pool_base;

        /** Job queue info */
        struct job_queue_info {
            using qstats_attrs = typename queue_stats::attributes;

            const size_t       size;    /**< Size       */
            const bool         closed;  /**< Status     */
            const qstats_attrs stats;   /**< Statistics */

            /** Constructor */
            job_queue_info(const job_queue_mt & job_queue):
                size   ( job_queue.size()        ),
                closed ( job_queue.closed        ),
                stats  ( job_queue.stats.attrs() )
            {}

        };  // end of struct job queue info

        const attributes     attrs;      /**< Thread pool attributes */
        const job_queue_info job_queue;  /**< Job queue info         */

        private:

        /** Constructor */
        info(const thread_pool_base & tpool):
            attrs     ( tpool.m_attrs     ),
            job_queue ( tpool.m_job_queue )
        {}

    };  // end of struct info

    private:

    attributes              m_attrs;       /**< Pool attributes      */
    job_queue_mt            m_job_queue;   /**< Job queue            */
    mutable std::mutex      m_mx;          /**< Operation mutex      */
    std::condition_variable m_cv;          /**< Status change signal */

    public:

    /**
     *  \brief  Constructor
     *
     *  The pool must contain at least 1 ready (i.e. running) threads.
     *  The threads pop jobs from \ref m_job_queue and execute them (in parallel).
     *  If it's decided that new threads should be created, the creation
     *  is facilitated by pushing another job (threads creation) to the queue.
     *  In such case, the queue should support prioritisation of the job
     *  so that the threads are created ASAP.
     *
     *  The pool allows to limit the number of threads that run (at a time).
     *  The default limit is an arbitrarily high number (much higher
     *  than the possible amount of parallel threads the system will allow
     *  you to create).
     *  Although you may use the default setting, you are STRONGLY ENCOURAGED
     *  to set a sane limit, namely if you intend to use auto-scaling pipes.
     *  Having too many parallel threads is unhealthy; it will force your system
     *  to constantly deal with context switching and cache misses.
     *  Just note that at any time, the max. thread number must be greater or
     *  equal to the number of reserved threads (see \ref reserve).
     *
     *  The pool also allows you to set job queue size high watermark.
     *  If queue grows over this number of waiting jobs, the pool will start
     *  to create new threads in order to deal with the queue size peak.
     *  Note that again, you probably will need this if you push high
     *  amount of jobs at once and expect the pool to run them ASAP.
     *  By default, it is set to an arbitrarily high number and therefore
     *  won't take effect.
     *
     *  The constructor will attempt to synchronously start \c reserved threads.
     *  However, if the system has not enough resources, the amount of reserved
     *  (and running) threads may be lower.
     *
     *  \param  high_wm    Job queue high watermark (none by default)
     *  \param  smpl_freq  Sampling frequency for queue statistics
     *  \param  max        Limit on created threads (unlimited by default)
     *  \param  reserved   Initial number of reserved threads (>0)
     *  \param  idle_tout  Max. thread idle time in us (before stopped)
     */
    thread_pool_base(
        size_t high_wm   = SIZE_MAX,
        size_t smpl_freq = 10,
        size_t max       = SIZE_MAX,
        size_t reserved  = 1,
        double idle_tout = 10000)
    :
        m_attrs(max, reserved, idle_tout),
        m_job_queue(high_wm, smpl_freq)
    {
        if (!m_attrs.treserved)
            throw std::logic_error(
                "libprocxx::mt::thread_pool_base: "
                "at least 1 thread must be reserved at all times");

        if (m_attrs.treserved > m_attrs.tmax)
            throw std::logic_error(
                "libprocxx::mt::thread_pool_base: "
                "number of reserved threads must not exceed the limit");

        reserve(m_attrs.treserved);
    }

    /** Copying is forbidden */
    thread_pool_base(const thread_pool_base & ) = delete;

    /** Destructor (waits for termination of all threads) */
    ~thread_pool_base() {
        shutdown();

        std::unique_lock<std::mutex> lock(m_mx);
        while (!m_attrs.terminated) m_cv.wait(lock);
    }

    private:

    /**
     *  \brief  Job queue processing routine
     *
     *  The function implements the job queue processing by a pooled thread.
     *  It returns when the thread decides that it should terminate (either
     *  because thread pool shutdown was signalised or because threads count
     *  is unnecessarily high).
     *
     *  \return \c true iff shutdown was signalised
     */
    bool process_queue() {
        std::cv_status cv_status;

        std::unique_lock<std::mutex> lock(m_job_queue.mx);
        do {
            size_t job_queue_size;
            while ((job_queue_size = m_job_queue.size())) {
                // Update job queue statistics and schedule thread creation
                ssize_t tcnt_diff = m_job_queue.stats.update(
                    job_queue_size);

                // Start more threads or stop if there are too many
                if (tcnt_diff) {
                    // Delegate the job
                    m_job_queue.cv_consume.notify_one();

                    if (tcnt_diff < 0) return false;  // resign preemptively

                    // Start more threads
                    unlock4scope(lock);

                    // Honour the thread count limit though
                    {
                        std::unique_lock<std::mutex> lock(m_mx);

                        if (m_attrs.tmax < m_attrs.tcnt + tcnt_diff)
                            tcnt_diff = m_attrs.tmax - m_attrs.tcnt;

                        m_attrs.tcnt += tcnt_diff;  // pre-increase thread count
                    }

                    size_t failed = tcnt_diff - start_threads(tcnt_diff);

                    if (failed) {
                        std::unique_lock<std::mutex> lock(m_mx);

                        m_attrs.tcnt -= failed;  // fix thread count
                    }

                    continue;
                }

                // Pop job from queue
                job_t job(m_job_queue.pop());

                // Signalise to a job producer
                const size_t job_queue_size_hwm =
                    m_job_queue.stats.attrs().size_high_wm;
                if (job_queue_size <= job_queue_size_hwm)  // one already popped
                    m_job_queue.cv_produce.notify_one();

                // Execute job
                unlock4scope(lock);
                job();
            }

            if (m_job_queue.closed) return true;  // shutdown

            cv_status = m_job_queue.cv_consume.wait_for(
                lock, m_attrs.idle_tout);

        } while (std::cv_status::no_timeout == cv_status);

        return false;  // idle for too long
    }

    /**
     *  \brief  Pooled thread main routine
     *
     *  The thread routine runs the job queue processing routine and deals with
     *  it's return status.
     *  Should the thread be kept running, the job queue processing routine
     *  is restarted; otherwise, the thread terminates.
     *
     *  \param  tpool  Thread pool
     */
    static void routine(thread_pool_base & tpool) {
        for (;;) {
            bool shutdown = tpool.process_queue();

            // Thread termination (?)
            {
                std::unique_lock<std::mutex> lock(tpool.m_mx);

                // Thread is reserved, no shutdown
                if (!shutdown && tpool.m_attrs.tcnt <= tpool.m_attrs.treserved)
                    continue;  // re-start queue processing

                // Last thread stops
                if (0 == --tpool.m_attrs.tcnt) {
                    tpool.m_attrs.terminated = true;
                    tpool.m_cv.notify_one();
                }
            }

            break;  // thread routine terminates
        }
    }

    /**
     *  \brief  Start \c n threads
     *
     *  The function attempts to start \c n new threads.
     *  It will stop as soon as new thread can't be created.
     *
     *  \return Number of created threads
     */
    size_t start_threads(size_t n) {
        size_t cnt = 0;
        for (; cnt < n; ++cnt) {
            try {
                std::thread(routine, std::ref(*this)).detach();
            }

            // The thread could not be started
            catch (std::system_error & x) {
                break;  // give up
            }
        }

        return cnt;
    }

    public:

    /**
     *  \brief  Reserve more threads
     *
     *  The function increases the number of reserved thread by \c n
     *  and attempt to start more threads (if necessary) to make sure that
     *  the required amount is available.
     *  However, if the necessary threads can't be created by the system,
     *  the function can't force it to.
     *
     *  \param  n  Number of additionally reserved threads
     *
     *  \return Number of currently reserved threads
     */
    size_t reserve(size_t n) {
        size_t treserved;

        {
            std::unique_lock<std::mutex> lock(m_mx);

            if (m_attrs.treserved + n > m_attrs.tmax)
                throw std::logic_error(
                    "libprocxx::mt::thread_pool_base::reserve: "
                    "number of reserved threads must not exceed the limit");

            treserved = m_attrs.treserved += n;
            n = m_attrs.treserved > m_attrs.tcnt
                ? m_attrs.treserved - m_attrs.tcnt
                : 0;
            m_attrs.tcnt += n;  // pre-increase thread count
        }

        size_t failed = n - start_threads(n);

        if (failed) {
            std::unique_lock<std::mutex> lock(m_mx);

            treserved = m_attrs.treserved -= failed;
            m_attrs.tcnt -= failed;
        }

        return treserved;
    }

    /**
     *  \brief  Unreserve \c n threads
     *
     *  The threads will not immediately end, but they will, as soon as they
     *  become idle for long enough or it's decided that they are no longer
     *  required.
     */
    void unreserve(size_t n) {
        std::unique_lock<std::mutex> lock(m_mx);

        m_attrs.treserved -= n;
    }

    /**
     *  \brief  Schedule a job
     *
     *  Job is normally scheduled only if the job queue size is below the high
     *  watermark.
     *  If job queue size high watermark is reached then, unless the \c force
     *  flag is set, the function call shall be suspended for \c tout us.
     *  0 means no blocking, negative value (the default) means no time limit.
     *
     *  This mechanism allows for propagation of the situation when the job
     *  processing back-end threads are overwhelmed with jobs.
     *  In such a situation, the job producer(s) may be slowed down (willingly
     *  or by blocking) unless they choose to force jobs scheduling.
     *
     *  Note that the function will throw an exception if the job queue
     *  was already closed.
     *  Otherwise, if in \c force mode or negative \c tout is set,
     *  the function guarantees that te job will be scheduled
     *  (i.e. the function will return \c true ).
     *
     *  Also note that, unless \c force mode is used, the function quarantees
     *  that the job queue shall not grow over the high watermark.
     *
     *  \param  job    Scheduled job
     *  \param  force  Schedule job despite the job queue high WM was reached
     *  \param  tout   Wait timeout [us] (not meaningful if \c force is set)
     *
     *  \return \c true iff the job was scheduled
     */
    bool schedule(job_t && job, bool force = false, double tout = -1.0) {
        std::unique_lock<std::mutex> lock(m_job_queue.mx);

        do {  // pragmatic do ... while (0) loop allowing for breaks
            if (force) break;  // schedule job no matter what

            const size_t q_size_hwm = m_job_queue.stats.attrs().size_high_wm;

            if (m_job_queue.size() < q_size_hwm) break;         // green light
            if (0.0 == tout)                     return false;  // don't wait

            // Wait indefinitely
            if (tout < 0) {
                do m_job_queue.cv_produce.wait(lock);
                while (m_job_queue.size() >= q_size_hwm);

                break;  // schedule job now
            }

            // Wait for at most tout us
            std::cv_status cv_status =
                m_job_queue.cv_produce.wait_for(lock, timeout_t(tout));

            // Timeout or queue still too long
            if (std::cv_status::timeout == cv_status ||
                m_job_queue.size()      >= q_size_hwm)
            {
                return false;
            }

        } while (0);  // end of pragmatic loop

        if (m_job_queue.closed)
            throw std::logic_error(
                "libprocxx::mt::thread_pool_base::schedule: "
                "attempt to push a job to closed job queue");

        m_job_queue.push(std::move(job));
        m_job_queue.cv_consume.notify_one();

        return true;  // job scheduled
    }

    /** Schedule a job copy */
    inline bool schedule(
        const job_t & job,
        bool          force = false,
        double        tout  = -1.0)
    {
        return schedule(job_t(job), force, tout);
    }

    /**
     *  \brief  Shut the pool down
     *
     *  After this, any attempt to push another job to the queue
     *  will fail with an exception.
     */
    inline void shutdown() {
        std::unique_lock<std::mutex> lock(m_job_queue.mx);
        m_job_queue.closed = true;
        m_job_queue.cv_consume.notify_all();
    }

    /**
     *  \brief  Get thread pool info
     *
     *  NOTE: The info is collected under lock (in order to be consistent).
     *  Gathering the info too often may (and will) negatively influence
     *  the thread pool efficiency.
     */
    inline info get_info() const {
        std::unique_lock<std::mutex> tpool_lock(m_mx);
        std::unique_lock<std::mutex> queue_lock(m_job_queue.mx);

        return info(*this);
    }

};  // end of template class thread_pool_base

}  // end of anonymous namespace


/** Thread pool (FIFO) */
template <typename Job>
using thread_pool_fifo = thread_pool_base<std::queue<Job> >;

/** Thread pool (priority queue) */
template <typename Job>
using thread_pool_prio = thread_pool_base<std::priority_queue<Job> >;

}}  // end of namespace mt libprocxx

#endif  // end of #ifndef libprocxx__mt__thread_pool_hxx
