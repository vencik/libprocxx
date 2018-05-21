#ifndef libprocxx__mt__scheduler_hxx
#define libprocxx__mt__scheduler_hxx

/**
 *  \file
 *  \brief  Task Scheduler
 *
 *  \date   2017/09/06
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

#include <libprocxx/mt/thread_pool.hxx>
#include <libprocxx/mt/utils.hxx>
#include <libprocxx/container/queue.hxx>

#include <cassert>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <limits>


namespace libprocxx {
namespace mt {

/**
 *  \brief  Task Scheduler
 *
 *  Scheduler capable of timely (and possibly periodical) execution
 *  of tasks.
 *  Tasks are kept in a priority queue which keeps the tasks ordered
 *  in terms of execution time (lowest first).
 *  The tasks are executed in a single thread that is registered and obtained
 *  from a thread pool
 *  The task waits for the execution time point of the task
 *  on the queue front.
 *  Should the task be executed periodically, it is re-inserted to the queue
 *  after its execution automatically.
 *
 *  Note that, as said above, the scheduled tasks are serialised
 *  by the scheduler.
 *  You are however free to run multiple schedulers in parallel if your
 *  tasks take long to execute or you schedule many tasks.
 *
 *  \tparam  Task  Scheduled task
 *  \tparam  TP    Thread pool type
 */
template <typename Task, class TP>
class scheduler {
    public:

    using task_t  = Task;  /**< Scheduled task type */
    using tpool_t = TP;    /**< Thread pool type    */

    /**
     *  \brief  Scheduler runtime levels
     *
     *  The runtime levels have the following meaning:
     *  * \c STARTED                - The scheduler was started
     *                                (intermediate phase before scheduling
     *                                thread starts to process task queue;
     *                                this should normally be pretty quick,
     *                                except if the thread pool is exhausted
     *                                at the time of the scheduler creation)
     *  * \c RUNNING                - Scheduler is in operation
     *  * \c SHUTDOWN_PENDING       - Shut down when no more tasks
     *                                are scheduled (but keep on
     *                                rescheduling periodic tasks)
     *  * \c SHUTDOWN_NO_RESCHEDULE - Shut down when no more tasks
     *                                are scheduled (cease rescheduling)
     *  * \c SHUTDOWN_ASAP          - Shut down ASAP, discard queued tasks
     *  * \c TERMINATED             - The scheduler thread has terminated
     *                                (no further scheduling is possible)
     */
    enum runtime_level {
        STARTED = 0,
        RUNNING,
        SHUTDOWN_PENDING,
        SHUTDOWN_NO_RESCHEDULE,
        SHUTDOWN_ASAP,
        TERMINATED,

        SHUTDOWN_MODE_MIN = SHUTDOWN_PENDING,  /**< Min. shutdown mode    */
        SHUTDOWN_NORMALLY = SHUTDOWN_PENDING,  /**< Default shutdown mode */
        SHUTDOWN_MODE_MAX = SHUTDOWN_ASAP,     /**< Max. shutdown mode    */
    };  // end of enum runtime_level

    /** Time point type */
    using time_point_t = std::chrono::time_point<std::chrono::system_clock>;

    static const time_point_t asap;   /**< ASAP schedule timepoint */

    private:

    /** Time duration type [us] */
    using duration_t = std::chrono::duration<long, std::micro>;

    /** Task schedule */
    struct task_schedule {
        task_t       task;   /**< Task                                     */
        time_point_t at;     /**< Time point of execution                  */
        duration_t   every;  /**< Repetition period, 0 means no repetition */
        size_t       cnt;    /**< Number of execs (only for repetition)    */

        /** Constructor */
        task_schedule(
            task_t             && task_,
            const time_point_t &  at_,
            duration_t         && every_,
            size_t                cnt_)
        :
            task  ( std::move(task_)  ),
            at    ( at_               ),
            every ( std::move(every_) ),
            cnt   ( cnt_              )
        {}

        /** Copy Constructor */
/*
        task_schedule(const task_schedule & ts):
            task  ( ts.task  ),
            at    ( ts.at    ),
            every ( ts.every ),
            cnt   ( ts.cnt   )
        {}
*/

        /** Move constructor */
/*
        task_schedule(task_schedule && ts):
            task  ( std::move(ts.task)  ),
            at    ( std::move(ts.at)    ),
            every ( std::move(ts.every) ),
            cnt   ( ts.cnt              )
        {}
*/

        /** Task schedule comparison */
        inline bool operator < (const task_schedule & ts) const {
            return at > ts.at;
        }

    };  // end of struct task_schedule

    /** Task schedule queue type */
    using queue_t = container::priority_queue<task_schedule>;

    using routine_t = std::function<void()>;  /**< Scheduler thread routine */

    using routine_job_t = typename tpool_t::job_t;  /**< Thread pool job */

    tpool_t &               m_tpool;     /**< Thread pool used         */
    queue_t                 m_queue;     /**< Task schedule queue      */
    runtime_level           m_rt_level;  /**< Current runtime level    */
    mutable std::mutex      m_mx;        /**< Operation mutex          */
    std::condition_variable m_cv_state;  /**< Sched. state changed     */
    std::condition_variable m_cv_done;   /**< S. thread terminated     */
    routine_t               m_routine;   /**< Scheduler thread routine */

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  tpool  Thread pool used for scheduling thread acquisition
     */
    scheduler(tpool_t & tpool):
        m_tpool    ( tpool   ),
        m_rt_level ( STARTED ),
        m_routine  ( [this]() { this->routine(); } )
    {
        m_tpool.reserve(1);       // reserve scheduler thread
        m_tpool.push(m_routine);  // start scheduler thread
    }

