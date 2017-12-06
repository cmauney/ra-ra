
// (c) Daniel Llorens - 2013-2014

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

/// @file big.H
/// @brief Arrays with dynamic size.

#pragma once
#include "ra/small.H"
#include "ra/iterator.H"
#include "ra/wrank.H"
#include <complex>
#include <memory>

#ifndef RA_CHECK_BOUNDS_BIG
  #ifndef RA_CHECK_BOUNDS
    #define RA_CHECK_BOUNDS_BIG 1
  #else
    #define RA_CHECK_BOUNDS_BIG RA_CHECK_BOUNDS
  #endif
#endif
#if RA_CHECK_BOUNDS_BIG==0
    #define CHECK_BOUNDS( cond )
#else
    #define CHECK_BOUNDS( cond ) assert( cond )
#endif

namespace ra {

// Always C order. If you need another, transpose this.
template <class S, class D>
dim_t filldim(int n, S send, D dend)
{
    dim_t next = 1;
    for (; n>0; --n) {
        --send;
        --dend;
        assert(*send>=0);
        (*dend).size = *send;
        (*dend).stride = next;
        next *= *send;
    }
    return next;
}

template <class D>
dim_t proddim(D d, D dend)
{
    dim_t t = 1;
    for (; d!=dend; ++d) {
        t *= (*d).size;
    }
    return t;
}

// raw <- shared; raw <- unique; shared <-- unique.
// layout is
// [data] (fixed shape)
// [size] p -> [data...] (fixed rank)
// [rank] [size...] p -> [data...] (var rank)
// TODO size is immutable so that it can be kept together with rank.

// Would do this with a base class, but I like View being POD.
// TODO In C++17 begin() end() may be different types. See if we can use this to simplify end() and !=end() test.
#define DEF_ITERATORS(RANK)                                             \
    template <rank_t c=0> using iterator = ra::ra_iterator<View<T, RANK>, c>; \
    template <rank_t c=0> using const_iterator = ra::ra_iterator<View<T const, RANK>, c>; \
    template <rank_t c=0> auto iter() { return iterator<c>(dim, p); }   \
    template <rank_t c=0> auto iter() const { return const_iterator<c>(dim, p); } \
    auto begin() { return stl_iterator<iterator<0>>(dim, p); }          \
    auto begin() const { return stl_iterator<const_iterator<0>>(dim, p); } \
    auto end() { return stl_iterator<iterator<0>> {}; }                 \
    auto end() const { return stl_iterator<const_iterator<0>> {}; }

template <class I> constexpr bool is_initializer_list = false;
template <class T> constexpr bool is_initializer_list<std::initializer_list<T> > = true;

template <rank_t k, class T> struct ranked_ilist_def;
template <rank_t k, class T> using ranked_ilist = typename ranked_ilist_def<k, T>::type;

template <rank_t k, class T> struct ranked_ilist_def
{
    static_assert(k>=0);
    using type = std::initializer_list<ranked_ilist<k-1, T>>;
};
template <class T> struct ranked_ilist_def<0, T> { using type = T; };
template <class T> struct ranked_ilist_def<RANK_ANY, T> { using type = ranked_ilist<1, T>; };

// See same thing for SmallBase.
#define DEF_ASSIGNOPS(OP)                                               \
    template <class X> View & operator OP (X && x)                      \
    {                                                                   \
        for_each([](T & y, T const & x) { y OP x; }, *this, x);         \
        return *this;                                                   \
    }
#define DEF_ASSIGNMENT_OPS                                              \
    FOR_EACH(DEF_ASSIGNOPS, =, *=, +=, -=, /=)                          \
    /* These three are unsafe */                                        \
    View(): p(nullptr) {}                                               \
    View(Dimv const & dim_, T * p_): dim(dim_), p(p_) {}                \
    View(std::initializer_list<dim_t> const s, T * p_): p(p_)           \
    {                                                                   \
        ra::resize(View::dim, s.size());                                \
        filldim(s.size(), s.end(), View::dim.end());                    \
    }                                                                   \
    template <class SS> View(SS const & s, T * p_): p(p_)               \
    {                                                                   \
        ra::resize(View::dim, s.size());                                \
        filldim(s.size(), s.end(), View::dim.end());                    \
    }                                                                   \
    View(View && x): dim(std::move(x.dim)), p(x.p) {}                   \
    View(View const & x): dim(x.dim), p(x.p) {}                         \
    View & operator=(View const & x)                                    \
    {                                                                   \
        for_each([](T & dest, T const & src) { dest = src; }, *this, x); \
        return *this;                                                   \
    }                                                                   \
    /* Ignore rank, row-major fill. TODO Use xpr traversal */          \
    View & operator=(std::initializer_list<T> const x)                  \
    {                                                                   \
        assert(p && this->size()==ra::dim_t(x.size()) && "bad assignment"); \
        std::copy(x.begin(), x.end(), this->begin());                   \
        return *this;                                                   \
    }                                                                   \
    bool const empty() const { return size()==0; } /* TODO Optimize */

inline dim_t select(Dim * dim, Dim const * dim_src, dim_t i)
{
    CHECK_BOUNDS(inside(i, dim_src->size));
    return dim_src->stride*i;
}
template <class II>
inline dim_t select(Dim * dim, Dim const * dim_src, ra::Iota<II> i)
{
    CHECK_BOUNDS((inside(i.org_, dim_src->size) && inside(i.org_+(i.size_-1)*i.stride_, dim_src->size))
                 || (i.size_==0 && i.org_<=dim_src->size));
    dim->size = i.size_;
    dim->stride = dim_src->stride * i.stride_;
    return dim_src->stride*i.org_;
}
template <class I0, class ... I>
inline dim_t select_loop(Dim * dim, Dim const * dim_src, I0 && i0, I && ... i)
{
    return select(dim, dim_src, std::forward<I0>(i0))
        + select_loop(dim+is_beatable<I0>::skip, dim_src+is_beatable<I0>::skip_src, std::forward<I>(i) ...);
}
template <int n, class ... I>
inline dim_t select_loop(Dim * dim, Dim const * dim_src, dots_t<n> dots, I && ... i)
{
    for (Dim * end = dim+n; dim!=end; ++dim, ++dim_src) {
        *dim = *dim_src;
    }
    return select_loop(dim, dim_src, std::forward<I>(i) ...);
}
template <int n, class ... I>
inline dim_t select_loop(Dim * dim, Dim const * dim_src, newaxis_t<n> dots, I && ... i)
{
    for (Dim * end = dim+n; dim!=end; ++dim) {
        dim->size = 1;
        dim->stride = 0;
    }
    return select_loop(dim, dim_src, std::forward<I>(i) ...);
}
inline dim_t select_loop(Dim * dim, Dim const * dim_src)
{
    return 0;
}

// TODO Parameterize on Child having .data() so that there's only one pointer.
// TODO A constructor, if only for CHECK_BOUNDS (positive sizes, strides inside, etc.)
template <class T, rank_t RANK>
struct View
{
    using Dimv = Small<Dim, RANK>;

