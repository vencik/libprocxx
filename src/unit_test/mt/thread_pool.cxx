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

#include <iostream>
#include <functional>
#include <chrono>
#include <mutex>
#include <sstream>


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
    static std::ostream & report(
        std::ostream & out,
        const TP     & tpool)
    {
        static std::mutex out_mx;

        const auto info(tpool.get_info());

        double now =
            (double)(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count())
            / 1000000.0;

        std::stringstream info_ss; info_ss
            << "{\"timestamp\":"  << std::showpoint << std::fixed << now
            << ",\"tmax\":"       << info.attrs.tmax
            << ",\"tcnt\":"       << info.attrs.tcnt
            << ",\"treserved\":"  << info.attrs.treserved
            << ",\"idle_tout\":"  << std::showpoint << std::fixed << info.attrs.idle_tout.count()
            << ",\"terminated\":" << std::boolalpha << info.attrs.terminated
            << ",\"job_queue\":"
                   "{\"size\":"         << info.job_queue.size
                << ",\"closed\":"       << info.job_queue.closed
                << ",\"statistics\":"
                       "{\"size_high_wm\":"  << info.job_queue.stats.size_high_wm
                    << ",\"sampling_freq\":" << info.job_queue.stats.sampling_freq
                    << ",\"sampling_cnt\":"  << info.job_queue.stats.sampling_cnt
                    << ",\"size\":"          << info.job_queue.stats.size
                    << ",\"diff\":"          << info.job_queue.stats.diff
                    << "}"
                "}"
            "}";

        // Lock the output so as the JSON objects stay consistent
        std::unique_lock<std::mutex> lock(out_mx);

        // TODO: Buffered shoveling of data from info_ss to out
        out << info_ss.str() << std::endl;

        return out;
    }

    public:

    /**
     *  \brief  Thread pool UT
     *
     *  The arguments are passed to thread pool constructor
     */
    template <typename... Args>
    test(Args... args) {
        const size_t n   = 100;
        const double ms1 = 100;
        const double ms2 =  80;
        const double ms3 =  90;

        std::function<void()> job1, job2, job3;

        TP tpool(args...);

        job1 = [ms1, &tpool]() {
            report(std::cout, tpool);

            sleep_ms(ms1);

            report(std::cout, tpool);
        };

        job2 = [ms2, &tpool]() {
            report(std::cout, tpool);

            sleep_ms(ms2);

            report(std::cout, tpool);
        };

        job3 = [ms3, &tpool]() {
            report(std::cout, tpool);

            sleep_ms(ms3);

            report(std::cout, tpool);
        };

        for (size_t i = 0; i < n; ++i) {
            report(std::cout, tpool);

            tpool.schedule(job1);
            tpool.schedule(job2);
            tpool.schedule(job3);

            sleep_ms(7);

            report(std::cout, tpool);
        }
    }

};  // end of template class test


static int main_impl(int argc, char * const argv[]) {
    //unsigned rng_seed = std::time(nullptr);
    //std::srand(rng_seed);
    //std::cerr << "RNG seeded by " << rng_seed << std::endl;

    size_t high_wm   = 20;  // job queue high watermark
    size_t smpl_freq =  6;  // job queue statistics sampling frequency
    size_t max       = 30;  // thread limit
    size_t reserved  =  5;  // number of reserved threads

    if (argc > 1) high_wm   = (size_t)std::atol(argv[1]);
    if (argc > 2) smpl_freq = (size_t)std::atol(argv[2]);
    if (argc > 3) max       = (size_t)std::atol(argv[3]);
    if (argc > 4) reserved  = (size_t)std::atol(argv[4]);

    test<my_tpool_copy_t>(high_wm, smpl_freq, max, reserved);
    test<my_tpool_ref_t> (high_wm, smpl_freq, max, reserved);

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
