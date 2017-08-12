#ifndef libprocxx__meta__function_hxx
#define libprocxx__meta__function_hxx

/**
 *  \file
 *  \brief  Function (functor) manipulation
 *
 *  \date   2017/07/20
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

#include <functional>
#include <tuple>


namespace libprocxx {
namespace meta {

namespace function {

/** \cond */
template <typename T>
struct traits;
/** \endcond */

/**
 *  \brief  Function (functor) type traits
 *
 *  The \c struct collects return type and argument types of a function
 *  or other callable object.
 *
 *  \tparam  R     Return type
 *  \tparam  Args  Positional arguments' types
 */
template<typename R, typename... Args>
struct traits<R(Args...)> {
    /** Number of positional arguments */
    static constexpr unsigned arg_cnt = sizeof...(Args);

    using type        = R(Args...);           /**< Function type    */
    using result_type = R;                    /**< Return type      */
    using arg_types   = std::tuple<Args...>;  /**< Arguments' types */

    /** Argument type getter */
    template <size_t i>
    struct arg {
        /** Type of i-th positinal argument (0-based index) */
        using type = typename std::tuple_element<i, arg_types>::type;
    };

    /** Type of \c std::function wrapper */
    using std_function_type = std::function<type>;

};  // end of template struct traits


/**
 *  \brief  Function (functor) run-time wrapper
 *
 *  Provides unified wrapper around function, functor and/or
 *  \c std::function calls.
 *
 *  \tparam  Fn  Functional type
 */
template <typename Fn>
struct runtime_wrapper {
    using function_type = Fn;  /**< Function type */

    function_type fn;  /**< Wrapped object */

    /** Function call */
    template <typename... Args>
    struct call {
        /** Function call type traits */
        using traits = traits<
            typename std::result_of<function_type(Args...)>::type,
            Args...>;
    };

    /** Default function constructor */
    runtime_wrapper() {}

    /** Constructor with arguments */
    template <typename... Args>
    runtime_wrapper(Args... args): fn(args...) {}

    /** Copying is forbidden */
    runtime_wrapper(const runtime_wrapper & ) = delete;

    /** Move constructor (moves the wrapped object) */
    runtime_wrapper(runtime_wrapper && rval): fn(std::move(rval.fn)) {};

    /** Constructor from rvalue function object (moved in) */
    runtime_wrapper(function_type && fn_): fn(std::move(fn_)) {}

    /** Wrapped functional object call */
    template <typename... Args>
    inline typename call<Args...>::traits::result_type
    operator () (Args... args) { return fn(args...); }

};  // end of template struct runtime_wrapper


/** Standard function run-time wrapper */
template <typename Fn>
struct runtime_wrapper<std::function<Fn> > {
    using function_type = std::function<Fn>;  /**< Function type */

    std::function<Fn> fn;  /**< Wrapped \c std::function */

    /** \c std::function call */
    template <typename... Args>
    struct call {
        /** \c std::function call type traits */
        using traits = traits<
            typename function_type::result_type, Args...>;
    };

    /** Constructor */
    runtime_wrapper(std::function<Fn> fn_): fn(fn_) {}

    /** Wrapped \c std::function call */
    template <typename... Args>
    inline typename call<Args...>::traits::result_type
    operator () (Args... args) { return fn(args...); }

};  // end of template struct runtime_wrapper


/**
 *  \brief  Function chain
 *
 *  Implemens concatenation of function calls, i.e.
 *  (f * g * ... * q)(x) = q( ... g( f(x) ) ... )
 *
 *  Its `operator ()` passes input to the 1st function supplied,
 *  its result is passed to te second and so on.
 *  The result of the last function becomes the return value of
 *  the chain's `operator ()`.
 *
 *  \tparam  Fn   The 1st function
 *  \tparam  Fns  The next functions
 */
template <typename Fn, typename... Fns>
struct chain {
    using fn_wrapper = runtime_wrapper<Fn>;
    using fns_chain  = chain<Fns...>;

    fn_wrapper fn;   /**< 1st function             */
    fns_chain  fns;  /**< The rest of the fuctions */

    /** Function chain call */
    template <typename... Args>
    struct call {
        using fn_result_type =
            typename fn_wrapper::template call<Args...>::result_type;
        using traits = traits<
            typename fns_chain::template call<fn_result_type>::result_type,
            Args...>;
    };