    Dimv dim;
    T * p;
    constexpr static rank_t rank() { return RANK; }
    constexpr static rank_t rank_s() { return RANK; };
    constexpr dim_t size() const { return proddim(dim.begin(), dim.end()); }
    constexpr static dim_t size_s() { return DIM_ANY; }
    constexpr dim_t size(int j) const { return dim[j].size; }
    constexpr dim_t stride(int j) const { return dim[j].stride; }
    constexpr auto data() { return p; }
    constexpr auto data() const { return p; }

    // Specialize for rank() integer-args -> scalar, same in ra::SmallBase in small.H.
#define SUBSCRIPTS(CONST)                                               \
    template <class ... I,                                              \
              std::enable_if_t<((0 + ... + std::is_integral_v<I>)<RANK \
                                && (0 + ... + is_beatable<I>::value)==sizeof...(I)), int> = 0> \
    auto operator()(I const & ... i) CONST                              \
    {                                                                   \
        constexpr rank_t extended = (0 + ... + (is_beatable<I>::skip-is_beatable<I>::skip_src)); \
        constexpr rank_t subrank = rank_sum(RANK, extended);            \
        static_assert(subrank>=0, "bad subrank");                       \
        View<T CONST, subrank> sub;                                     \
        sub.p = data() + select_loop(sub.dim.data(), this->dim.data(), i ...); \
        /* fill the rest of dim, skipping over beatable subscripts */   \
        for (int i=(0 + ... + is_beatable<I>::skip); i<subrank; ++i) {  \
            sub.dim[i] = this->dim[i-extended];                         \
        }                                                               \
        return sub;                                                     \
    }                                                                   \
    /* BUG doesn't handle generic zero-rank indices */                 \
    template <class ... I, std::enable_if_t<(0 + ... + std::is_integral_v<I>)==RANK, int> = 0> \
    decltype(auto) operator()(I const & ... i) CONST                    \
    {                                                                   \
        return data()[select_loop(nullptr, this->dim.data(), i ...)];   \
    }                                                                   \
    /* TODO More than one selector... This still covers (unbeatable, integer) for example, which could be reduced. */ \
    template <class ... I, std::enable_if_t<!(is_beatable<I>::value && ...), int> = 0> \
    auto operator()(I && ... i) CONST                                   \
    {                                                                   \
        return from(*this, std::forward<I>(i) ...);                     \
    }                                                                   \
    template <class ... I>                                              \
    auto wipparens(I && ... i) CONST;                                   \
    template <class  I>                                                 \
    auto at(I const & i) CONST                                          \
    {                                                                   \
        using Sub = View<T CONST, rank_diff(RANK, I::size_s())>; /* gcc accepts i.size() */ \
        return Sub { typename Sub::Dimv(dim.begin()+i.size(0), dim.end()), \
                data() + Indexer1::index_p(dim, i) };                   \
    }                                                                   \
    decltype(auto) operator[](dim_t const i) CONST                      \
    {                                                                   \
        return (*this)(i);                                              \
    }
    SUBSCRIPTS(const)
    SUBSCRIPTS(/*const*/)
#undef SUBSCRIPTS

