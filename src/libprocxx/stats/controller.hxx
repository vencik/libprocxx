#ifndef libprocxx__stats__controller_hxx
#define libprocxx__stats__controller_hxx

/**
 *  \file
 *  \brief  PID (PSD) Controller
 *
 *  See https://en.wikipedia.org/wiki/PID_controller
 *
 *  \date   2017/08/29
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
#include <stdexcept>


namespace libprocxx {
namespace stats {

/**
 *  \brief  PID (PSD) Controller
 *
 *  See https://en.wikipedia.org/wiki/PID_controller
 *
 *  This implementation uses floating window for the integral
 *  (i.e. it only keeps more or less short history of the error integral part
 *  of computed correction).
 *  Note that (in theory), the controller's output should be less erratic
 *  with increasing error window size; however, it may adapt less readily
 *  to change of the setpoint and/or the controlled system behaviour change
 *  if it is too wide.
 *
 *  \tparam  T  Base numeric type
 */
template <typename T>
class pid_controller {
    public:

    using base_t = T;  /**< Base numeric type */

    base_t setpoint;  /**< Setpoint                       */
    base_t kp;        /**< Proportional coefficient       */
    base_t ki;        /**< Integral coefficient           */
    base_t kd;        /**< Derivative coefficient         */
    base_t dt;        /**< Time step between measurements */

    private:

    /** Sliding window summation error integral parts */
    using integral_t = container::sliding_window_aggregation<
        base_t, std::plus<base_t>, std::minus<base_t> >;

    base_t     m_error;     /**< Last reported error */
    integral_t m_integral;  /**< Integral            */

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  wsize      Integral window size (must be positive)
     *  \param  setpoint_  Initial setpoint
     *  \param  kp_        Initial proportional coefficient
     *  \param  ki_        Initial integral coefficient
     *  \param  kd_        Initial derivative coefficient
     *  \param  dt_        Initial time step
     */
    pid_controller(
        size_t wsize,
        base_t setpoint_,
        base_t kp_,
        base_t ki_,
        base_t kd_,
        base_t dt_ = 1)
    :
        setpoint(setpoint_), kp(kp_), ki(ki_), kd(kd_), dt(dt_),
        m_error(0),
        m_integral(wsize, (base_t)0)
    {}

    /**
     *  \brief  Controller action
     *
     *  \param  pv  Process variable
     *
     *  \return Correction
     */
    base_t operator () (base_t pv) {
        const base_t error       = setpoint - pv;
        const base_t integral    = m_integral << error * dt;
        const base_t delta_error = (error - m_error) / dt;

        m_error = error;

        return kp * error + ki * integral + kd * delta_error;
    }

};  // end of template class pid_controller

}}  // end of namespace stats libprocxx

#endif  // end of #ifndef libprocxx__stats__controller_hxx