    /** Default constructor */
    chain() {}

    /** Copying is forbidden */
    chain(const chain & ) = delete;

    /** Move constructor */
    chain(chain && rval):
        fn(std::move(rval.fn)),
        fns(std::move(rval.fns))
    {}

    /** Constructor from rvalue function objects */
    chain(Fn && fn_, Fns&&... fns_):
        fn(std::move(fn_)),
        fns(std::forward<Fns>(fns_)...)
    {}

    /** Function chain call */
    template <typename... Args>
    typename call<Args...>::result_type operator () (Args... args) {
        return fns(fn(args...));
    }
};

/** Function chain (recursion fixed point) */
template <typename Fn>
struct chain<Fn> {
    using fn_wrapper = runtime_wrapper<Fn>;

    fn_wrapper<Fn> fn;  /**< Final function */

    template <typename... Args>
    struct call {
        using result_type =
            typename wrapper<Fn>::template call<Args...>::result_type;
    };

    chain() {}

    chain(const chain & ) = delete;

    chain(chain && rval): fn(std::move(rval.fn)) {}

    chain(Fn && fn_): fn(std::move(fn_)) {}

    template <typename... Args>
    typename call<Args...>::result_type operator() (Args... args) {
        return fn(args...);
    }
};

#if (0)
template <typename Fn, typename... Fns>
struct chain<std::function<Fn>, Fns...> {
    using fn_type    = std::function<Fn>;
    using fn_wrapper = wrapper<fn_type>;
    using fns_chain  = chain<Fns...>;

    fn_wrapper fn;
    fns_chain  fns;

    template <typename... Args>
    struct call {
        using fn_result_type = typename fn_type::result_type;
        using result_type =
            typename fns_chain::template call<fn_result_type>::result_type;
    };

    template <typename... Args>
    typename call<Args...>::result_type operator () (Args... args) {
        return fns(fn(args...));
    }
};

template <typename Fn>
struct chain<std::function<Fn> > {
    using fn_type    = std::function<Fn>;
    using fn_wrapper = wrapper<fn_type>;

    fn_wrapper fn;

    template <typename... Args>
    struct call {
        using result_type = typename fn_type::result_type;
    };

    template <typename... Args>
    typename call<Args...>::result_type operator() (Args... args) {
        return fn(args...);
    }
};
#endif

template <typename... Fns>
chain<Fns...> new_chain(Fns&&... fns) {
    return std::move(chain<Fns...>(std::forward<Fns>(fns)...));
}


template <typename Fn, typename... Fns>
struct sequence {
    using fn_wrapper = wrapper<Fn>;
    using fns_seq    = sequence<Fns...>;

    fn_wrapper fn;
    fns_seq    fns;

    sequence(Fn fn_, Fns... fns_): fn(fn_), fns(fns_...) {}

    template <typename... Args>
    void operator () (Args... args) {
        fn(args...);
        fns(args...);
    }
};

template <typename Fn>
struct sequence<Fn> {
    wrapper<Fn> fn;

    sequence(Fn fn_): fn(fn_) {}

    template <typename... Args>
    void operator() (Args... args) {
        fn(args...);
    }
};

}  // end of namespace function


/**
 *  \brief  Convenience \c runtime_wrapper creation function
 *
 *  \param  fn  function
 *
 *  \return Function wrapper
 */
template <typename Fn>
rt_wrapper_t<Fn> rt_wrapper(Fn && fn) {
    return rt_wrapper_t<Fn>(std::move(fn));
}

/** Convenience \c rt_wrapper_t creation function (for \c std::function) */
template <typename Fn>
rt_wrapper_t<std::function<Fn> > rt_wrapper(std::function<Fn> fn) {
    return rt_wrapper_t<std::function<Fn> >(fn);
}



struct R{};
struct A{};
struct B{};


namespace {

template <typename Fn, typename R, size_t i, typename Arg, typename... Args>
struct check_types_impl {
    typedef typename function_traits<Fn>::template arg<i>::type arg_t;