    DEF_ITERATORS(RANK)
    DEF_ASSIGNMENT_OPS

    operator T & () { static_assert(RANK==0, "bad rank"); return data()[0]; }
    operator T const & () const { static_assert(RANK==0, "bad rank"); return data()[0]; }
};

// TODO To be placed as specialization of operator() above.
template <class T, rank_t RANK>
template <class ... I>
auto View<T, RANK>::wipparens(I && ... i) /* const */
{
}

template <class T, rank_t RANK>
struct ra_traits_def<View<T, RANK>>
{
    using V = View<T, RANK>;
    using value_type = T;
    using shape_type = Small<dim_t, RANK>; // TODO should be taken from v.iter().shape()

    static auto shape(V const & v) { return v.iter().shape(); }
    static dim_t size(V const & v) { return v.size(); }
    constexpr static rank_t rank(V const & v) { return v.rank(); }
    constexpr static rank_t rank_s() { return RANK; };
    constexpr static dim_t size_s() { return RANK==0 ? 1 : DIM_ANY; }
};

// Storage types.

template <class V> struct storage_traits
{
    using T = std::decay_t<decltype(*std::declval<V>().get())>;
    static V create(dim_t n) { assert(n>=0); return V(new T[n]); }
    static T const * data(V const & v) { return v.get(); }
    static T * data(V & v) { return v.get(); }
};
template <class T> struct storage_traits<std::vector<T>>
{
// BUG This uses T(), so would need a create(, T) ...
    static std::vector<T> create(dim_t n) { return std::vector<T>(n); }
// BUG No such for std::vector<bool>
    static T const * data(std::vector<T> const & v) { return v.data(); }
    static T * data(std::vector<T> & v) { return v.data(); }
};

template <class T, rank_t RANK> inline
bool is_c_order(View<T, RANK> const & a)
{
    dim_t s = 1;
    for (int i=a.rank()-1; i>=0; --i) {
        if (s!=a.stride(i)) {
            return false;
        }
        s *= a.size(i);
        if (s==0) {
            return true;
        }
    }
    return true;
}

// TODO be convertible to View only, so that View::p is not duplicated
template <class Store, rank_t RANK>
struct WithStorage: public View<std::decay_t<decltype(*storage_traits<Store>::data(std::declval<Store>()))>, RANK>
{
    Store store;
    using T = std::decay_t<decltype(*storage_traits<Store>::data(store))>;
    using View = ra::View<T, RANK>;
    using View::rank_s;

