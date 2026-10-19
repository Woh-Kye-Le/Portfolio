#pragma once

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Assembly implementations - defined in funcs.asm (x64 MASM).
// extern "C" keeps the symbol names undecorated so MASM and MSVC agree.
// ---------------------------------------------------------------------------
extern "C" {
std::size_t   asm_strlen(const char* s);
std::int64_t  asm_sum_array(const std::int64_t* arr, std::size_t n);
std::uint64_t asm_popcount_array(const std::uint64_t* arr, std::size_t n);
void          asm_reverse_array(std::int64_t* arr, std::size_t n);
void          asm_grayscale(const std::uint8_t* src, std::uint8_t* dst, std::size_t pixel_count);
}

// ---------------------------------------------------------------------------
// C++ reference implementations - defined in main.cpp. These are the
// "compiler" side of the benchmark and the ground truth for tests.cpp.
// ---------------------------------------------------------------------------
std::size_t   cpp_strlen(const char* s);
std::int64_t  cpp_sum_array(const std::int64_t* arr, std::size_t n);
std::uint64_t cpp_popcount_array(const std::uint64_t* arr, std::size_t n);
void          cpp_reverse_array(std::int64_t* arr, std::size_t n);
void          cpp_grayscale(const std::uint8_t* src, std::uint8_t* dst, std::size_t pixel_count);

// Defined in tests.cpp: returns true when every asm result matches C++.
bool run_tests();
