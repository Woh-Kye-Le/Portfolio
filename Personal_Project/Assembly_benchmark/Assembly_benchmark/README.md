# Can I beat the compiler?

I'd just learned the basics of x64 assembly and wanted to try it on something
real, so I wrote five small functions by hand, called them from C++, and raced
them against the plain C++ versions. The main goal of this project is to just
find out *why* each one went the way it did.

Short version: the compiler won one, the functions that i had coded won two, and the other two were basically ties.
Here's the full story.

## What's in the repo

```
Assembly_benchmark/
├── Assembly_benchmark.slnx          # open this in Visual Studio
└── Assembly_benchmark/
    ├── main.cpp                     # C++ versions, the timing code, and main()
    ├── Funcs.asm                    # my assembly versions (MASM)
    ├── funcs.h                      # extern "C" declarations for both sides
    ├── tests.cpp                    # checks the asm gives the same answers as C++
    └── README.md                    
```

The five functions are `strlen`, summing an array, counting set bits
(popcount), reversing an array, and converting a 1080p RGBA image to grayscale.

## Running it

It's x64 only. The assembly follows the Microsoft x64 calling convention, so it
won't even assemble for Win32.

The easy way: open `Assembly_benchmark.slnx` in Visual Studio, switch to
**Release** and **x64** in the toolbar, and hit Ctrl+F5. From a developer
prompt you can also do:

```
msbuild Assembly_benchmark.slnx /p:Configuration=Release /p:Platform=x64
x64\Release\Assembly_benchmark.exe
```

Use release for accurate comparison 

## Results

My machine: AMD Ryzen 9 8945HS, Windows, MSVC 14.50 (Visual Studio 18), Release
build with `/O2 /GL /arch:AVX2`. The arrays are about 8 MiB (1M 64-bit
numbers), the string is 1 MiB, and the image is 1920x1080 RGBA.

| Benchmark                | C++ (ns/op) | asm (ns/op) | Winner | By how much |
| ------------------------ | ----------: | ----------: | :----: | ----------: |
| `strlen`, 1 MiB string   |     521,409 |     316,997 |  asm   |       1.64x |
| `sum`, 1M `int64`        |     104,728 |     345,894 |  C++   |       3.30x |
| `popcount`, 1M `uint64`  |     350,830 |     380,538 |  C++   |       1.08x |
| `reverse`, 1M `int64`    |     277,573 |     289,049 |  C++   |       1.04x |
| `grayscale`, 1080p RGBA  |   2,185,821 |     523,724 |  asm   |       4.17x |

"By how much" is just how much faster the winner was. Anything under about
1.1x I'm calling a tie (more on why below).

## What happened with each one

**`sum`: the compiler wins, and it isn't close (3.3x).**
This is the simplest loop there is: `sum += a[i]`. With AVX2 turned on, the
compiler turns it into vector loads and adds, so it handles several numbers at
once. It got through 8 MiB in about 105 µs, which works out to around 80 GB/s
and is far too fast for a loop that does one add at a time. That one-at-a-time
loop is exactly what I wrote (`add rax, [rcx]` in a loop, about 0.33 ns per
number). To catch up I'd have to hand-write the same AVX2 code the compiler
already produces, so there's not much to be proud of there.

**`grayscale`: I win by 4x, because the compiler didn't vectorize it.**
Each pixel is four bytes (R, G, B, A), and the loop needs to widen them,
multiply by weights, add, and shift. That awkward byte layout seems to put the
auto-vectorizer off: the C++ takes about 1.05 ns per pixel, which looks like
plain scalar code. My version uses SSE2 and does four pixels per loop. `pmaddwd`
computes the `77*R + 150*G + 29*B` part for two pixels at a time, a shuffle and
add fold the pairs together, and then a shift and two pack instructions squeeze
the result down to four gray bytes. That lands at about 0.25 ns per pixel. This
is the case where knowing SIMD actually paid off. (I haven't looked at the
compiler's disassembly yet, so "it stayed scalar" is my best reading of the
timing, not something I've confirmed.)

**`popcount`: a tie, because we both used the same instruction.**
`std::popcount` should compile down to the CPU's `popcnt` instruction, and
that's the one I used too. There's no faster instruction to reach for, so
there's nothing to win. The 8% gap is within the noise.

**`reverse`: also a tie.**
Both versions swap elements from the two ends moving inward. The whole thing is
mostly limited by how fast the CPU can load and store, not by which
instructions are used, so there's very little to gain.

**`strlen`: a win I don't fully trust.**
This is the one I can't explain yet. Both versions are a plain byte-at-a-time
loop, so I expected a tie. Instead mine came out 1.64x faster (on an earlier run
the gap was only 1.08x). My guess is that it comes down to how the compiler laid
out the loop compared to mine, but I haven't compared the disassembly, so that's
only a guess. The main thing I take from this one: assembly isn't automatically
fast. Real library `strlen` implementations use SIMD to check many bytes at a
time and would almost certainly beat both of ours. I didn't benchmark that.

## How the measuring works, and how far to trust it

- **Nothing gets optimized away.** Every result is added into a `volatile`
  variable, and the pointers are read through `volatile` too, so the compiler
  can't spot that the work is pointless and delete it. The checksum printed at
  the end is that variable.
- **Both sides do the same work.** They read the same buffers, and `tests.cpp`
  checks the asm output matches the C++ output first. That covers empty input,
  odd lengths, a 1080p frame on a deliberately unaligned buffer, and image sizes
  that aren't a multiple of four pixels (which exercises the leftover-pixels
  code in the asm).
- **Warm-up and calibration.** Each benchmark does an untimed call first, then
  times one call to work out how many repeats fit in about 250 ms, then runs
  that many.

One caveat: each number is a single ~250 ms window from a single run, not an
average over many runs. The exact numbers move around between runs (an earlier
run had `sum` at 4.2x and `strlen` at 1.08x), but the overall picture stayed
the same: the compiler wins `sum`, I win `grayscale`, and the other three are
close. Treat anything near 1.0x as a tie.

## What I took away from this

- **For a simple loop over memory, the compiler already writes the vector code
  you'd write.** Hand assembly mostly loses here.
- **For awkward data (interleaved bytes, widening multiplies, packing), the
  auto-vectorizer often gives up**, and hand-written SIMD can win by a lot.
- **If the CPU has an instruction that does exactly the job, or the loop is
  limited by memory, everyone ties.**

Writing this also made me get the C++/assembly boundary exactly right, which is
where most beginner bugs live: arguments arrive in RCX, RDX, R8, R9 and the
return value goes in RAX; XMM0-XMM5 are free to use but XMM6-XMM15 must be
preserved; every function here is a leaf that only touches volatile registers,
so it needs no stack frame; and `extern "C"` stops C++ name mangling from
breaking the link.

## What I'd do next

- Look at the compiler's disassembly for `strlen` and `grayscale` (Debug >
  Windows > Disassembly, or godbolt.org with `/O2 /arch:AVX2`) and replace my
  guesses above with facts.
- Fix my `sum`. Using four independent accumulators, or SSE2/AVX2 adds, should
  close most of that 3.3x gap.
- Run each benchmark several times and report the median.
