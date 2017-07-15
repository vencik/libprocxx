/**
 *  \file
 *  \brief  General slots unit test
 *
 *  \date   2017/07/13
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

#include <libprocxx/container/slots.hxx>

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>


class test_slot {
    private:

    static size_t s_cnt;  /**< Slot counter */

    public:

    const size_t no;  /**< Slot number */

    /** Constructor */
    test_slot(): no(s_cnt++) {}

};  // end of class test_slot

size_t test_slot::s_cnt = 0;

using slots_t = libprocxx::container::slots<test_slot>;


static int main_impl(int argc, char * const argv[]) {
    unsigned rng_seed = std::time(nullptr);
    std::srand(rng_seed);
    std::cerr << "RNG seeded by " << rng_seed << std::endl;

    size_t slot_cnt = 10;

    slots_t slots(slot_cnt);

    std::vector<test_slot *> used_slots;
    used_slots.reserve(slots.size());

    while (slots.available()) {
        used_slots.push_back(&slots.acquire());

        std::cerr
            << "Acquired slot #"
            << used_slots.back()->no
            << std::endl;
    }

    std::random_shuffle(used_slots.begin(), used_slots.end());

    size_t min_no = slot_cnt;
    for (auto slot: used_slots) {
        if (min_no > slot->no) min_no = slot->no;

        std::cerr
            << "Releasing slot #"
            << slot->no
            << std::endl;

        slots.release(*slot);

        auto & first_avail_slot = slots.acquire();
        std::cerr
            << "1st available slot now: "
            << first_avail_slot.no
            << " (expected: " << min_no << ')'
            << std::endl;

        if (min_no != first_avail_slot.no)
            throw std::logic_error(
                "libprocxx::container::slots UT failed: "
                "didn't get 1st available slot");

        slots.release(first_avail_slot);
    }

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
