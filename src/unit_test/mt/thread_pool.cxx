/**
 *  \file
 *  \brief  Thread pool unit test
 *
 *  \date   2017/07/28
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
#include <libprocxx/mt/scheduler.hxx>
#include <libprocxx/stats/controller.hxx>
#include <libprocxx/stats/utils.hxx>

#include <iostream>
#include <functional>
#include <chrono>


using my_tpool_copy_t = libprocxx::mt::thread_pool_fifo<
    std::function<void()> >;

using my_tpool_ref_t = libprocxx::mt::thread_pool_fifo<
    std::reference_wrapper<std::function<void()> > >;


/** Sleep for \c x milliseconds */
static void sleep_ms(double x) {
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(x));
}


/** Thread pool info serialisation */
template <typename TP>
class test {
    private:

    /** Report thread pool info */
    static void report(
        std::ostream            & out,
        const typename TP::info & info)
    {
        double now =
            (double)(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count())
            / 1000000.0;

        out
            << "{\"timestamp\":"   << std::showpoint << std::fixed << now
            << ",\"tmax\":"        << info.attrs.tmax
            << ",\"tcnt\":"        << info.attrs.tcnt
            << ",\"treserved\":"   << info.attrs.treserved
            << ",\"keepalive4\":"  << info.attrs.keepalive4
            << ",\"idle_tout\":"   << std::showpoint << std::fixed << info.attrs.idle_tout.count()
            << ",\"terminated\":"  << std::boolalpha << info.attrs.terminated
            << ",\"queue\":"
                   "{\"high_wm\":" << info.queue.high_wm
                << ",\"size\":"    << info.queue.size
                << ",\"closed\":"  << info.queue.closed
                << "}"
            "}"
            << std::endl;
    }

    public:

    /**
     *  \brief  Thread pool UT
     *
     *  \param  n     Number of test loops
     *  \param  args  Arguments passed to thread pool constructor
     */
    template <typename... Args>
    test(size_t n, Args... args) {
        const double msc =  50;
        const double ms1 = 190;
        const double ms2 = 100;
        const double ms3 =  20;

        std::function<void()> controller, job1, job2, job3;

        libprocxx::stats::pid_controller<double> pid_ctrl(
            20, 0.0, 2, 0, 3, msc / 1000.0);
        libprocxx::stats::moving_average<double> correction_avg(10);

        // Create thread pool
        TP tpool(args...);

        // Create scheduler
        using scheduler_task_t = typename TP::job_t;
        using scheduler_t      = libprocxx::mt::scheduler<scheduler_task_t, TP>;
        scheduler_t scheduler(tpool);

        // Create thread pool controller
        controller = [&tpool, &scheduler, &pid_ctrl, &correction_avg]() {
            auto info = tpool.get_info();

            // Calculate thread count correction using PID controller
            double fill_rate = (double)info.queue.size / info.queue.high_wm;
            double corr      = pid_ctrl(fill_rate);
            double corr_avg  = correction_avg << corr;

std::cerr
    << "Fill rate: " << fill_rate << std::endl
    << "Correction: " << corr << std::endl
    << "Correction avg: " << corr_avg << std::endl;

            size_t start = corr_avg <= -0.5 ? (size_t)-corr_avg : 0;

            //report(std::cout, info, fill_rate, corr, corr_avg, start);
            report(std::cout, info);

            // The pool has shut down and scheduler uses the last thread
            if (info.queue.closed && info.attrs.tcnt == 1) {
                scheduler.shutdown(scheduler_t::runtime_level::SHUTDOWN_ASAP);
                return;
            }

            // Start more threads
            if (start) tpool.start(start);
        };

        // Schedule thread pool controller
        scheduler.schedule(controller, scheduler_t::asap, msc * 1000);

        // Start some jobs
        job1 = [ms1]() { sleep_ms(ms1); };
        job2 = [ms2]() { sleep_ms(ms2); };
        job3 = [ms3]() { sleep_ms(ms3); };

        for (size_t i = 0; i < n; ++i) {
            tpool.push(job1);
            sleep_ms(3);

            tpool.push(job2);
            sleep_ms(5);

            tpool.push(job3);
            sleep_ms(10);
        }

        tpool.shutdown();
    }

};  // end of template class test


static int main_impl(int argc, char * const argv[]) {
    //unsigned rng_seed = std::time(nullptr);
    //std::srand(rng_seed);
    //std::cerr << "RNG seeded by " << rng_seed << std::endl;

    size_t loops      = 100;  // test loops
    size_t high_wm    =  50;  // job queue high watermark
    size_t max        =  30;  // thread limit
    size_t reserved   =   5;  // number of reserved threads
    size_t keepalive4 =   2;  // keep thread alive if 2+ jobs were popped

    int i = 0;
    if (++i < argc) loops       = (size_t)std::atol(argv[i]);
    if (++i < argc) high_wm     = (size_t)std::atol(argv[i]);
    if (++i < argc) max         = (size_t)std::atol(argv[i]);
    if (++i < argc) reserved    = (size_t)std::atol(argv[i]);
    if (++i < argc) keepalive4  = (size_t)std::atol(argv[i]);

    test<my_tpool_copy_t>(loops, high_wm, max, reserved, keepalive4);
    test<my_tpool_ref_t> (loops, high_wm, max, reserved, keepalive4);

    return 0;
}

int main(int argc, char * const argv[]) {
    int exit_code = 128;

    try {
        exit_code = main_impl(argc, argv);
    }
    catch (const std::exception & x) {
        std::cerr
            << "Standard exception caught: "
            << x.what()
            << std::endl;
    }
    catch (...) {
        std::cerr
            << "Unhandled non-standard exception caught"
            << std::endl;
    }

    std::cerr << "Exit code: " << exit_code << std::endl;
    return exit_code;
}
