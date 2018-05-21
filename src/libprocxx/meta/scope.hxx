#ifndef libprocxx__meta__scope_hxx
#define libprocxx__meta__scope_hxx

/**
 *  \file
 *  \brief  Pairwise actions planning (typicaly for scope)
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

#include <libprocxx/meta/macro.hxx>


/** Convenience macro for scope pairwise calls */
#define libprocxx__meta__deferred(fn) \
    auto CONCAT(__libprocxx__meta__deferred_, __LINE__) = \
        libprocxx::meta::deferred(fn)

/** Convenience macro for scope pairwise calls */
#define libprocxx__meta__pairwise(fnc, fnd) \
    auto CONCAT(__libprocxx__meta__pairwise_, __LINE__) = \
        libprocxx::meta::pairwise((fnc), (fnd))


/**
 *  \brief  Call function at the point of leaving current scope
 *
 *  Convenience macro for deferring call to the point of scope leaving.
 *  The \c fn function is called using \ref deferred_action wrapper.
 */
#define when_leaving_scope(fn) libprocxx__meta__deferred(fn)

/**
 *  \brief  Call \c fnc now and \c fnd when leaving current scope
 *
 *  Convenience macro for scope pairwise calls (short identifier).
 *  The \c fnc function is called immediately.
 *  The \c fnd function is called using \ref deffered_action wrapper.
 */
#define pair4scope(fnc, fnd) libprocxx__meta__pairwise((fnc), (fnd))


namespace libprocxx {
namespace meta {

/**
 *  \brief  Deferred action
 *
 *  The object's destructor executes the function supplied.
 *  Thus, the action is deferred to the point of leaving the scope.
 *
 *  NOTE: Everything that applies to destructor (exception-wise) naturally
 *  applies to the function, too.
 *
 *  \tparam  Fn  Function called upon destruction
 */
template <class Fn>
class deferred_action {
    private:

    Fn m_at_destroy;  /**< Action done at the point of destruction */

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  fn  Function called upon destruction
     */
    deferred_action(Fn fn): m_at_destroy(fn) {}

    /** Destructor (calls the second function) */
    ~deferred_action() {
        m_at_destroy();
    }

};  // end of template class deferred_action

/** Deferred call (done at the point of leaving the scope) */
template <class Fn>
inline deferred_action<Fn> deferred(Fn fn) {
    return deferred_action<Fn>(fn);
}


/**
 *  \brief  Pair-wise call
 *
 *  \c FnNow is done immediatelly, \c FnD at the point of leaving the scope.
 */
template <class FnNow, class FnD>
inline deferred_action<FnD> pairwise(FnNow fnn, FnD fnd) {
    fnn();
    return deferred_action<FnD>(fnd);
}

}}  // end of namespaces meta libprocxx

#endif  // end of #ifndef libprocxx__meta__scope_hxx
