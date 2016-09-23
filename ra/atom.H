
// (c) Daniel Llorens - 2011-2015

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

#ifndef RA_ATOM_H
#define RA_ATOM_H

/// @file atom.H
/// @brief Terminal nodes for expression templates.

#include "ra/traits.H"
#include "ra/opcheck.H"

#ifdef RA_CHECK_BOUNDS
    #define RA_CHECK_BOUNDS_RA_ATOM RA_CHECK_BOUNDS
#else
    #ifndef RA_CHECK_BOUNDS_RA_ATOM
        #define RA_CHECK_BOUNDS_RA_ATOM 1
    #endif
#endif
#if RA_CHECK_BOUNDS_RA_ATOM==0
    #define CHECK_BOUNDS( cond )
#else
    #define CHECK_BOUNDS( cond ) assert( cond )
#endif

namespace ra {

#define BAD_INTERFACE(X) static_assert(X, "bad use of Driver interface"); return 0;

// value_type may be needed to avoid conversion issues.
template <int w_, class value_type=ra::dim_t>
struct TensorIndex
{
    static_assert(w_>=0, "bad TensorIndex");

    constexpr static int w = w_;
    constexpr static rank_t rank_s() { return w+1; }
    constexpr static rank_t rank() { return w+1; }
    constexpr static dim_t size_s() { return DIM_BAD; }

    constexpr void adv(rank_t const k, dim_t const d) {}
    template <class I> constexpr value_type at(I const & i) const { return value_type(i[w]); }

    constexpr static dim_t size(int const k) { return DIM_BAD; } // used in shape checks with dyn. rank.
    dim_t stride(int const i) const { BAD_INTERFACE(w<0) } // used by Expr::stride_t.
    value_type * flat() const { BAD_INTERFACE(w<0) } // used by Expr::atom_type type signature.
};

#undef BAD_INTERFACE

constexpr TensorIndex<0, int> _0 {};
constexpr TensorIndex<1, int> _1 {};
constexpr TensorIndex<2, int> _2 {};
constexpr TensorIndex<3, int> _3 {};

template <class S> struct Scalar;

// Leaf type for Scalar. This is separate from Scalar itself so that operator+=, etc. can have the default use.
template <class S>
struct ScalarFlat: public Scalar<S>
{
    constexpr void operator+=(dim_t const d) const {}
    constexpr S const & operator*() const { return this->s; }
};

// Wrap a scalar for traversal.
// f(T) will normally be a specialization, so this doesn't drive.
template <class S>
struct Scalar
{
    constexpr static rank_t rank_s() { return 0; }
    constexpr static rank_t rank() { return 0; }
    constexpr static dim_t size_s() { return 1; }

    using shape_type = std::array<dim_t, 0>;
    constexpr static shape_type shape() { return shape_type {}; }

    S s;
    constexpr Scalar(S && s_): s(std::forward<S>(s_)) {}

// Scalar acts as driver in a few places where I didn't bother to specialize (e.g. ra::from). Elsewhere (e.g. in +, *, operator=, etc.) there's always another expr to be driver, or a specialization (e.g. array constructors). Use in ra::from should be removed since a scalar subscript can be beaten; this is probably true of all other uses, and @TODO then we could remove this.
    constexpr static void adv(rank_t const k, dim_t const d) {}
    template <class I> constexpr S const & at(I const & i) const { return s; }

// Used in shape checks with dynamic rank. (Never called because rank is 0).
    constexpr static dim_t size(int const k) { assert(0); return 0; }

// @TODO The result is never used, so maybe there's a way to remove this. @TODO All constexpr.
    constexpr static dim_t stride(int const i) { return 0; }
    constexpr static bool compact_stride(dim_t const step, int const z, int const j) { return true; }
    constexpr ScalarFlat<S> const & flat() const { return static_cast<ScalarFlat<S> const &>(*this); }

// impl depends on ra::Expr, see expr.H
#define DEF_EQOPS(OP) template <class X> void operator OP(X && x);
    FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS
};

// For the use of std::forward<>, see eg http://www.justsoftwaresolutions.co.uk/cplusplus/rvalue_references_and_perfect_forwarding.html
template <class S>
constexpr Scalar<S> scalar(S && s)
{
    return Scalar<S> { std::forward<S>(s) };
}

// Wrap something with {size, begin} as 1-D vector. Sort of reduced ra_iterator.
// ra::ra_traits_def<V> must be defined with ::size, ::size_s.
// @TODO This can handle temporaries and make_a().begin() can't, look out for that.
template <class V>
struct Vector
{
    using traits = ra_traits<V>;

