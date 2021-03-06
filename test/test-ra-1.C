
// (c) Daniel Llorens - 2013-2015

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

/// @file test-ra-1.C
/// @brief Fundamental tests for ra:: ported from old test-traversal.C.

#include <iostream>
#include <iterator>
#include <numeric>
#include "ra/complex.H"
#include "ra/test.H"
#include "ra/big.H"
#include "ra/operators.H"
#include "ra/io.H"

using std::cout; using std::endl; using std::flush;

template <int i> using TI = ra::TensorIndex<i, int>;
using A2 = ra::Unique<int, 2>;
using A1 = ra::Unique<int, 1>;
using int3 = ra::Small<int, 3>;
using int2 = ra::Small<int, 2>;
using std_int3 = std::array<int, 3>;
using std_int2 = std::array<int, 2>;

template <class AA>
void CheckPlyReverse1(TestRecorder & tr, AA && a)
{
    std::iota(a.begin(), a.end(), 1);
    auto invert = [](int & a) { a = -a; return a; };
    ply_ravel(ra::expr(invert, a.iter()));
    for (int i=0; i<6; ++i) {
        tr.test_eq(-(i+1), a(i));
    }
    auto b = reverse(a, 0);
    ply_ravel(ra::expr(invert, b.iter()));
    for (int i=0; i<6; ++i) {
        tr.test_eq(6-i, b(i));
        tr.test_eq(i+1, a(i));
    }
}

template <class CC, class AA, class BB>
void CheckPly(TestRecorder & tr, char const * tag, AA && A, BB && B)
{
// need to slice because B may be Unique (!) and I have left own-type constructors as default on purpose. Here, I need C's contents to be a fresh copy of B's.
    CC C(B());
    auto sub = [](int & b, int const a) -> int { return b -= a; };
    ra::ply_ravel(ra::expr(sub, B.iter(), A.iter()));
    for (int i=0; i!=A.size(0); ++i) {
        for (int j=0; j!=A.size(1); ++j) {
            tr.info(tag, " ravel").test_eq(C(i, j)-A(i, j), B(i, j));
        }
    }
    auto add = [](int & b, int const a) -> int { return b += a; };
    ra::ply_ravel(ra::expr(add, B.iter(), A.iter()));
    ra::ply_index(ra::expr(sub, B.iter(), A.iter()));
    for (int i=0; i!=A.size(0); ++i) {
        for (int j=0; j!=A.size(1); ++j) {
            tr.info(tag, " index").test_eq(C(i, j)-A(i, j), B(i, j));
        }
    }
}

using complex = std::complex<double>;

