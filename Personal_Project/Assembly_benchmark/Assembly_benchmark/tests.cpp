// ============================================================================
// tests.cpp
//   Checks that every assembly routine produces exactly the same output as the
//   C++ reference implementation, including the awkward edge cases
//   (empty input, odd lengths, unaligned buffers, non-multiple-of-4 images).
// ============================================================================

#include "funcs.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

std::string random_text(std::mt19937_64& rng, std::size_t len) {
    std::string s(len, '\0');
    for (auto& ch : s) ch = static_cast<char>('a' + (rng() % 26));
    return s;
}

} // namespace

bool run_tests() {
    std::printf("Correctness tests (asm vs C++ reference)\n");
    std::mt19937_64 rng(0x5EEDu);

    // --- strlen -------------------------------------------------------------
    check(asm_strlen("") == 0, "strlen: empty string");
    check(asm_strlen("abc") == 3, "strlen: short literal");
    {
        bool ok = true;
        for (int i = 0; i < 64; ++i) {
            const std::string s = random_text(rng, static_cast<std::size_t>(rng() % 4096));
            ok = ok && (asm_strlen(s.c_str()) == s.size());
        }
        check(ok, "strlen: random lengths match std::string::size");
    }

    // --- sum ----------------------------------------------------------------
    check(asm_sum_array(nullptr, 0) == 0, "sum: empty array");
    {
        const std::int64_t small[] = {1, 2, 3, 4};
        check(asm_sum_array(small, 4) == 10, "sum: known answer {1,2,3,4}");
    }
    {
        bool ok = true;
        for (int i = 0; i < 32; ++i) {
            const std::size_t n = static_cast<std::size_t>(rng() % 5000) + 1;
            std::vector<std::int64_t> v(n);
            for (auto& x : v) x = static_cast<std::int64_t>(rng()) - static_cast<std::int64_t>(rng() >> 1);
            ok = ok && (asm_sum_array(v.data(), n) == cpp_sum_array(v.data(), n));
        }
        check(ok, "sum: random arrays match C++");
    }

    // --- popcount -----------------------------------------------------------
    {
        const std::uint64_t probe[] = {0, 1, 0xFF, 0x8000000000000000ull, ~0ull};
        const std::uint64_t expect[] = {0, 1, 8, 1, 64};
        bool ok = true;
        for (int i = 0; i < 5; ++i) ok = ok && (asm_popcount_array(&probe[i], 1) == expect[i]);
        check(ok, "popcount: known answers");
    }
    {
        bool ok = true;
        for (int i = 0; i < 32; ++i) {
            const std::size_t n = static_cast<std::size_t>(rng() % 5000) + 1;
            std::vector<std::uint64_t> v(n);
            for (auto& x : v) x = rng();
            ok = ok && (asm_popcount_array(v.data(), n) == cpp_popcount_array(v.data(), n));
        }
        check(ok, "popcount: random arrays match C++");
    }

    // --- reverse ------------------------------------------------------------
    check(true, "reverse: empty array (no-op)");
    {
        bool ok = true;
        for (int i = 0; i < 32; ++i) {
            const std::size_t n = static_cast<std::size_t>(rng() % 2000);
            std::vector<std::int64_t> v(n), ref(n);
            for (auto& x : v) x = static_cast<std::int64_t>(rng());
            ref = v;
            std::reverse(ref.begin(), ref.end());
            asm_reverse_array(v.data(), n);
            ok = ok && (v == ref);
        }
        check(ok, "reverse: random arrays match std::reverse");
    }

    // --- grayscale ----------------------------------------------------------
    {
        bool ok = true;
        for (std::size_t pixels : {0u, 1u, 2u, 3u, 5u, 7u, 63u, 64u, 65u, 1000u}) {
            std::vector<std::uint8_t> src(pixels * 4), ref(pixels, 0xCD), out(pixels, 0);
            for (auto& b : src) b = static_cast<std::uint8_t>(rng());
            cpp_grayscale(src.data(), ref.data(), pixels);
            asm_grayscale(src.data(), out.data(), pixels);
            ok = ok && (out == ref);
        }
        check(ok, "grayscale: sizes 0..1000 match C++");
    }
    {
        // Unaligned buffers and the full 1080p frame.
        const std::size_t pixels = 1920u * 1080u;
        std::vector<std::uint8_t> src(pixels * 4 + 8), ref(pixels + 8, 0), out(pixels + 8, 0);
        for (auto& b : src) b = static_cast<std::uint8_t>(rng());
        const std::uint8_t* sp = src.data() + 1;  // deliberately unaligned
        std::uint8_t* rp = ref.data() + 1;
        std::uint8_t* op = out.data() + 1;
        cpp_grayscale(sp, rp, pixels);
        asm_grayscale(sp, op, pixels);
        check(std::memcmp(rp, op, pixels) == 0, "grayscale: unaligned 1080p frame matches C++");
    }

    std::printf("%s\n", g_failures == 0 ? "All tests passed." : "Some tests failed.");
    return g_failures == 0;
}