    V v;
    decltype(v.begin()) p__;
    static_assert(!std::is_reference<decltype(p__)>::value, "bad iterator type");

    constexpr dim_t size() const { return traits::size(v); }
    constexpr dim_t size(int const i) const { CHECK_BOUNDS(i==0); return traits::size(v); }
    constexpr static dim_t size_s() { return traits::size_s(); }
    constexpr static rank_t rank() { return 1; }
    constexpr static rank_t rank_s() { return 1; };
    using shape_type = std::array<dim_t, 1>;
    constexpr auto shape() const { return shape_type { { dim_t(traits::size(v)) } }; }

// see test-compatibility.C [a1] for forward() here.
    Vector(V && v_): v(std::forward<V>(v_)), p__(v.begin()) {}

    void adv(rank_t const k, dim_t const d)
    {
// k>0 happens on frame-matching when the axes k>0 can't be unrolled; see [trc-01] in test-compatibility.C.
// k==0 && d!=1 happens on turning back at end of ply; @TODO we need this only on outer products and such.
        CHECK_BOUNDS(d==1 || d<0);
        p__ += (k==0) * d;
    }
    template <class I>
    decltype(auto) at(I const & i)
    {
        CHECK_BOUNDS(inside(i[0], this->size()));
        return p__[i[0]];
    }
    constexpr static dim_t stride(int const i) { return i==0 ? 1 : 0; }
    constexpr static bool compact_stride(dim_t const step, int const z, int const j)
    {
        return (z==0) == (j==0); // reduced from cell_iterator::compact_stride()
    }
    constexpr auto flat() { return p__; }
    constexpr auto flat() const { return p__; }

// impl depends on ra::Expr, see expr.H
#define DEF_EQOPS(OP) template <class X> void operator OP(X && x);
    FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS
};

template <class V> inline
constexpr Vector<V> vector(V && v)
{
    return Vector<V> { std::forward<V>(v) };
}

template <class T> inline
constexpr Vector<std::initializer_list<T>> vector(std::initializer_list<T> && v)
{
    return Vector<std::initializer_list<T>> { std::move(v) };
}

// Like Vector, but on the iterator itself, so no size, P only needs to have +=, *, [].
// ra::ra_traits_def<P> doesn't need to be defined.
template <class P>
struct Ptr
{
    P p__;

    constexpr static dim_t size() { return DIM_BAD; }
    constexpr static dim_t size(int const i) { CHECK_BOUNDS(i==0); return DIM_BAD; }
    constexpr static dim_t size_s() { return DIM_BAD; }
    constexpr static rank_t rank() { return 1; }
    constexpr static rank_t rank_s() { return 1; };
    using shape_type = std::array<dim_t, 1>;
    constexpr static auto shape() { return shape_type { { dim_t(DIM_BAD) } }; }

    constexpr Ptr(P const & p_): p__(p_) {}

    constexpr void adv(rank_t const k, dim_t const d)
    {
        CHECK_BOUNDS(d==1 || d<0); // cf Vector::adv
        p__ += (k==0) * d;
    }
    template <class I>
    constexpr decltype(auto) at(I const & i)
    {
        return p__[i[0]];
    }
    constexpr static dim_t stride(int const i) { return i==0 ? 1 : 0; }
    constexpr static bool compact_stride(dim_t const step, int const z, int const j)
    {
        return (z==0) == (j==0); // cf Vector::compact_stride
    }
    constexpr auto flat() { return p__; }
    constexpr auto flat() const { return p__; }

// impl depends on ra::Expr, see expr.H
#define DEF_EQOPS(OP) template <class X> void operator OP(X && x);
    FOR_EACH(DEF_EQOPS, =, *=, +=, -=, /=)
#undef DEF_EQOPS
};

template <class T> inline
constexpr Ptr<T *> ptr(T * p)
{
    return Ptr<T *> { p };
}

template <class T>
struct IotaLeaf
{
    T i_, stride_;
    T const & operator*() const { return i_; } // @TODO if not for this, I could use plain T. Maybe ra::eval_expr...
    void operator+=(dim_t const d) { i_ += T(d)*stride_; }
};

template <class T_>
struct Iota
{
    using T = T_;
    dim_t const size_;
    T const org_;
    T const stride_;
    T i_;

