
// (c) Daniel Llorens - 2011-2014, 2016

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

#ifndef RA_EXPR_H
#define RA_EXPR_H

/// @file expr.H
/// @brief Expression templates for arrays with frame matching: operator nodes.

#include "ra/ply.H"

#ifdef RA_CHECK_BOUNDS
    #define RA_CHECK_BOUNDS_RA_EXPR RA_CHECK_BOUNDS
#else
    #ifndef RA_CHECK_BOUNDS_RA_EXPR
        #define RA_CHECK_BOUNDS_RA_EXPR 1
    #endif
#endif
#if RA_CHECK_BOUNDS_RA_EXPR==0
    #define CHECK_BOUNDS( cond )
#else
    #define CHECK_BOUNDS( cond ) assert( cond )
#endif

namespace ra {

// Manipulate ET through flat (raw pointer-like) iterators P ...
template <class Op, class T, class I=std::make_integer_sequence<int, mp::len<T> > >
struct Flat;

template <class Op, class ... P, int ... I>
struct Flat<Op, std::tuple<P ...>, std::integer_sequence<int, I ...> >
{
    using T = std::tuple<P ...>;

    Op & op;
    T t;
    Flat(Op & op_, P ... p_): op(op_), t(p_ ...) {}

// @TODO after https://gcc.gnu.org/bugzilla/show_bug.cgi?id=68377 try std::get<I>(t) += std::get<I>(s), ...;
    template <int II, class S> void add(S const & s) { std::get<II>(t) += std::get<II>(s); }
    template <class S> void operator+=(S const & s) { (add<I>(s), ...); }
    decltype(auto) operator*() { return op(*std::get<I>(t) ...); } // @TODO std::apply(op, t)
};

template <class Op, class ... P> inline constexpr
auto expr(Op && op, P && ... p)
{
    return Expr<Op, std::tuple<P ...>> { std::forward<Op>(op), std::forward<P>(p) ... };
}

// forward decl in atom.H
template <class Op, class ... P, int ... I>
struct Expr<Op, std::tuple<P ...>, std::integer_sequence<int, I ...>>
{
    using T = std::tuple<P ...>;

// A-th argument decides rank and shape.
    constexpr static int A = largest_rank<P ...>::value;
    using PA = std::decay_t<mp::Ref_<T, A>>;
    using NotA = mp::ComplementList_<mp::int_list<A>, std::tuple<mp::int_t<I> ...> >;
    using flat_t = Flat<Op, std::tuple<decltype(std::declval<P>().flat()) ...>>;
    using stride_t = std::tuple<decltype(std::declval<P>().stride(0)) ...>;

    Op op;
    T t;

// If driver is RANK_ANY, driver selection should wait til run time, unless we can tell statically that RANK_ANY would be selected anyway.
    constexpr static bool HAS_DRIVER = PA::size_s()!=DIM_BAD ; //&& (PA::rank_s()!=RANK_ANY || sizeof...(P)==1);

    template <bool depend=true> constexpr auto & driver() const
    {
        static_assert(HAS_DRIVER, "can't drive this xpr");
        return std::get<A>(t);
    }

    template <int iarg>
    std::enable_if_t<(iarg==mp::len<NotA>), bool>
    check(int const driver_rank) const { return true; }

    template <int iarg>
    std::enable_if_t<(iarg<mp::len<NotA>), bool>
    check(int const driver_rank) const
    {
        rank_t ranki = std::get<mp::Ref_<NotA, iarg>::value>(t).rank();
// Provide safety where RANK_ANY was selected as driver in a leap of faith. @TODO Disable this test when it's known statically that it isn't needed. @TODO Do dynamic driver selection if static is uncertain.
        assert(ranki<=driver_rank && "driver not max rank (could be RANK_ANY)");
        for (int k=0; k!=ranki; ++k) {
// sk0==DIM_BAD might happen on subexpressions.
            dim_t sk0 = std::get<A>(t).size(k);
            if (sk0!=DIM_BAD) {
                dim_t sk = std::get<mp::Ref_<NotA, iarg>::value>(t).size(k);
                assert((sk==sk0 || sk==DIM_BAD) && "mismatched dimensions");
            }
        }
        return check<iarg+1>(driver_rank);
    }

// see test-compatibility.C [a1] for forward() here.
    constexpr Expr(Op op_, P ... p_): op(std::forward<Op>(op_)), t(std::forward<P>(p_) ...)
    {
// @TODO Try to static_assert. E.g., size_s() vs size_s() can static_assert if we try real3==real2.
// @TODO Should check only the driver: do this on ply.
        CHECK_BOUNDS(check<0>(rank()));
    }

