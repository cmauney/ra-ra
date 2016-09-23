
// (c) Daniel Llorens - 2014-2016

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

#ifndef RA_OPERATORS_H
#define RA_OPERATORS_H

/// @file operators.H
/// @brief Sugar for ra:: expression templates.

#include "ra/wrank.H"
#include "ra/pick.H"
// @TODO These (dependence on specific ra:: types) should be elsewhere.
#include "ra/small.H"
#include "ra/large.H"
#include "ra/complex.H"
#include "ra/wedge.H"

#ifdef RA_CHECK_BOUNDS
#define RA_CHECK_BOUNDS_RA_OPERATORS RA_CHECK_BOUNDS
#else
#ifndef RA_CHECK_BOUNDS_RA_OPERATORS
#define RA_CHECK_BOUNDS_RA_OPERATORS 1
#endif
#endif
#if RA_CHECK_BOUNDS_RA_OPERATORS==0
#define CHECK_BOUNDS( cond )
#else
#define CHECK_BOUNDS( cond ) assert( cond )
#endif

#include "ra/optimize.H"

#ifndef RA_OPTIMIZE
#define RA_OPTIMIZE 1 // enabled by default
#endif
#if RA_OPTIMIZE==1
#define OPTIMIZE optimize
#else
#define OPTIMIZE
#endif

#define START(a) *(ra::start(a).flat())

// This global version must be available so that ra::transpose<> may be searched by ADL even when giving explicit template args. See http://stackoverflow.com/questions/9838862 . See also wedge() in ra/wedge.H.
template <class ... Iarg, class A, std::enable_if_t<ra::is_scalar<A>, int> =0>
decltype(auto) transpose(A && a)
{
    static_assert(sizeof...(Iarg)==0, "bad transpose axes list for scalar");
    return START(a);
}

// See ::wedge() in ra/wedge.H, need ra::is_scalar<> to define.
// These ra::start are needed b/c rank 0 converts to and from scalar, so ? can't pick the right (-> scalar) conversion.
template <class T, class F,
          std::enable_if_t<ra::is_zero_or_scalar<T> && ra::is_zero_or_scalar<F>, int> =0>
inline decltype(auto) where(bool const w, T && t, F && f) { return w ? START(t) : START(f); }

