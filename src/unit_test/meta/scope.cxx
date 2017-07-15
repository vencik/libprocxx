/**
 *  \file
 *  \brief  Scope-related metaprogramming unit test
 *
 *  \date   2017/07/15
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

#include <iostream>
#include <list>
#include <algorithm>


static int main_impl(int argc, char * const argv[]) {
    std::list<int> actions_order;

    {
        pair4scope([&actions_order]() {
            std::cout << "1st action" << std::endl;
            actions_order.push_back(0);
        }, [&actions_order]() {
            std::cout << "Last action (deferred)" << std::endl;
            actions_order.push_back(999);
        });

        do {
            std::cout << "1st inner scope action" << std::endl;
            actions_order.push_back(10);

            when_leaving_scope([&actions_order]() {
                std::cout << "Last inner scope action (deferred)" << std::endl;
                actions_order.push_back(99);
            });

            std::cout << "Middle inner scope action" << std::endl;
            actions_order.push_back(50);

            break;  // now do the deferred actions

            throw std::logic_error("Unreachable code reached");

        } while (0);

        std::cout << "Last action in conventional order" << std::endl;
        actions_order.push_back(199);
    }

    // Check actions order
    int prev_action = -1;
    std::for_each(actions_order.begin(), actions_order.end(),
    [&prev_action](int action) {
        if (!(prev_action < action))
            throw std::logic_error("Actions order is wrong");

        prev_action = action;
    });

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
