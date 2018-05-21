#ifndef libprocxx__stats__utils_hxx
#define libprocxx__stats__utils_hxx

/**
 *  \file
 *  \brief  Various statistical utilities
 *
 *  \date   2017/09/05
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

#include <libprocxx/container/sliding_window.hxx>

#include <functional>


namespace libprocxx {
namespace stats {

/**
 *  \brief  (Simple) moving average
 *
 *  See https://en.wikipedia.org/wiki/Moving_average
 *
 *  \tparam  T  Base numeric type
 */
template <typename T>
class moving_average {
    public:

    using base_t = T;  /**< Base numeric type */

    private:

    /** Sliding summation type */
    using summation_t = container::sliding_window_aggregation<
        base_t, std::plus<base_t>, std::minus<base_t> >;

    summation_t m_sum;  /**< Sliding summation */

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  n  Window size
     */
    moving_average(size_t n): m_sum(n, (base_t)0) {}

    /** Current average value getter */
    inline operator base_t() const { return m_sum / m_sum.size(); }

    /**
     *  \brief  Update the average by another value
     *
     *  \param  val  Value
     *
     *  \return Current average value
     */
    inline base_t operator << (const base_t & val) {
        return (m_sum << val) / m_sum.size();
    }

};  // end of template class moving_average

}}  // end of namespace stats libprocxx

#endif  // end of #ifndef libprocxx__stats__utils_hxx