    View & view() { return *this; }
    View const & view() const { return *this; }

// TODO Explicit definitions are needed only to have View::p set. Remove the duplication as in SmallBase/SmallArray, then remove these, both the constructors and the operator=s.
    WithStorage(WithStorage && w): store(std::move(w.store))
    {
        View::dim = std::move(w.dim);
        View::p = storage_traits<Store>::data(store);
    }
    WithStorage(WithStorage const & w): store(w.store)
    {
        View::dim = w.dim;
        View::p = storage_traits<Store>::data(store);
    }
    WithStorage(WithStorage & w): store(w.store)
    {
        View::dim = w.dim;
        View::p = storage_traits<Store>::data(store);
    }
// Override View::operator= to allow initialization-of-reference. Unfortunately operator>>(std::istream &, WithStorage &) requires it. The presence of these operator= means that A(shape 2 3) = type-of-A [1 2 3] initializes so it doesn't behave as A(shape 2 3) = not-type-of-A [1 2 3] which will use View::operator= and frame match. See test-ownership.C [ra20].
// TODO do this through .replace() op.
// TODO don't require copiable T from constructors, see fill1 below. That requires initialization and not update semantics for operator=.
    WithStorage & operator=(WithStorage && w)
    {
        store = std::move(w.store);
        View::dim = std::move(w.dim);
        View::p = storage_traits<Store>::data(store);
        return *this;
    }
    WithStorage & operator=(WithStorage const & w)
    {
        store = w.store;
        View::dim = w.dim;
        View::p = storage_traits<Store>::data(store);
        return *this;
    }
    WithStorage & operator=(WithStorage & w)
    {
        store = w.store;
        View::dim = w.dim;
        View::p = storage_traits<Store>::data(store);
        return *this;
    }

// Provided b/c want {} to call initializer_list constructor below. TODO Why do I need to init p; warning on test-io.C.
    WithStorage()
    {
        for (Dim & dimi: View::dim) { dimi = {0, 1}; } // 1 so we can push_back()
    }

    template <class SS> void init(SS const & s)
    {
        constexpr dim_t sr = ra_traits<decltype(s)>::size_s();
        static_assert(sr==RANK || sr==DIM_ANY || RANK==RANK_ANY, "rank mismatch for init shape");
        ra::resize(View::dim, s.size());
        dim_t t = filldim(s.size(), s.end(), View::dim.end());
        store = storage_traits<Store>::create(t);
        View::p = storage_traits<Store>::data(store);
    }
    template <class Pbegin> void fill1(dim_t xsize, Pbegin xbegin)
    {
        CHECK_BOUNDS(this->size()==xsize && "mismatched sizes");
        std::copy_n(xbegin, xsize, this->begin()); // TODO Use xpr traversal.
    }

