#ifndef libprocxx__container__sliding_window_hxx
#define libprocxx__container__sliding_window_hxx

/**
 *  \file
 *  \brief  Sliding window of values
 *
 *  \date   2017/09/04
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

#include <vector>
#include <cstddef>
#include <stdexcept>


namespace libprocxx {
namespace container {

/**
 *  \brief  Sliding window of values
 *
 *  Window has fixed size.
 *  Provides access to the least recent value.
 *  When new value is assigned to it, the most recent one is lost.
 *
 *  Time complexity of all operations is O(1).
 *
 *  \tparam  T  Value type
 */
template <typename T>
class sliding_window {
    public:

    using value_t = T;  /**< Value type */

    private:

    std::vector<value_t> m_values;  /**< Vector of values               */
    size_t               m_ix;      /**< Least recently set value index */

    public:

    /**
     *  \brief  Constructor
     *
     *  Uses default value for items initialisation.
     *
     *  \param  n  Window size (at least 1)
     */
    sliding_window(size_t n):
        m_values(n),
        m_ix(0)
    {
        if (!n)
            throw std::range_error(
                "libprocxx::container::sliding_window: "
                "window size must be positive");
    }

    /**
     *  \brief  Constructor
     *
     *  \param  n  Window size (at least 1)
     *  \param  i  Items initialiser
     */
    sliding_window(size_t n, const value_t & i):
        m_values(n, i),
        m_ix(0)
    {
        if (!n)
            throw std::range_error(
                "libprocxx::container::sliding_window: "
                "window size must be positive");
    }

    /** Window size */
    inline size_t size() const { return m_values.size(); }

    /** Get least recent value */
    inline operator value_t() const { return m_values[m_ix]; }

    /**
     *  \brief  Set most recent value and shift window
     *
     *  \param  val  Most recent value
     *
     *  \return Least recent value
     */
    inline value_t operator << (const value_t & val) {
        value_t lr_val = m_values[m_ix];
        m_values[m_ix] = val;
        if (!(++m_ix < m_values.size())) m_ix = 0;
        return lr_val;
    }

};  // end of template class sliding_window


/**
 *  \brief  Sliding window values aggregation
 *
 *  Keeps a sliding window of values and their aggregation produced
 *  by the \c AggregFn function.
 *  As a value leaves the sliding window, it is disaggregated using
 *  the \c DisaggregFn function.
 *
 *  \tparam  T            Value type
 *  \tparam  AggregFn     Aggregation function (functor)
 *  \tparam  DisaggregFn  Disaggregation function (functor)
 */
template <typename T, class AggregFn, class DisaggregFn>
class sliding_window_aggregation {
    public:

    using value_t        = T;            /**< Value type                */
    using aggreg_fn_t    = AggregFn;     /**< Aggregation function type */
    using disaggreg_fn_t = DisaggregFn;  /**< Disaggregation fn. type   */

    private:

    using swin_t = sliding_window<value_t>;  /**< Sliding window type */

    aggreg_fn_t    m_aggreg_fn;     /**< Aggregation function    */
    disaggreg_fn_t m_disaggreg_fn;  /**< Disaggregation function */
    swin_t         m_swin;          /**< Sliding window          */
    value_t        m_aggreg;        /**< Aggregation             */

    public:

    /**
     *  \brief  Constructor
     *
     *  Uses default value for values initialisation.
     *  See \ref sliding_window::sliding_window constructors.
     *
     *  \param  n     Window size
     *  \param  args  (Dis)aggregation functions constructor arguments
     */
    template <typename... Args>
    sliding_window_aggregation(size_t n, Args... args):
        m_aggreg_fn(args...),
        m_disaggreg_fn(args...),
        m_swin(n)
    {}

    /**
     *  \brief  Constructor
     *
     *  See \ref sliding_window::sliding_window constructors.
     *
     *  \param  n     Window size
     *  \param  i     Value initialiser
     *  \param  args  (Dis)aggregation functions constructor arguments
     */
    template <typename... Args>
    sliding_window_aggregation(size_t n, const value_t & i, Args... args):
        m_aggreg_fn(args...),
        m_disaggreg_fn(args...),
        m_swin(n, i),
        m_aggreg(i)
    {}

    /** Sliding window size getter */
    inline size_t size() const { return m_swin.size(); }

    /** Aggregated value getter */
    inline operator value_t() const { return m_aggreg; }

    /**
     *  \brief  Aggregate another value
     *
     *  \param  val  Value
     *
     *  \return Current value aggregation
     */
    inline value_t operator << (const value_t & val) {
        return m_aggreg = m_aggreg_fn(
            m_disaggreg_fn(m_aggreg, m_swin << val),
            val);
    }

};  // end of template class sliding_window_aggregation

}}  // end of namespaces container libprocxx

#endif  // end of #ifndef libprocxx__container__sliding_window_hxx
