#ifndef libprocxx__mt__utils_hxx
#define libprocxx__mt__utils_hxx

/**
 *  \file
 *  \brief  Various utilities for multithreading
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
#include <libprocxx/meta/macro.hxx>


/**
 *  \brief  Convenience macro for scope lock guard
 *
 *  Instantiates a local variable of \c std::lock_guard<decltype(lockable)>
 *  type passing the \c lockable object to its constructor.
 *  As long as the variable exists, the \c lockable object is in locked state.
 *
 *  \param  lockable  Lockable object (e.g. \c std::mutex )
 */
#define lock4scope(lockable) \
    std::lock_guard<decltype(lockable)> \
        CONCAT(__libprocxx__mt__utils__lock4scope, __LINE__)(lockable)


/**
 *  \brief  Convenience macro for scope shared lock guard
 *
 *  Instantiates a local variable of \c std::shared_lock<decltype(lockable)>
 *  type passing the \c lockable object to its constructor.
 *  The \c lockable object must implement shared locking.
 *  As long as the variable exists, the \c lockable object is in shared-locked
 *  state.
 *
 *  \param  lockable  Lockable object (e.g. \c std::shared_mutex )
 */
#define shared_lock4scope(lockable) \
    std::shared_lock<decltype(lockable)> \
        CONCAT(__libprocxx__mt__utils__lock4scope, __LINE__)(lockable)


/**
 *  \brief  Convenience macro for scope unique lock
 *
 *  Instantiates and returns \c std::unique_lock<decltype(lockable)> object,
 *  passing the \c lockable object to its constructor.
 *  As long as the variable exists, the \c lockable object is in locked state.
 *
 *  Note that you MUST assign the returned object to a local variable
 *  in order to keep \c lockable object in locked state, e.g:
 *      auto lock = get_lock4scope(my_mutex);
 *
 *  \param  lockable  Lockable object (e.g. \c std::mutex )
 *
 *  \return \c std::unique_lock<decltype(lockable)>(lockable)
 */
#define get_lock4scope(lockable) \
    std::unique_lock<decltype(lockable)>(lockable)


/**
 *  \brief  Convenience macro for scope shared lock
 *
 *  Instantiates and returns \c std::shared_lock<decltype(lockable)> object,
 *  passing the \c lockable object to its constructor.
 *  The \c lockable object must implement shared locking.
 *  As long as the variable exists, the \c lockable object is in shared-locked
 *  state.
 *
 *  Note that you MUST assign the returned object to a local variable
 *  in order to keep \c lockable object in locked state, e.g:
 *      auto lock = get_shared_lock4scope(my_mutex);
 *
 *  \param  lockable  Lockable object (e.g. \c std::shared_mutex )
 *
 *  \return \c std::shared_lock<decltype(lockable)>(lockable)
 */
#define get_shared_lock4scope(lockable) \
    std::shared_lock<decltype(lockable)>(lockable)


/**
 *  \brief  Convenience macro for scope unlock
 *
 *  Unlocks the \c lockable object (typically obtained from \ref get_lock4scope)
 *  for the rest of the scope.
 *  When the scope is abandoned (by \c break , \c continue , \c return or
 *  throwing an exception etc) the object gets locked again.
 *
 *  The \c lockable object must implement \c lock() and \c unlock() methods.
 *
 *  \param  lockable  Lockable object (e.g. \c std::unique_lock )
 */
#define unlock4scope(lockable) \
    libprocxx__meta__pairwise( \
        [&lockable]() { (lockable).unlock(); }, \
        [&lockable]() { (lockable).lock(); })

#endif  // end of #ifndef libprocxx__mt__utils_hxx