    template <class J> constexpr decltype(auto) at(J const & i)
    {
        return op(std::get<I>(t).at(i) ...);
    }
    constexpr void adv(rank_t const k, dim_t const d)
    {
        (std::get<I>(t).adv(k, d), ...);
    }

// there's one size (by A), but each arg has its own strides.
// Note: do not require driver. This is needed by check for all leaves.
    constexpr auto size(int const i) const -> dim_t
    {
        return std::get<A>(t).size(i);
    }
    constexpr bool compact_stride(dim_t const step, int const z, int const j) const
    {
        return (std::get<I>(t).compact_stride(step, z, j) && ...);
    }
    auto stride(int const i) const -> stride_t // @BUG clang 3.9 issues with constexpr/TensorIndex
    {
        return stride_t(std::get<I>(t).stride(i) ...);
    }
    auto flat() const -> flat_t // clang complains if this is constexpr, gcc does not
    {
        return flat_t(op, std::get<I>(t).flat() ...);
    }
    auto flat() -> flat_t // clang complains if this is constexpr, gcc does not
    {
        return flat_t(op, std::get<I>(t).flat() ...);
    }

    constexpr static dim_t size_s() { return PA::size_s(); }
    constexpr static rank_t rank_s() { return PA::rank_s(); }
    constexpr rank_t rank() const { return std::get<A>(t).rank(); }
    constexpr decltype(auto) shape() const { return driver().shape(); }

// needed for xpr with rank_s()==RANK_ANY, which don't decay to scalar when used as operator arguments.
    operator decltype(*std::declval<flat_t>())()
    {
        static_assert(rank_s()==0 || rank_s()==RANK_ANY || (rank_s()==1 && size_s()==1), // for coord types
                      "bad rank in conversion to scalar");
        assert(rank()==0 || (rank_s()==1 && size_s()==1)); // for coord types; so fixed only
        return *flat();
    }

// forward to make sure value y is not misused as ref. Cf. test-ra-8.C
#define DEF_EQOPS_(OP) OP(X && x) \
    { for_each([](auto && y, auto && x) { std::forward<decltype(y)>(y) OP x; }, *this, x); }

#define DEF_EQOPS(OP) template <class X> void operator DEF_EQOPS_(OP)
    FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS
};

#define DEF_EQOPS(OP) template <class V> template <class X> void Vector<V>::operator DEF_EQOPS_(OP)
FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS
#define DEF_EQOPS(OP) template <class V> template <class X> void Ptr<V>::operator DEF_EQOPS_(OP)
FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS
#define DEF_EQOPS(OP) template <class V> template <class X> void Scalar<V>::operator DEF_EQOPS_(OP)
FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS

#undef DEF_EQOPS_

// others:
// * 'static': Like expr, but the operator is compile time (e.g. ::apply()).
// * 'dynamic': Like expr, but driver is selected at run time (e.g. if args are RANK_ANY).
// * ...

// Wrappers over expr & ply_either.

template <class F, class ... A> inline
auto map(F && f, A && ... a)
{
    return expr(std::forward<F>(f), start(std::forward<A>(a)) ...);
}

template <class F, class ... A> inline
void for_each(F && f, A && ... a)
{
    ply_either(expr(std::forward<F>(f), start(std::forward<A>(a)) ...));
}

} // namespace ra

#undef CHECK_BOUNDS
#undef RA_CHECK_BOUNDS_RA_EXPR

#endif // RA_EXPR_H