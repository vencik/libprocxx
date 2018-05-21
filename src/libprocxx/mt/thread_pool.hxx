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

#include <libprocxx/stereotype/prod_cons.hxx>
#include <libprocxx/container/queue.hxx>

#include <cassert>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <utility>


namespace libprocxx {
namespace mt {

namespace {

/**
 *  \brief  Thread pool (base ancestor)
 *
 *  Tread pool is implemented as a consumer of queued jobs.
 *  Job consuming means to execute it.
 *  See \ref stereotype::consumer for exhaustive documentation.
 *
 *  \tparam  Q  Job queue
 */
template <class Q>
class thread_pool_base: public stereotype::consumer<Q> {
    public:

    using super_t   = stereotype::consumer<Q>;      /**< Ancestor type      */
    using routine_t = typename super_t::routine_t;  /**< Cons. routine type */
    using job_t     = typename super_t::item_t;     /**< Executed job type  */

    /**
     *  \brief  Constructor (see \ref stereotype::consumer::consumer)
     */
    thread_pool_base(
        size_t high_wm    = SIZE_MAX,
        size_t max        = SIZE_MAX,
        size_t reserved   = 1,
        size_t keepalive4 = 2,
        double idle_tout  = 10000)
    :
        super_t(
        // Thread starting
        [](auto routine) -> bool {
            try {
                std::thread(routine).detach();
                return true;
            }

            catch (std::system_error &) {}  // can't start another thread

            return false;
        },

        // Job consuming is execution
        [](job_t && job) { job(); },

        // Initialisers
        high_wm, max, reserved, keepalive4, idle_tout)
    {}

};  // end of template class thread_pool_base

}  // end of anonymous namespace


/** Thread pool (FIFO) */
template <typename Job>
using thread_pool_fifo = thread_pool_base<container::queue<Job> >;

/** Thread pool (priority queue) */
template <typename Job>
using thread_pool_prio = thread_pool_base<container::priority_queue<Job> >;

}}  // end of namespace mt libprocxx

#endif  // end of #ifndef libprocxx__mt__thread_pool_hxx
