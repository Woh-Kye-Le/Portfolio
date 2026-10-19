// ============================================================================
// main.cpp
//   "Can I beat the compiler?" - benchmark driver.
//
//   Defines the C++ reference implementations and times them against the
//   hand-written x64 assembly in funcs.asm. Build Release / x64: the C++ side
//   is only optimized there, and the comparison is only fair with /O2.
// ============================================================================

#include "funcs.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// C++ reference implementations (the "compiler" side)
// ---------------------------------------------------------------------------

std::size_t cpp_strlen(const char* s) {
    std::size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

std::int64_t cpp_sum_array(const std::int64_t* arr, std::size_t n) {
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < n; ++i) sum += arr[i];
    return sum;
}

std::uint64_t cpp_popcount_array(const std::uint64_t* arr, std::size_t n) {
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < n; ++i) total += std::popcount(arr[i]);
    return total;
}

void cpp_reverse_array(std::int64_t* arr, std::size_t n) {
    for (std::size_t i = 0; i < n / 2; ++i) std::swap(arr[i], arr[n - 1 - i]);
}

void cpp_grayscale(const std::uint8_t* src, std::uint8_t* dst, std::size_t pixel_count) {
    for (std::size_t i = 0; i < pixel_count; ++i) {
        const std::uint8_t* px = src + i * 4;
        const unsigned r = px[0];
        const unsigned g = px[1];
        const unsigned b = px[2];
        dst[i] = static_cast<std::uint8_t>((77u * r + 150u * g + 29u * b) >> 8);
    }
}

// ---------------------------------------------------------------------------
// Timing harness
// ---------------------------------------------------------------------------
namespace {

using Clock = std::chrono::steady_clock;

// Consumes every result so the optimizer cannot delete the measured work.
volatile std::uint64_t g_sink = 0;

// Times op() and returns the average nanoseconds per call. The call count is
// calibrated from one sample so every workload runs for roughly the same wall
// time, which keeps short and long routines comparable.
template <typename Op>
double measure(Op&& op) {
    constexpr double kBudgetNs = 2.5e8; // ~250 ms per measurement

    op();               // warm up caches / branch predictors
    g_sink += op();

    const auto t0 = Clock::now();
    g_sink += op();
    const auto t1 = Clock::now();

    const double sample = std::max(
        1.0, std::chrono::duration<double, std::nano>(t1 - t0).count());
    const long long iters = static_cast<long long>(
        std::clamp(kBudgetNs / sample, 1.0, 2.0e7));

    const auto s0 = Clock::now();
    for (long long i = 0; i < iters; ++i) g_sink += op();
    const auto s1 = Clock::now();

    const double total = std::chrono::duration<double, std::nano>(s1 - s0).count();
    return total / static_cast<double>(iters);
}

void report(const char* name, double cpp_ns, double asm_ns) {
    const bool asm_wins = asm_ns < cpp_ns;
    const double factor = asm_wins ? cpp_ns / asm_ns : asm_ns / cpp_ns;
    std::printf("%-20s %13.1f %13.1f   %-4s %5.2fx\n",
                name, cpp_ns, asm_ns, asm_wins ? "asm" : "cpp", factor);
}

} // namespace

int main() {
    std::printf("==========================================================\n");
    std::printf(" Can I beat the compiler?  x64 asm vs optimized C++ (/O2)\n");
    std::printf("==========================================================\n\n");

    if (!run_tests()) {
        std::printf("\nCorrectness tests FAILED - skipping the benchmark.\n");
        return 1;
    }

    std::printf("\n%-20s %13s %13s   %-4s %6s\n",
                "Benchmark", "C++ (ns/op)", "ASM (ns/op)", "win", "factor");
    std::printf("--------------------------------------------------------------------\n");

    std::mt19937_64 rng(0xC0FFEEu);

    // strlen over a 1 MiB string --------------------------------------------
    {
        std::string s(1u << 20, 'a');
        const char* volatile p = s.c_str();
        const double c = measure([&] { return cpp_strlen(p); });
        const double a = measure([&] { return asm_strlen(p); });
        report("strlen (1 MiB)", c, a);
    }

    // sum of 1M int64 --------------------------------------------------------
    {
        std::vector<std::int64_t> v(1u << 20);
        for (auto& x : v) x = static_cast<std::int64_t>(rng());
        const std::int64_t* volatile p = v.data();
        const std::size_t volatile n = v.size();
        const double c = measure([&] { return static_cast<std::uint64_t>(cpp_sum_array(p, n)); });
        const double a = measure([&] { return static_cast<std::uint64_t>(asm_sum_array(p, n)); });
        report("sum i64 (1M)", c, a);
    }

    // popcount over 1M uint64 ------------------------------------------------
    {
        std::vector<std::uint64_t> v(1u << 20);
        for (auto& x : v) x = rng();
        const std::uint64_t* volatile p = v.data();
        const std::size_t volatile n = v.size();
        const double c = measure([&] { return cpp_popcount_array(p, n); });
        const double a = measure([&] { return asm_popcount_array(p, n); });
        report("popcount u64 (1M)", c, a);
    }

    // reverse 1M int64 -------------------------------------------------------
    {
        std::vector<std::int64_t> v(1u << 20);
        for (auto& x : v) x = static_cast<std::int64_t>(rng());
        std::int64_t* volatile p = v.data();
        const std::size_t volatile n = v.size();
        const double c = measure([&] { cpp_reverse_array(p, n); return std::uint64_t(0); });
        const double a = measure([&] { asm_reverse_array(p, n); return std::uint64_t(0); });
        report("reverse i64 (1M)", c, a);
    }

    // RGBA8 -> gray8 on a 1080p image ---------------------------------------
    {
        constexpr std::size_t kWidth = 1920, kHeight = 1080;
        const std::size_t pixels = kWidth * kHeight;
        std::vector<std::uint8_t> src(pixels * 4);
        for (auto& b : src) b = static_cast<std::uint8_t>(rng());
        std::vector<std::uint8_t> dst_cpp(pixels), dst_asm(pixels);

        const std::uint8_t* volatile sp = src.data();
        std::uint8_t* volatile dc = dst_cpp.data();
        std::uint8_t* volatile da = dst_asm.data();
        const std::size_t volatile n = pixels;

        const double c = measure([&] { cpp_grayscale(sp, dc, n); return std::uint64_t(0); });
        const double a = measure([&] { asm_grayscale(sp, da, n); return std::uint64_t(0); });
        report("grayscale (1080p)", c, a);
    }

    std::printf("--------------------------------------------------------------------\n");
    std::printf("checksum %llu (consumed so nothing is optimised away)\n",
                static_cast<unsigned long long>(g_sink));
    return 0;
}