    template <class SS> WithStorage(SS const & s, unspecified_t) { init(s); }
    WithStorage(std::initializer_list<dim_t> const s, unspecified_t) { init(s); }
    template <class SS, class XX> WithStorage(SS const & s, XX && x): WithStorage(s, unspecified)
    {
        static_cast<View &>(*this) = x;
    }
    template <class XX> WithStorage(std::initializer_list<dim_t> const s, XX && x): WithStorage(s, unspecified)
    {
        static_cast<View &>(*this) = x;
    }
    template <class XX> WithStorage(XX && x): WithStorage(start(x).shape(), unspecified)
    {
        static_cast<View &>(*this) = x;
    }
// FIXME use of fill1 requires T to be copiable, this is unfortunate as it conflicts with the semantics of View::operator=.
// store(x) avoids it for Big, but doesn't work for Unique. Should construct in place as std::vector does.
    WithStorage(std::initializer_list<T> const x): WithStorage({dim_t(x.size())}, unspecified)
    {
        static_assert(RANK==RANK_ANY || RANK==1, "rank mismatch with init list (of rank 1)");
        fill1(x.size(), x.begin());
    }
// TODO Replace these rank 1 -> rank N constructors by explicit reshape. See also small.H.
    WithStorage(std::initializer_list<dim_t> const s, std::initializer_list<T> const x): WithStorage(s, unspecified)
    {
        fill1(x.size(), x.begin());
    }
    template <class SS> WithStorage(SS const & s, std::initializer_list<T> const x): WithStorage(s, unspecified)
    {
        fill1(x.size(), x.begin());
    }
    template <class TT>
    WithStorage(std::initializer_list<dim_t> const s, TT * p): WithStorage(s, unspecified)
    {
        fill1(this->size(), p);
    }
    template <class P>
    WithStorage(std::initializer_list<dim_t> const s, P pbegin, P pend): WithStorage(s, unspecified)
    {
        fill1(this->size(), pbegin);
    }
    using View::operator=;

// only for some kinds of store.
    void resize(dim_t const s)
    {
        assert(this->rank()>0);
        store.resize(proddim(View::dim.begin()+1, View::dim.end())*s /* std::unspecified_t someday... */);
        View::dim[0].size = s;
        View::p = store.data();
    }
    void resize(dim_t const s, T const & t)
    {
        assert(this->rank()>0);
        store.resize(proddim(View::dim.begin()+1, View::dim.end())*s, t);
        View::dim[0].size = s;
        View::p = store.data();
    }
// lets us move. A template + std::forward wouldn't work for push_back(brace-enclosed-list).
    void push_back(T && t)
    {
        assert(this->rank()==1);
        store.push_back(std::move(t));
        ++View::dim[0].size;
        View::p = store.data();
    }
    void push_back(T const & t)
    {
        assert(this->rank()==1);
        store.push_back(t);
        ++View::dim[0].size;
        View::p = store.data();
    }
    template <class ... A>
    void emplace_back(A && ... a)
    {
        assert(this->rank()==1);
        store.emplace_back(std::forward<A>(a) ...);
        ++View::dim[0].size;
        View::p = store.data();
    }
    void pop_back()
    {
        assert(this->rank()==1 && View::dim[0].size>0);
        store.pop_back();
        --View::dim[0].size;
    }
    bool empty() const
    {
        return this->size()==0;
    }
    T const & back() const { assert(this->rank()==1 && "bad rank for back"); return store[this->size()-1]; }
    T & back() { assert(this->rank()==1 && "bad rank for back"); return store[this->size()-1]; }

// WithStorage is always compact/row-major. Then the 0-rank STL-like iterators can be raw pointers. TODO But .iter() should also be able to benefit from this constraint, and the check should be faster for some cases (like RANK==1) or elidable.

    auto begin() { assert(is_c_order(*this)); return this->data(); }
    auto begin() const { assert(is_c_order(*this)); return this->data(); }
    auto end() { return this->data()+this->size(); }
    auto end() const { return this->data()+this->size(); }
};

template <class Store, rank_t RANK>
void swap(WithStorage<Store, RANK> & a, WithStorage<Store, RANK> & b)
{
    std::swap(a.dim, b.dim);
    std::swap(a.store, b.store);
    std::swap(a.p, b.p);
}

// Beyond this, we probably should have fixed-size (~std::dynarray), resizeable (~std::vector).
template <class T, rank_t RANK=RANK_ANY> using Big = WithStorage<std::vector<T>, RANK>;
template <class T, rank_t RANK=RANK_ANY> using Unique = WithStorage<std::unique_ptr<T []>, RANK>;
template <class T, rank_t RANK=RANK_ANY> using Shared = WithStorage<std::shared_ptr<T>, RANK>;

// -------------
// Used in the Guile wrappers to allow an array parameter to either borrow from Guile
// storage or convert into a new array (e.g. passing 'f32 into 'f64).
// TODO Can use unique_ptr's deleter for this?
// TODO Shared/Unique should maybe have constructors with unique_ptr/shared_ptr args
// -------------

struct NullDeleter { template <class T> void operator()(T * p) {} };
struct Deleter { template <class T> void operator()(T * p) { delete[] p; } };

template <rank_t RANK, class T>
Shared<T, RANK> shared_borrowing(View<T, RANK> & raw)
{
    Shared<T, RANK> a;
    a.dim = raw.dim;
    a.p = raw.p;
    a.store = std::shared_ptr<T>(raw.data(), NullDeleter());
    return a;
}

template <class T>
struct View<T, RANK_ANY>
{
    using Dimv = std::vector<Dim>; // maybe use Unique<Dim, 1> here.

