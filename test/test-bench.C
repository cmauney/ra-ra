
// (c) Daniel Llorens - 2017

// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

/// @file bench.H
/// @brief Test the benchmarking microlibrary.

#include <string>
#include <iostream>
#include <iomanip>
#include <limits>
#include <chrono>
#include <thread>
#include "ra/operators.H"
#include "ra/io.H"
#include "ra/test.H"
#include "ra/bench.H"

using std::cout, std::endl;

int main()
{
    TestRecorder tr;
    tr.section("straight");
    {
        auto f = [](auto && a, auto && b)
                 {
                     std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                     return a+b;
                 };
        auto b = Benchmark {/* repeats */ 100, /* runs */ 10};

        auto vala = b.run(f, 1, 2);
        cout << "empty: " << vala.empty() << endl;
        b.report(std::cout, vala, 1e-9);

        auto valb = b.run(f, ra::iota(3), ra::iota(10, 3));
        b.report(std::cout, valb, 1e-9);
    }
    tr.section("fixture");
    {
        auto g = [](auto && repeat, auto && a, auto && b)
                 {
                     /* do stuff */
                     repeat([&]() { std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                             return a+b; });
                     /* do stuff */
                 };
        auto b = Benchmark {/* repeats */ 100, /* runs */ 10};

        auto vala = b.run_f(g, 1, 2);
        cout << "empty: " << vala.empty() << endl;
        b.report(std::cout, vala, 1e-9);

        auto valb = b.run_f(g, ra::iota(3), ra::iota(10, 3));
        b.report(std::cout, valb, 1e-9);
    }
    return tr.summary();
};
