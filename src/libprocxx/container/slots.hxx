#ifndef libprocxx__container__slots_hxx
#define libprocxx__container__slots_hxx

/**
 *  \file
 *  \brief  General slots
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

#include <vector>
#include <queue>
#include <functional>
#include <cstddef>
#include <stdexcept>


namespace libprocxx {
namespace container {

/**
 *  \brief  Provider of a slot from vector of slots of constant size
 *
 *  The container provides 1st available slot from a vector of N slots.
 *  The time complexity of acquire/release operations is O(log N).
 *
 *  \tparam  Slot  Slot type
 */
template <typename Slot>
class slots {
    private:

    std::vector<Slot>   m_slots;     /**< Vector of slots */
    std::vector<size_t> m_slot_ixs;  /**< Slot indices    */

    /** Priority queue of available slot indices */
    std::priority_queue<size_t, decltype(m_slot_ixs), std::greater<size_t> >
        m_slot_ix_pqueue;

    /** Slot index getter */
    inline size_t slot_ix(const Slot & slot) const {
        return &slot - m_slots.data();
    }

    /** Slot index queue initialisation */
    void init_slot_ix_queue(size_t n) {
        m_slot_ixs.reserve(n);

        // All the slots are available
        for (size_t i = 0; i < n; ++i)
            m_slot_ix_pqueue.push(i);
    }

    public:

    /**
     *  \brief  Constructor
     *
     *  \param  n  Number of slots
     */
    slots(size_t n):
        m_slots(n),
        m_slot_ix_pqueue(std::greater<size_t>(), m_slot_ixs)
    {
        init_slot_ix_queue(n);
    }

    /**
     *  \brief  Constructor
     *
     *  \param  n     Number of slots
     *  \param  args  Slot constructor parameters
     */
    template <typename... Args>
    slots(size_t n, Args... args):
        m_slot_ix_pqueue(std::greater<size_t>(), m_slot_ixs)
    {
        m_slots.reserve(n);
        for (size_t i = 0; i < n; ++i)
            m_slots.emplace_back(args...);
        init_slot_ix_queue(n);
    }

    /** Total number of slots */
    inline size_t size() const { return m_slots.size(); }

    /** Number of available slots */
    inline size_t available() const { return m_slot_ix_pqueue.size(); }

    /** Acquire an unused slot */
    Slot & acquire() {
        if (m_slot_ix_pqueue.empty())
            throw std::runtime_error(
                "libprocxx::container::slots: "
                "slots exhausted");

        size_t ix = m_slot_ix_pqueue.top();
        m_slot_ix_pqueue.pop();
        return m_slots[ix];
    }

    inline void release(const Slot & slot) {
        m_slot_ix_pqueue.push(slot_ix(slot));
    }

};  // end of template class slots

}}  // end of namespaces container libprocxx

#endif  // end of #ifndef libprocxx__container__slots_hxx