    Dimv dim;
    T * p;

    constexpr rank_t rank() const { return rank_t(dim.size()); }
    constexpr static rank_t rank_s() { return RANK_ANY; }
    constexpr dim_t size() const { return proddim(dim.begin(), dim.end()); }
    constexpr static dim_t size_s() { return DIM_ANY; }
    constexpr dim_t size(rank_t const j) const { return dim[j].size; }
    constexpr dim_t stride(rank_t const j) const { return dim[j].stride; }

    constexpr auto data() { return p; }
    constexpr auto data() const { return p; }

// Contrary to RANK!=RANK_ANY, the scalar case cannot be separated at compile time. So operator() will return a rank 0 view in that case.
#define SUBSCRIPTS(CONST)                                               \
    template <class ... I, std::enable_if_t<(is_beatable<I>::value && ...), int> = 0> \
    auto operator()(I const & ... i) CONST                              \
    {                                                                   \
        constexpr rank_t extended = (0 + ... + (is_beatable<I>::skip-is_beatable<I>::skip_src)); \
        assert(this->rank()+extended>=0);                               \
        View<T CONST, RANK_ANY> sub;                                    \
        sub.dim.resize(this->rank()+extended);                          \
        sub.p = data() + select_loop(sub.dim.data(), this->dim.data(), i ...); \
        for (int i=(0 + ... + is_beatable<I>::skip); i<sub.rank(); ++i) { \
            sub.dim[i] = this->dim[i-extended];                         \
        }                                                               \
        return sub;                                                     \
    }                                                                   \
    /* TODO More than one selector... */                               \
    template <class ... I, std::enable_if_t<!(is_beatable<I>::value && ...), int> = 0> \
    auto operator()(I && ... i) CONST                                   \
    {                                                                   \
        return from(*this, std::forward<I>(i) ...);                     \
    }                                                                   \
    template <class I>                                                  \
    auto at(I const & i) CONST                                          \
    {                                                                   \
        return View<T CONST, RANK_ANY> { Dimv(dim.begin()+i.size(), dim.end()), \
                data() + Indexer1::index_p(dim, i) };                   \
    }                                                                   \
    T CONST & operator[](dim_t const i) CONST                           \
    {                                                                   \
        CHECK_BOUNDS(rank()==1 && "bad rank");                          \
        CHECK_BOUNDS(inside(i, dim[0].size));                           \
        return data()[i*dim[0].stride];                                 \
    }
    SUBSCRIPTS(const)
    SUBSCRIPTS(/*const*/)
#undef SUBSCRIPTS

    DEF_ITERATORS(RANK_ANY)
    DEF_ASSIGNMENT_OPS
    template <rank_t SRANK> View(View<T, SRANK> const & x): dim(x.dim.begin(), x.dim.end()), p(x.p) {}

    operator T & () { assert(rank()==0); return data()[0]; };
    operator T const & () const { assert(rank()==0); return data()[0]; };
    template <rank_t R> operator View<T, R> ()
    {
        return View<T, R>(typename View<T, R>::Dimv(dim), data());
    }
    template <rank_t R> operator View<T const, R> () const
    {
        return View<T const, R>(typename View<T const, R>::Dimv(dim), data());
    }
};

#undef DEF_ITERATORS
#undef DEF_ASSIGNMENT_OPS
#undef DEF_ASSIGNOPS

template <class T>
struct ra_traits_def<View<T, RANK_ANY>>
{
    using V = View<T, RANK_ANY>;
    using value_type = T;
    using shape_type = std::vector<dim_t>; // TODO should be taken from v.iter().shape()

    static auto shape(V const & v) { return v.iter().shape(); }
    static dim_t size(V const & v) { return v.size(); }
    static rank_t rank(V const & v) { return v.rank(); }
    constexpr static rank_t rank_s() { return RANK_ANY; };
    constexpr static rank_t size_s() { return DIM_ANY; }
};

template <class Store, rank_t RANK>
struct ra_traits_def<WithStorage<Store, RANK>>
    : public ra_traits_def<View<typename WithStorage<Store, RANK>::T, RANK>> {};

} // namespace ra::

#undef CHECK_BOUNDS
#undef RA_CHECK_BOUNDS_BIG