    static constexpr bool value =
        std::is_same<Arg, arg_t>::value &&
        check_types_impl<Fn, R, i + 1, Args...>::value;
};

template <typename Fn, typename R, size_t i, typename Arg>
struct check_types_impl<Fn, R, i, Arg> {
    typedef typename function_traits<Fn>::template arg<i>::type arg_t;
    typedef typename function_traits<Fn>::result_type result_t;

    static constexpr bool value =
        std::is_same<R, result_t>::value &&
        std::is_same<Arg, arg_t>::value;
};

}  // end of anonymous namespace

template <typename Fn, typename R, typename... Args>
struct check_types {
    static constexpr bool value = check_types_impl<Fn, R, 0, Args...>::value;
};


class functor {
    public:

    functor() {}

    //functor(const functor & ) = delete;

    //functor(functor && ) {}

    int operator () (char c) const { return (int)c; }
};


class functor_nocopy {
    public:

    functor_nocopy() {}

    functor_nocopy(const functor_nocopy & ) = delete;

    functor_nocopy(functor_nocopy && ) {}

    int operator () (char c) const { return (int)c; }
};


char to_lower(char c) {
    return std::tolower(c);
}


int main(int argc, char * const argv[]) {
    typedef std::function<R(A,B)> fn;

    std::cout
        << std::is_same<R, function_traits<fn>::result_type>::value
        << std::endl;

    std::cout
        << std::is_same<A, function_traits<fn>::arg<0>::type>::value
        << std::endl;

    std::cout
        << std::is_same<B, function_traits<fn>::arg<1>::type>::value
        << std::endl;

    std::cout
        << "Checking std::function: "
        << check_types<fn, R, A, B>::value
        << std::endl;

    auto lambda1 = [](int a, float b) -> bool {
        return 0.0 == a * b;
    };

    typedef std::function<bool(int, float)> lambda1_t;

    std::cout
        << "Checking lambda1: "
        << check_types<lambda1_t, bool, int, float>::value
        << std::endl;

    lambda1_t func1 = lambda1;

    typedef std::function<int(char)> functor_t;
    functor_t func2(functor());

    auto lambda2_wrapper = new_wrapper([](int i) -> double {
        return 2.0 * i;
    });

    using lambda2_result_type =
        decltype(lambda2_wrapper)::call<int>::result_type;

    std::cout
        << "Checking lambda2 return type: "
        << std::is_same<double, lambda2_result_type>::value
        << std::endl;

    auto lambda3_wrapper = new_wrapper([](lambda2_result_type d) -> float {
        return (float)d;
    });

    using lambda3_result_type =
        decltype(lambda3_wrapper)::call<lambda2_result_type>::result_type;

    std::cout
        << "Checking lambda3 return type: "
        << std::is_same<float, lambda3_result_type>::value
        << std::endl;

    std::cout
        << "Lambda2 call: "
        << lambda2_wrapper(123)
        << std::endl;

    auto chain1 = new_chain(
        [](int i, bool times2) -> float {
            return (float)(times2 ? 2 * i : i);
        },
        [](float f) -> double {
            return (double)f * 1.5;
        },
        [](double d) -> bool {
            return d < 5.0;
        });

    std::cout
        << "Chain1 call: "
        << chain1(2, true)
        << std::endl;

    std::function<char(char)> tolower = to_lower;

    wrapper<std::function<char(char)> > std_fn_wrapper(tolower);
    auto std_fn_wrapper2 = new_wrapper(tolower);

    auto chain2 = new_chain(
        []() { return 'F'; },
        functor(),
        [](int i) -> char { return (char)(i + 1); },
        std::function<char(char)>(to_lower),
        std::move(tolower),
        to_lower
    );

    std::cout
        << "Chain2 call: "
        << chain2()
        << std::endl;

    functor_nocopy fn_nocopy_;
    functor_nocopy fn_nocopy(std::move(fn_nocopy_));

    auto functor_nocopy_wrapper = new_wrapper(std::move(functor_nocopy()));

    std::cout
        << "Functor no-copy wrapper: "
        << functor_nocopy_wrapper('X')
        << std::endl;

    auto chain_functor_nocopy = new_chain(
        functor_nocopy(),
        functor_nocopy()
    );

    std::cout
        << "Chain no-copy chain: "
        << chain_functor_nocopy('X')
        << std::endl;

    return 0;
}

}}  // end of namespace meta libprocxx

#endif  // end of #ifndef libprocxx__meta__function_hxx
