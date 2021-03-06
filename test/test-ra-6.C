
// (c) Daniel Llorens - 2014-2015

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

// Regression test for a bug with > 2 non-beatable selectors. The bug was due to
// bad assumptions in ra::Iota::adv() and ra::Vector::adv().

#include "ra/operators.H"
#include "ra/io.H"
#include "ra/test.H"

using std::cout; using std::endl;

int main()
{
    int N = 4;
    ra::Big<float, 3> A({N, N, N}, ra::_2 + ra::_1*4 + ra::_0*16);
    ra::Big<float, 2> B({N, N}, ra::_1 + ra::_0*4);
    ra::Big<float, 1> C({N}, ra::_0);

    cout << "A: " << N << endl;
    cout << "B: " << N << endl;
    cout << "C: " << N << endl;

// beatable.
    ra::Iota<int> i(2, 1);
// making unbeatable on purpose, but still depends on internal Iota counter.
    auto j = ra::map([](int i) { return i; }, i);
// naturally unbeatable.
    ra::Small<int, 2> k { 1, 2 };
    auto l = std::array<int, 2> { 1, 2 };
    auto ll = ra::vector(l);

    cout << "X0: " << ra::from([](int i, int j, int k) { return ra::Small<int, 3>{i, j, k}; }, i, i, i) << endl;
    cout << "X1: " << ra::from([](int i, int j, int k) { return ra::Small<int, 3>{i, j, k}; }, j, j, j) << endl;
    cout << "X2: " << ra::from([](int i, int j, int k) { return ra::Small<int, 3>{i, j, k}; }, k, k, k) << endl;
    cout << "X3: " << ra::from([](int i, int j, int k) { return ra::Small<int, 3>{i, j, k}; }, ra::vector(l), ra::vector(l), ra::vector(l)) << endl;
    cout << "X4: " << ra::from([](int i, int j, int k) { return ra::Small<int, 3>{i, j, k}; }, ll, ll, ll) << endl;

    cout << endl;

    cout << "Y0: " << ra::from(A, i, i, i) << endl;
    cout << "Y1: " << ra::from(A, j, j, j) << endl;
    cout << "Y2: " << ra::from(A, k, k, k) << endl;
    cout << "Y3: " << ra::from(A, ra::vector(l), ra::vector(l), ra::vector(l)) << endl;
    cout << "Y4: " << ra::from(A, ll, ll, ll) << endl;

    cout << B(i, i) << endl;

    TestRecorder tr(std::cout);
    {
        ra::Small<int, 2, 2, 2> ref3 {21, 22, 25, 26, 37, 38, 41, 42};
        tr.test_eq(ref3, A(i, i, i));
        // tr.test_eq(ref3, A(j, j, j));
        tr.test_eq(ref3, A(k, k, k));
        tr.test_eq(ref3, A(ra::vector(l), ra::vector(l), ra::vector(l)));
        tr.test_eq(ref3, A(ll, ll, ll));

// TODO have a proper rank / shape match error when comparing these with ref3
        ra::Small<int, 2, 2> ref2 {5, 6, 9, 10};
        tr.test_eq(ref2, B(i, i));
        tr.test_eq(ref2, B(j, j));
        tr.test_eq(ref2, B(k, k));
        tr.test_eq(ref2, B(ra::vector(l), ra::vector(l)));
        tr.test_eq(ref2, B(ll, ll));

// TODO have a proper rank / shape match error when comparing these with ref3
        ra::Small<int, 2> ref1 {1, 2};
        tr.test_eq(ref1, C(i));
        tr.test_eq(ref1, C(j));
        tr.test_eq(ref1, C(k));
        tr.test_eq(ref1, C(ra::vector(l)));
        tr.test_eq(ref1, C(ll));
    }
    return tr.summary();
}