    /**
     *  \brief  Schedule \c task
     *
     *  Note that as soon as the scheduler has entered any shutdown
     *  runtime level, calls to the function will result in
     *  throwing an exception.
     *
     *  \param  task      Scheduled task
     *  \param  at        Execution time point (0 means ASAP)
     *  \param  every_us  Execution periodicity in [us] (0 means none)
     *  \param  cnt       Number of executions (0 means until shutdown)
     *
     *  \return Current amount of scheduled tasks in the queue
     */
    size_t schedule(
        task_t &&            task,
        const time_point_t & at       = 0,
        long                 every_us = 0,
        size_t               cnt      = 0)
    {
        lock4scope(m_mx);

        if (RUNNING < m_rt_level)
            throw std::logic_error(
                "libprocxx::mt::scheduler::schedule: "
                "no more tasks may be scheduled");

        m_queue.emplace(std::move(task), at, duration_t(every_us), cnt);
        m_cv_state.notify_one();
        return m_queue.size();
    }

    /** Schedule task copy (see \ref schdule) */
    size_t schedule(
        const task_t       & task,
        const time_point_t & at       = 0,
        long                 every_us = 0,
        size_t               cnt      = 0)
    {
        return schedule(task_t(task), at, every_us, cnt);
    }

    /** Scheduler runtime level getter */
    inline runtime_level state() const {
        lock4scope(m_mx);
        return m_rt_level;
    }

    /**
     *  \brief  Shut the scheduler down
     *
     *  Sets the runtime level to requested shutdown mode and notifies
     *  the scheduler thread.
     *
     *  The possible \c shutdown parameter values are \c SHUTDOWN_*
     *  \ref runtime_level values.
     *
     *  As soon as shutdown runtime level is set, no more tasks may be
     *  queued.
     *
     *  \c SHUTDOWN_PENDING shutdown mode guarrantees that all already queued
     *  tasks will get executed when due (unless the shutdown mode is risen
     *  further).
     *
     *  \c SHUTDOWN_ASAP shutdown mode guarrantees that at the time of
     *  the function return, no more queued tasks will be executed
     *  (except the one that is executed at the call point, if any).
     *
     *  Note that this function may be called more than once;
     *  however, runtime level may only increase in time.
     *  If the current runtime level is greater or equal to
     *  the supplied \c shutdown parameter, the call has no effect.
     *  Also note that using other \c shutdown modes than above
     *  will result in throwing an exception.
     *
     *  \param  shutdown  Requested shutdown level
     */
    void shutdown(runtime_level shutdown = SHUTDOWN_NORMALLY) {
        if (!(SHUTDOWN_MODE_MIN <= shutdown && shutdown <= SHUTDOWN_MODE_MAX))
            throw std::logic_error(
                "libprocxx::mt::scheduler::shutdown: "
                "invalid shutdown mode");

        lock4scope(m_mx);

        if (shutdown <= m_rt_level) return;  // won't decrease runtime level

        m_rt_level = shutdown;
        m_cv_state.notify_one();
    }

    /** Destructor (waits for scheduler thread termination) */
    ~scheduler() {
        shutdown();  // signal shut down to scheduler thread

        // Wait till scheduler thread terminates
        {
            auto lock = get_lock4scope(m_mx);

            while (m_rt_level < TERMINATED)
                m_cv_done.wait(lock);
        }

        m_tpool.unreserve(1);  // unreserve the scheduler thread
    }

    private:

    /** Scheduler routine */
    void routine() {
        static const time_point_t never(time_point_t::max());
        static const duration_t   O(0);

        auto lock = get_lock4scope(m_mx);

        m_rt_level = RUNNING;  // we're scheduling

        for (;;) {
            time_point_t at(never);

            while (!m_queue.empty()) {
                if (SHUTDOWN_ASAP <= m_rt_level) goto SHUTDOWN;

                time_point_t now(std::chrono::system_clock::now());

                // Resolve next task execution time
                at = m_queue.front().at;
                if (asap == at) at = now;

                if (at > now) break;  // too soon

                // Remove task from queue
                task_schedule task_sh(m_queue.front());
                m_queue.pop();

                // Execute due task
                {
                    unlock4scope(lock);
                    task_sh.task();
                }

                // Reschedule periodically executed task
                if (SHUTDOWN_NO_RESCHEDULE > m_rt_level && task_sh.every > O) {
                    size_t cnt = 0;

                    if (!task_sh.cnt || (cnt = task_sh.cnt - 1))
                        m_queue.emplace(std::move(task_sh.task),
                            at + task_sh.every, std::move(task_sh.every), cnt);
                }
            }

            // Task queue is empty
            if (m_queue.empty()) {
                if (SHUTDOWN_PENDING <= m_rt_level) goto SHUTDOWN;
            }

            // Some tasks in queue
            else {
                if (SHUTDOWN_ASAP <= m_rt_level) goto SHUTDOWN;
            }

            m_cv_state.wait_until(lock, at);  // suspend till next task is due
        }

        SHUTDOWN:
        m_rt_level = TERMINATED;
        m_cv_done.notify_one();  // notify destructor that we're done
    }

};  // end of template class scheduler

// Static members initialisations
template <typename Task, class TP>
const typename scheduler<Task, TP>::time_point_t
    scheduler<Task, TP>::asap(scheduler<Task, TP>::time_point_t::min());

}}  // end of namespace mt libprocxx

#endif  // end of #ifndef libprocxx__mt__scheduler_hxx
