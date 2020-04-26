// This file is part of PGM-index <https://github.com/gvinciguerra/PGM-index>.
// Copyright (c) 2019 Giorgio Vinciguerra.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "pgm_index.hpp"
#include "pgm_index_compressed.hpp"
#include <type_traits>

TEMPLATE_TEST_CASE("Segmentation algorithm", "", float, double, uint32_t, uint64_t) {
    const auto error = GENERATE(32, 64, 128);
    std::vector<TestType> data(1000000);
    std::random_device rnd_device;
    std::mt19937 engine{rnd_device()};
    using RandomFunction = std::function<TestType()>;

    if constexpr (std::is_floating_point<TestType>()) {
        RandomFunction lognormal = std::bind(std::lognormal_distribution<TestType>(0, 0.5), engine);
        RandomFunction exponential = std::bind(std::exponential_distribution<TestType>(1.2), engine);
        auto rand = GENERATE_COPY(as<RandomFunction>{}, lognormal, exponential);
        std::generate(data.begin(), data.end(), rand);
    } else {
        RandomFunction uniform_dense = std::bind(std::uniform_int_distribution<TestType>(0, 10000), engine);
        RandomFunction uniform_sparse = std::bind(std::uniform_int_distribution<TestType>(0, 10000000), engine);
        RandomFunction binomial = std::bind(std::binomial_distribution<TestType>(50000), engine);
        RandomFunction geometric = std::bind(std::geometric_distribution<TestType>(0.8), engine);
        auto rand = GENERATE_COPY(as<RandomFunction>{}, uniform_dense, uniform_sparse, binomial, geometric);
        std::generate(data.begin(), data.end(), rand);
    }

    std::sort(data.begin(), data.end());
    auto segments = make_segmentation(data.begin(), data.end(), error);
    auto it = segments.begin();
    auto [slope, intercept] = it->get_floating_point_segment(it->get_first_x());

    for (auto i = 0; i < data.size(); ++i) {
        if (i != 0 && data[i] == data[i - 1])
            continue;
        if (std::next(it) != segments.end() && std::next(it)->get_first_x() <= data[i]) {
            ++it;
            std::tie(slope, intercept) = it->get_floating_point_segment(it->get_first_x());
        }

        auto pos = (data[i] - it->get_first_x()) * slope + intercept;
        auto e = std::fabs(i - pos);
        REQUIRE(e <= error + 1);
    }
}

TEMPLATE_TEST_CASE_SIG("PGM-index", "",
                       ((typename T, size_t E1, size_t E2), T, E1, E2),
                       (uint32_t, 16, 0), (uint32_t, 32, 0), (uint32_t, 64, 0),
                       (uint64_t, 16, 4), (uint64_t, 32, 4), (uint64_t, 64, 4),
                       (uint64_t, 4, 16), (uint64_t, 4, 32), (uint64_t, 4, 64)) {
    std::vector<T> data(2000000);
    std::random_device rnd_device;
    std::mt19937 engine{rnd_device()};

    using RandomFunction = std::function<T()>;
    RandomFunction uniform_dense = std::bind(std::uniform_int_distribution<T>(0, 10000), engine);
    RandomFunction uniform_sparse = std::bind(std::uniform_int_distribution<T>(0, 10000000), engine);
    RandomFunction binomial = std::bind(std::binomial_distribution<T>(50000), engine);
    RandomFunction geometric = std::bind(std::geometric_distribution<T>(0.8), engine);
    auto rand = GENERATE_COPY(as<RandomFunction>{}, uniform_dense, uniform_sparse, binomial, geometric);

    std::generate(data.begin(), data.end(), rand);
    std::sort(data.begin(), data.end());
    PGMIndex<T, E1, E2> pgm_index(data);

    for (auto i = 1; i <= 10000; ++i) {
        auto q = data[std::rand() % data.size()];
        auto approx_range = pgm_index.find_approximate_position(q);
        auto lo = data.begin() + approx_range.lo;
        auto hi = data.begin() + approx_range.hi;
        auto k = std::lower_bound(lo, hi, q);
        REQUIRE(*k == q);
    }

    // Test elements outside range
    auto q = data.back() + 42;
    auto approx_range = pgm_index.find_approximate_position(q);
    auto lo = data.begin() + approx_range.lo;
    auto hi = data.begin() + approx_range.hi;
    if (approx_range.hi != 2000000)
        std::cout << data.back() << ", " << approx_range.lo << ", " << approx_range.hi << std::endl;
    REQUIRE(std::lower_bound(lo, hi, q) == data.end());

    q = 0;
    approx_range = pgm_index.find_approximate_position(q);
    lo = data.begin() + approx_range.lo;
    hi = data.begin() + approx_range.hi;
    REQUIRE(std::lower_bound(lo, hi, q) == data.begin());
}

TEST_CASE("Compressed PGM-index") {
    std::vector<uint32_t> data(1000000);
    std::generate(data.begin(), data.end(), [] { return std::rand() % 10000; });
    std::sort(data.begin(), data.end());
    CompressedPGMIndex<uint32_t, 32, 32> compressed_pgm_index(data);

    for (auto i = 1; i <= 1000; ++i) {
        auto q = data[std::rand() % data.size()];
        auto approx_range = compressed_pgm_index.find_approximate_position(q);
        auto lo = data.cbegin() + approx_range.lo;
        auto hi = data.cbegin() + approx_range.hi;
        auto k = std::lower_bound(lo, hi, q);
        REQUIRE(*k == q);
    }
}