    using shape_type = std::array<dim_t, 1>;
    constexpr auto shape() const { return shape_type { { size_ } }; }

    constexpr dim_t size() const { return size_; } // this is a Slice function...
    constexpr dim_t size(int const i) const { CHECK_BOUNDS(i==0); return size_; }
    constexpr rank_t rank() const { return 1; }
    constexpr static rank_t rank_s() { return 1; };
    constexpr static dim_t size_s() { return DIM_ANY; }

    constexpr Iota(dim_t size, T org=0, T stride=1): size_(size), org_(org), stride_(stride), i_(org)
    {
        CHECK_BOUNDS(size>=0);
    }

    constexpr void adv(rank_t const k, dim_t const d)
    {
        i_ += T((k==0) * d) * stride_; // cf Vector::adv below.
    }
    template <class I>
    constexpr decltype(auto) at(I const & i)
    {
        return org_ + T(i[0])*stride_;
    }
    constexpr static dim_t stride(rank_t const i) { return i==0 ? 1 : 0; }
    constexpr static bool compact_stride(dim_t const step, int const z, int const j)
    {
        return (z==0) == (j==0); // reduced from cell_iterator::compact_stride()
    }
    constexpr auto flat() const { return IotaLeaf<T> { i_, stride_ }; }
};

template <class O=dim_t, class S=O> inline
constexpr auto
iota(dim_t size, O org=0, S stride=1)
{
    using T = std::common_type_t<O, S>;
    return Iota<T> { size, T(org), T(stride) };
}

template <class I> struct is_beatable_def
{
    constexpr static bool value = std::is_integral<I>::value;
    constexpr static int skip_src = 1;
    constexpr static int skip = 0;
    constexpr static bool static_p = value; // can the beating can be resolved statically?
};

template <class II> struct is_beatable_def<Iota<II>>
{
    constexpr static bool value = std::numeric_limits<II>::is_integer;
    constexpr static int skip_src = 1;
    constexpr static int skip = 1;
    constexpr static bool static_p = false; // it cannot for Iota
};

template <int n> struct is_beatable_def<dots_t<n> >
{
    static_assert(n>=0, "bad count for dots_n");
    constexpr static bool value = (n>=0);
    constexpr static int skip_src = n;
    constexpr static int skip = n;
    constexpr static bool static_p = true;
};

template <class I> using is_beatable = is_beatable_def<std::decay_t<I> >;
template <class I> struct beatable_skip { struct type { constexpr static int value = is_beatable<I>::skip; }; };

template <class X> constexpr bool has_tensorindex_def = false;
template <class T> constexpr bool has_tensorindex = has_tensorindex_def<std::decay_t<T> >;
template <int w, class value_type> constexpr bool has_tensorindex_def<TensorIndex<w, value_type> > = true;

template <class Op, class T, class K=std::make_integer_sequence<int, mp::len<T> > > struct Expr;
template <class T, class K=std::make_integer_sequence<int, mp::len<T> > > struct Pick;
template <class FM, class Op, class T, class K=std::make_integer_sequence<int, mp::len<T> > > struct Ryn;
template <class LiveAxes, int depth, class A> struct ApplyFrames;

template <class Op, class ... Ti, class K>
constexpr bool has_tensorindex_def<Expr<Op, std::tuple<Ti ...>, K>> = (has_tensorindex<Ti> || ...);

template <class ... Ti, class K>
constexpr bool has_tensorindex_def<Pick<std::tuple<Ti ...>, K>> = (has_tensorindex<Ti> || ...);

template <class LiveAxes, int depth, class A>
constexpr bool has_tensorindex_def<ApplyFrames<LiveAxes, depth, A>> = has_tensorindex<A>;

template <class FM, class Op, class ... Ti, class K>
constexpr bool has_tensorindex_def<Ryn<FM, Op, std::tuple<Ti ...>, K>> = (has_tensorindex<Ti> || ...);

} // namespace ra

#undef CHECK_BOUNDS
#undef RA_CHECK_BOUNDS_RA_ATOM

#endif // RA_ATOM_H