int main()
{
    TestRecorder tr(std::cout);
    tr.section("nested, with references, ply_index or ply_ravel");
    {
        int check[3] = {0, 2, 4};
        ra::Small<int, 3> A {1, 0, -1};
        ra::Small<int, 3> B {1, 2, 3};
#define TEST(plier)                                                     \
        [&tr, &A, &B, check](auto && C)                                 \
        {                                                               \
            std::fill(C.begin(), C.end(), -99);                         \
            plier(ra::expr([](int & k, int const i) { k = -i; },        \
                           C.iter(),                                    \
                           ra::expr([](int const i, int const j) { return i-j; }, \
                                    A.iter(), B.iter())));              \
            tr.test(std::equal(check, check+3, C.begin()));             \
        }
#define TEST2(plier)                                            \
        TEST(plier)(ra::Small<int, 3> {});                      \
        TEST(plier)(ra::Unique<int, 1>({3}, ra::unspecified));
        TEST2(ply_ravel)
        TEST2(ply_index)
        TEST2(plyf)
        TEST2(plyf_index)
#undef TEST2
#undef TEST
            }
    tr.section("with ref terms only");
    {
#define TEST(plier, Biter, Citer)                                       \
        [&tr](auto && B, auto && C)                                     \
        {                                                               \
            plier(ra::expr([](int & k, int const i, int const j) { k = i+j; return k; }, \
                           Citer, Biter, Biter));                       \
            tr.test_eq(2, C[0]);                                        \
            tr.test_eq(4, C[1]);                                        \
            tr.test_eq(6, C[2]);                                        \
        }
#define TEST2(plier)                                                    \
        TEST(plier, B.iter(), C.iter())(int3 { 1, 2, 3 }, int3 { 77, 88, 99 }); \
        TEST(plier, ra::vector(B), ra::vector(C))(std_int3 {{ 1, 2, 3 }}, std_int3 {{ 77, 88, 99 }});
        TEST2(ply_ravel)
            TEST2(ply_index)
            TEST2(plyf)
            TEST2(plyf_index)
#undef TEST2
#undef TEST
            }
    tr.section("with ref & value terms");
    {
#define TEST(plier, Biter, Citer, Btemp)                                \
        [&tr](auto && B, auto && C)                                     \
        {                                                               \
            plier(ra::expr([](int & k, int const i, int const j) { k = i*j; return k; }, \
                           Citer, Btemp, Biter));                       \
            tr.test_eq(1, C[0]);                                        \
            tr.test_eq(4, C[1]);                                        \
            tr.test_eq(9, C[2]);                                        \
        }
        TEST(ply_ravel, B.iter(), C.iter(), (int3 {1, 2, 3}.iter()))(int3 { 1, 2, 3 }, int3 { 77, 88, 99 });
        TEST(ply_index, B.iter(), C.iter(), (int3 {1, 2, 3}.iter()))(int3 { 1, 2, 3 }, int3 { 77, 88, 99 });
        TEST(ply_ravel, ra::vector(B), ra::vector(C), ra::vector(std_int3 {{1, 2, 3}}))
            (std_int3 {{ 1, 2, 3 }}, std_int3 {{ 77, 88, 99 }});
        TEST(ply_index, ra::vector(B), ra::vector(C), ra::vector(std_int3 {{1, 2, 3}}))
            (std_int3 {{ 1, 2, 3 }}, std_int3 {{ 77, 88, 99 }});
        TEST(ply_index, ra::vector(B), ra::vector(C), ra::start({1, 2, 3}))(int3 { 1, 2, 3 }, int3 { 77, 88, 99 });
#undef TEST
    }
// Missing operators with int2; so not the same as in test-traversal.C.
    tr.section("complex or nested types");
    {
        int i;
        using A2of2 = ra::Unique<int2, 2>;
        auto sum2 = [](int2 const i, int2 const j, int2 & x) { x = { i[0]+j[0], i[1]+j[1] }; };
        A2of2 A({2, 3}, { {1,1}, {2,2}, {3,3}, {4,4}, {5,5}, {6,6} });
        ply_index(ra::expr([](int2 & a, int i, int j) { int k = i*3+j; a = {k, k}; },
                           A.iter(), TI<0>(), TI<1>()));
        A2of2 B({2, 3}, ra::scalar(int2 {0, 0}));
        cout << "A: " << A << endl;
        cout << "B: " << B << endl;

        cout << "\ntraverse_index..." << endl;
        ply_ravel(ra::expr([](int2 & b) { b = {0, 0}; }, B.iter()));
        ply_index(ra::expr(sum2, A.iter(), ra::scalar(int2{2, 2}), B.iter()));
        cout << B << endl;
        i=2; for (int2 & b: B) { tr.test_eq(i, b[0]); tr.test_eq(i, b[1]); ++i; }

        ply_ravel(ra::expr([](int2 & b) { b = {0, 0}; }, B.iter()));
        ply_index(ra::expr(sum2, ra::scalar(int2{3, 3}), A.iter(), B.iter()));
        cout << B << endl;
        i=3; for (int2 & b: B) { tr.test_eq(i, b[0]); tr.test_eq(i, b[1]); ++i; }

        cout << "\ntraverse..." << endl;
        ply_ravel(ra::expr([](int2 & b) { b = {0, 0}; }, B.iter()));
        ply_ravel(ra::expr(sum2, A.iter(), ra::scalar(int2{4, 5}), B.iter()));
        cout << B << endl;
        i=4; for (int2 & b: B) { tr.test_eq(i, b[0]); tr.test_eq(i+1, b[1]); ++i; }

        ply_ravel(ra::expr([](int2 & b) { b = {0, 0}; }, B.iter()));
        ply_ravel(ra::expr(sum2, ra::scalar(int2{5, 5}), A.iter(), B.iter()));
        cout << B << endl;
        i=5; for (int2 & b: B) { tr.test_eq(i, b[0]); tr.test_eq(i, b[1]); ++i; }
    }
    tr.section("reversed arrays, ply_index");
    {
        ra::Unique<int, 1> A({ 6 }, ra::unspecified);
        std::iota(A.begin(), A.end(), 1);
        ra::Unique<int, 1> B { {6}, ra::scalar(99) };
        auto copy = [](int & b, int const a) { b = a; return b; };
        ply_index(ra::expr(copy, B.iter(), A.iter()));
        for (int i=0; i<6; ++i) {
            tr.test_eq(i+1, B(i));
        }
        ply_index(ra::expr(copy, B.iter(), reverse(A, 0).iter()));
        for (int i=0; i<6; ++i) {
            tr.test_eq(6-i, B(i));
        }
    }
    tr.section("reversed arrays, traverse, only one");
    {
        CheckPlyReverse1(tr, ra::Unique<int, 1>({ 6 }, ra::unspecified));
        CheckPlyReverse1(tr, ra::Unique<int>({ 6 }, ra::unspecified));
    }
    tr.section("mismatched strides");
    {
        auto sum2 = [](int a, int b, int & c) { return c = a-b; };
        A2 a = A2({2, 3}, ra::unspecified); std::iota(a.begin(), a.end(), 1);
        A2 b = A2({3, 2}, ra::unspecified); std::iota(b.begin(), b.end(), 1);
        A2 c = A2({2, 3}, ra::unspecified);
        int check[6] = {0, -1, -2, 2, 1, 0};
#define TEST(plier)                                                     \
        {                                                               \
            std::fill(c.begin(), c.end(), 0);                           \
            plier(ra::expr(sum2, a.iter(), transpose<1, 0>(b).iter(), c.iter())); \
            tr.info(STRINGIZE(plier)).test(std::equal(check, check+6, c.begin())); \
        }                                                               \
        {                                                               \
            std::fill(c.begin(), c.end(), 0);                           \
            plier(ra::expr(sum2, transpose<1, 0>(a).iter(), b.iter(), transpose<1, 0>(c).iter())); \
            tr.info(STRINGIZE(plier)).test(std::equal(check, check+6, c.begin())); \
        }
        TEST(ply_ravel);
        TEST(ply_index);
        TEST(plyf);
        TEST(plyf_index);
#undef TEST
    }
    tr.section("reverse 1/1 axis, traverse");
#define TEST(plier)                                             \
    {                                                           \
        A1 a({ 6 }, ra::unspecified);                           \
        std::iota(a.begin(), a.end(), 1);                       \
        A1 b { {6}, ra::scalar(99) };                           \
        auto copy = [](int & b, int const a) { b = a; };        \
        plier(ra::expr(copy, b.iter(), a.iter()));              \
        cout << flush;                                          \
        for (int i=0; i<6; ++i) {                               \
            tr.test_eq(i+1, b[i]);                              \
        }                                                       \
        plier(ra::expr(copy, b.iter(), reverse(a, 0).iter()));  \
        for (int i=0; i<6; ++i) {                               \
            tr.test_eq(6-i, b(i));                              \
        }                                                       \
    }
    TEST(ply_index)
    TEST(ply_ravel)
    TEST(plyf)
    TEST(plyf_index)
#undef TEST
    tr.section("reverse (ref & non ref), traverse");
    {
        A2 A({2, 3}, { 1, 2, 3, 4, 5, 6 });
        A2 B({2, 3}, { 1, 2, 3, 4, 5, 6 });

        CheckPly<A2>(tr, "(a)", A, B);
        CheckPly<A2>(tr, "(b)", reverse(A, 0), B);
        CheckPly<A2>(tr, "(c)", A, reverse(B, 0));
        CheckPly<A2>(tr, "(d)", reverse(A, 0), reverse(B, 0));

        CheckPly<A2>(tr, "(e)", reverse(A, 1), B);
        CheckPly<A2>(tr, "(f)", A, reverse(B, 1));
        CheckPly<A2>(tr, "(g)", reverse(A, 1), reverse(B, 1));

// When BOTH strides are negative, B is still compact and this can be reduced to a single loop.
// TODO Enforce that the loop is linearized over both dimensions.

        CheckPly<A2>(tr, "(h)", A, reverse(reverse(B, 0), 1));
        CheckPly<A2>(tr, "(i)", reverse(reverse(A, 0), 1), B);
        CheckPly<A2>(tr, "(j)", reverse(reverse(A, 0), 1), reverse(reverse(B, 0), 1));
    }
    tr.section("reverse & transpose (ref & non ref), traverse");
    {
        using A2 = ra::Unique<int, 2>;
        A2 A({2, 2}, { 1, 2, 3, 4 });
        A2 B({2, 2}, { 1, 2, 3, 4 });

        CheckPly<A2>(tr, "(a)", transpose({1, 0}, A), B);
        CheckPly<A2>(tr, "(b)", A, transpose({1, 0}, B));
        CheckPly<A2>(tr, "(c)", reverse(reverse(transpose({1, 0}, A), 1), 0), B);
        CheckPly<A2>(tr, "(d)", A, reverse(reverse(transpose({1, 0}, B), 1), 0));

        CheckPly<A2>(tr, "(e)", transpose<1, 0>(A), B);
        CheckPly<A2>(tr, "(f)", A, transpose<1, 0>(B));
        CheckPly<A2>(tr, "(g)", reverse(reverse(transpose<1, 0>(A), 1), 0), B);
        CheckPly<A2>(tr, "(h)", A, reverse(reverse(transpose<1, 0>(B), 1), 0));

        CheckPly<A2>(tr, "(i)", transpose(mp::int_list<1, 0>(), A), B);
        CheckPly<A2>(tr, "(j)", A, transpose(mp::int_list<1, 0>(), B));
        CheckPly<A2>(tr, "(k)", reverse(reverse(transpose(mp::int_list<1, 0>(), A), 1), 0), B);
        CheckPly<A2>(tr, "(l)", A, reverse(reverse(transpose(mp::int_list<1, 0>(), B), 1), 0));
    }
    return tr.summary();
}