namespace ra {

template <int ... Iarg, class A>
inline decltype(auto) transpose(A && a, mp::int_list<Iarg ...>)
{
    return transpose<Iarg ...>(std::forward<A>(a));
}

// I considered three options for lookup.
// 1. define these in a class that ArrayIterator or Container or Slice types derive from. This was done for an old library I had (vector-ops.H). It results in the narrowest scope, but since those types are used in the definition (ra::Expr is an ArrayIterator), it requires lots of forwarding and traits:: .
// 2. raw ADL doesn't work because some ra:: types use ! != etc for different things (e.g. Flat). Possible solution: don't ever use + != == for Flat.
// 3. enable_if'd ADL is what you see here.

// --------------------------------
// Array versions of operators and functions
// --------------------------------

// We need the zero/scalar specializations because the scalar/scalar operators
// maybe be templated (e.g. complex<>), so they won't be found when an implicit
// conversion from zero->scalar is also needed. That is, without those
// specializations, ra::View<complex, 0> * complex will fail.

// These depend on OPNAME defined in optimize.H and used there to match ET patterns.
#define DEF_NAMED_BINARY_OP(OP, OPNAME)                                 \
    template <class A, class B, std::enable_if_t<ra_pos_and_any<A, B>, int> =0> \
    inline auto operator OP(A && a, B && b)                             \
    {                                                                   \
        return OPTIMIZE(map(OPNAME(), std::forward<A>(a), std::forward<B>(b))); \
    }                                                                   \
    template <class A, class B, std::enable_if_t<ra_zero<A, B>, int> =0> \
    inline auto operator OP(A && a, B && b)                             \
    {                                                                   \
        return START(a) OP START(b);                                    \
    }
DEF_NAMED_BINARY_OP(+, plus)
DEF_NAMED_BINARY_OP(-, minus)
DEF_NAMED_BINARY_OP(*, times)
DEF_NAMED_BINARY_OP(/, slash)
#undef DEF_NAMED_BINARY_OP

#define DEF_BINARY_OP(OP)                                               \
    template <class A, class B, std::enable_if_t<ra_pos_and_any<A, B>, int> =0> \
    inline auto operator OP(A && a, B && b)                             \
    {                                                                   \
        return map([](auto && a, auto && b) { return a OP b; },         \
                   std::forward<A>(a), std::forward<B>(b));             \
    }                                                                   \
    template <class A, class B, std::enable_if_t<ra_zero<A, B>, int> =0> \
    inline auto operator OP(A && a, B && b)                             \
    {                                                                   \
        return START(a) OP START(b);                                    \
    }
FOR_EACH(DEF_BINARY_OP, &&, ||, >, <, >=, <=, ==, !=)
#undef DEF_BINARY_OP

#define DEF_UNARY_OP(OP)                                                \
    template <class A, std::enable_if_t<is_ra_pos_rank<A>, int> = 0>    \
    inline auto operator OP(A && a)                                     \
    {                                                                   \
        return map([](auto && a) { return OP a; }, std::forward<A>(a)); \
    }
FOR_EACH(DEF_UNARY_OP, !, +, -) // @TODO Make + into nop.
#undef DEF_UNARY_OP

// When OP(a) isn't found from ra::, the deduction from rank(0) -> scalar doesn't work.
// @TODO Cf [ref:examples/useret.C:0].
#define DEF_NAME_OP(OP)                                                 \
    using ::OP;                                                         \
    template <class ... A, std::enable_if_t<ra_pos_and_any<A ...>, int> =0> \
    inline auto OP(A && ... a)                                          \
    {                                                                   \
        return map([](auto && ... a) { return OP(a ...); }, std::forward<A>(a) ...); \
    }                                                                   \
    template <class ... A, std::enable_if_t<ra_zero<A ...>, int> =0>    \
    inline auto OP(A && ... a)                                          \
    {                                                                   \
        return OP(START(a) ...);                                        \
    }
FOR_EACH(DEF_NAME_OP, rel_error, pow, xI, conj, sqr, sqrm, abs, cos, sin)
FOR_EACH(DEF_NAME_OP, exp, sqrt, log, log1p, log10, isfinite, isnan, isinf)
FOR_EACH(DEF_NAME_OP, max, min, odd, asin, acos, atan, atan2)
FOR_EACH(DEF_NAME_OP, cosh, sinh, tanh)
#undef DEF_NAME_OP

#define DEF_NAME_OP(OP)                                                 \
    using ::OP;                                                         \
    template <class ... A, std::enable_if_t<ra_pos_and_any<A ...>, int> =0> \
    inline auto OP(A && ... a)                                          \
    {                                                                   \
        return map([](auto && ... a) -> decltype(auto) { return OP(a ...); }, std::forward<A>(a) ...); \
    }                                                                   \
    template <class ... A, std::enable_if_t<ra_zero<A ...>, int> =0>    \
    inline decltype(auto) OP(A && ... a)                                \
    {                                                                   \
        return OP(START(a) ...);                                        \
    }
FOR_EACH(DEF_NAME_OP, real_part, imag_part)
#undef DEF_NAME_OP

template <class T, class A>
inline auto cast(A && a)
{
    return map([](auto && a) { return T(a); }, std::forward<A>(a));
}

// @TODO could be useful to deduce T as tuple of value_types (&).
template <class T, class ... A>
inline auto pack(A && ... a)
{
    return map([](auto && ... a) { return T { a ... }; }, std::forward<A>(a) ...);
}

template <class A, class I>
inline auto at(A && a, I && i)
{
    return map([&a](auto && i) -> decltype(auto) { return a.at(i); }, i);
}

template <class W, class T, class F, std::enable_if_t<is_ra_pos_rank<W> || is_ra_pos_rank<T> || is_ra_pos_rank<F>, int> = 0>
inline auto where(W && w, T && t, F && f)
{
    return pick(cast<bool>(start(std::forward<W>(w))), start(std::forward<F>(f)), start(std::forward<T>(t)));
}

// --------------------------------
// Some whole-array reductions.
// @TODO First rank reductions? Variable rank reductions?
// --------------------------------

// @TODO somehow make this work (ply_index_short_circuit is bad). Then variable rank...
template <class A>
inline auto index(A && a)
{
    struct index_exit { ra::dim_t i; };
    try {
        for_each([&](bool const a, auto && i) { if (a) { throw index_exit {i}; }; },
                 a, ra::iota(start(a).size(0)));
    } catch (index_exit & e) {
        return e.i;
    }
    return ra::dim_t(-1);
}

using std::min;
template <class A>
inline auto amin(A && a)
{
    using T = value_t<A>;
    T c(std::numeric_limits<T>::max());
    for_each([&c](auto && a) { c = min(c, a); }, a);
    return c;
}

using std::max;
template <class A>
inline auto amax(A && a)
{
    using T = value_t<A>;
    T c(std::numeric_limits<T>::lowest());
    for_each([&c](auto && a) { c = max(c, a); }, a);
    return c;
}

template <class A>
inline auto sum(A && a)
{
    value_t<A> c(0.);
    for_each([&c](auto && a) { c += a; }, a);
    return c;
}

template <class A>
inline auto prod(A && a)
{
    value_t<A> c(1.);
    for_each([&c](auto && a) { c *= a; }, a);
    return c;
}

template <class A>
inline auto reduce_sqrm(A && a)
{
    return sum(sqrm(a));
}

template <class A>
inline auto norm2(A && a)
{
    return sqrt(reduce_sqrm(a));
}

template <class A, class B>
inline auto dot(A && a, B && b)
{
    std::decay_t<decltype(START(a) * START(b))> c(0.);
    for_each([&c](auto && a, auto && b) { c = fma(a, b, c); }, a, b);
    return c;
}

template <class A, class B>
inline auto cdot(A && a, B && b)
{
    std::decay_t<decltype(conj(START(a)) * START(b))> c(0.);
    for_each([&c](auto && a, auto && b) { c = fma_conj(a, b, c); }, a, b);
    return c;
}

// --------------------
// Wedge product
// @TODO Handle the simplifications dot_plus, yields_scalar, etc. just as vec::wedge does.
// --------------------

template <class A>
struct torank1
{
    using type = mp::If_<is_scalar<A>, Small<std::decay_t<A>, 1>, A>;
};

template <class Wedge, class Va, class Vb, class Enable=void>
struct fromrank1
{
    using valtype = typename Wedge::template valtype<Va, Vb>;
    using type = mp::If_<Wedge::Nr==1, valtype, Small<valtype, Wedge::Nr> >;
};

#define DECL_WEDGE(condition)                                           \
    template <int D, int Oa, int Ob, class Va, class Vb,                \
              std::enable_if_t<!(is_scalar<Va> && is_scalar<Vb>), int> =0> \
    decltype(auto) wedge(Va const & a, Vb const & b)
DECL_WEDGE(general_case)
{
    Small<std::decay_t<decltype(START(a))>, decltype(start(a))::size_s()> aa = a;
    Small<std::decay_t<decltype(START(b))>, decltype(start(b))::size_s()> bb = b;

    using Ua = decltype(aa);
    using Ub = decltype(bb);

    typename fromrank1<fun::Wedge<D, Oa, Ob>, Ua, Ub>::type r;

    auto & r1 = reinterpret_cast<typename torank1<decltype(r)>::type &>(r);
    auto & a1 = reinterpret_cast<typename torank1<Ua>::type const &>(aa);
    auto & b1 = reinterpret_cast<typename torank1<Ub>::type const &>(bb);
    fun::Wedge<D, Oa, Ob>::product(a1, b1, r1);

    return r;
}
#undef DECL_WEDGE

#define DECL_WEDGE(condition)                                           \
    template <int D, int Oa, int Ob, class Va, class Vb, class Vr,      \
              std::enable_if_t<!(is_scalar<Va> && is_scalar<Vb>), int> =0> \
    void wedge(Va const & a, Vb const & b, Vr & r)
DECL_WEDGE(general_case)
{
    Small<std::decay_t<decltype(START(a))>, decltype(start(a))::size_s()> aa = a;
    Small<std::decay_t<decltype(START(b))>, decltype(start(b))::size_s()> bb = b;

    using Ua = decltype(aa);
    using Ub = decltype(bb);

    auto & r1 = reinterpret_cast<typename torank1<decltype(r)>::type &>(r);
    auto & a1 = reinterpret_cast<typename torank1<Ua>::type const &>(aa);
    auto & b1 = reinterpret_cast<typename torank1<Ub>::type const &>(bb);
    fun::Wedge<D, Oa, Ob>::product(a1, b1, r1);
}
#undef DECL_WEDGE

template <class A, class B, std::enable_if_t<std::decay_t<A>::size_s()==2 && std::decay_t<B>::size_s()==2, int> =0>
inline auto cross(A const & a_, B const & b_)
{
    Small<std::decay_t<decltype(START(a_))>, 2> a = a_;
    Small<std::decay_t<decltype(START(b_))>, 2> b = b_;
    Small<std::decay_t<decltype(START(a_) * START(b_))>, 1> r;
    fun::Wedge<2, 1, 1>::product(a, b, r);
    return r[0];
}

template <class A, class B, std::enable_if_t<std::decay_t<A>::size_s()==3 && std::decay_t<B>::size_s()==3, int> =0>
inline auto cross(A const & a_, B const & b_)
{
    Small<std::decay_t<decltype(START(a_))>, 3> a = a_;
    Small<std::decay_t<decltype(START(b_))>, 3> b = b_;
    Small<std::decay_t<decltype(START(a_) * START(b_))>, 3> r;
    fun::Wedge<3, 1, 1>::product(a, b, r);
    return r;
}

template <class V>
inline auto perp(V const & v)
{
    static_assert(v.size()==2, "dimension error");
    return Small<std::decay_t<decltype(START(v))>, 2> {v[1], -v[0]};
}
template <class V, class U, std::enable_if_t<is_scalar<U>, int> =0>
inline auto perp(V const & v, U const & n)
{
    static_assert(v.size()==2, "dimension error");
    return Small<std::decay_t<decltype(START(v) * n)>, 2> {v[1]*n, -v[0]*n};
}
template <class V, class U, std::enable_if_t<!is_scalar<U>, int> =0>
inline auto perp(V const & v, U const & n)
{
    static_assert(v.size()==3, "dimension error");
    return cross(v, n);
}

// --------------------
// Other whole-array ops. @TODO Cleaner way to get concrete types from maybe-exprs. (expr:: had resolve()).
// --------------------

// @TODO Would like to be able to return just a/norm2(a) without temp-ref errors.
template <class A, std::enable_if_t<is_slice<A>, int> =0>
inline auto normv(A const & a)
{
    static_assert(A::size_s()!=DIM_ANY, "waiting for ra::resolve or some such"); // gcc accepts a.size_s()
    return Small<std::decay_t<decltype(START(a))>, A::size_s()>(a/norm2(a));
}

template <class A, std::enable_if_t<!is_slice<A> && is_ra<A>, int> =0>
inline auto normv(A const & a)
{
    static_assert(A::size_s()!=DIM_ANY, "waiting for ra::resolve or some such");  // gcc accepts a.size_s()
    Small<std::decay_t<decltype(START(a))>, A::size_s()> b = a;
    b /= norm2(b);
    return b;
}

// @TODO Define within generalization...
template <class A, class B>
inline ra::Small<std::decay_t<decltype(START(std::declval<A>()) * START(std::declval<B>()))>, A::size(0), B::size(1)>
gemm(A const & a, B const & b)
{
    constexpr int M = a.size(0);
    constexpr int N = b.size(1);
    ra::Small<std::decay_t<decltype(START(std::declval<A>()) * START(std::declval<B>()))>, A::size(0), B::size(1)> c;
    for (int i=0; i<M; ++i) {
        for (int j=0; j<N; ++j) {
            c(i, j) = dot(a(i), b(ra::all, j));
        }
    }
    return c;
}

// the default for row-major x row-major. See bench-gemm.C for variants.
template <class S, class T>
inline auto
gemm(ra::View<S, 2> const & a, ra::View<T, 2> const & b)
{
    int const M = a.size(0);
    int const N = b.size(1);
    int const K = a.size(1);
    ra::Owned<std::decay_t<decltype(START(a)*START(b))>, 2> c({M, N}, 0.);
    for (int k=0; k<K; ++k) {
        c += from(times(), a(ra::all, k), b(k, ra::all));
    }
    return c;
}

template <class A, class B>
inline ra::Small<std::decay_t<decltype(START(std::declval<A>()) * START(std::declval<B>()))>, B::size(1)>
gevm(A const & a, B const & b)
{
    constexpr int N = b.size(1);
    ra::Small<std::decay_t<decltype(START(std::declval<A>()) * START(std::declval<B>()))>, B::size(1)> c;
    for (int j=0; j<N; ++j) {
        c(j) = dot(a, b(ra::all, j));
    }
    return c;
}

template <class S, class T>
inline auto
gevm(ra::View<S, 1> const & a, ra::View<T, 2> const & b)
{
    int const N = b.size(1);
    ra::Owned<decltype(a(0)*b(0, 0))> c({N}, ra::unspecified);
    for (int j=0; j<N; ++j) {
        c(j) = dot(a, b(ra::all, j));
    }
    return c;
}

template <class A, class B>
inline ra::Small<std::decay_t<decltype(START(std::declval<A>()) * START(std::declval<B>()))>, A::size(0)>
gemv(A const & a, B const & b)
{
    constexpr int M = a.size(0);
    ra::Small<std::decay_t<decltype(START(std::declval<A>()) * START(std::declval<B>()))>, A::size(0)> c;
    for (int i=0; i<M; ++i) {
        c(i) = dot(a(i), b);
    }
    return c;
}

template <class S, class T>
inline auto
gemv(ra::View<S, 2> const & a, ra::View<T, 1> const & b)
{
    int const M = a.size(0);
    ra::Owned<std::decay_t<decltype(START(a)*START(b))>, 1> c({M}, ra::unspecified);
    for (int i=0; i<M; ++i) {
        c(i) = dot(a(i), b);
    }
    return c;
}

#undef START

} // namespace ra

#undef CHECK_BOUNDS
#undef RA_CHECK_BOUNDS_RA_OPERATORS

#endif // RA_OPERATORS_H