#!/bin/bash
set -uo pipefail

set -x

# Get to the directory the script is in
cd "$(dirname "$0")"

export BASE_CFLAGS='-ansi -Wno-long-long -Wall -Wextra -pedantic -Wc++-compat -ggdb3 -flto -Wshift-overflow -fsanitize=address,undefined,pointer-compare,pointer-subtract,leak -fsanitize-address-use-after-scope -include sanitizer/lsan_interface.h -fno-common -fno-omit-frame-pointer -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC -D_FORTIFY_SOURCE=3 -O3 -flto=auto'
export ASAN_OPTIONS=check_initialization_order=1:detect_stack_use_after_return=1:max_malloc_fill_size=524288:max_free_fill_size=524288:print_scariness=1:windows_hook_rtl_allocators=1:handle_ioctl=1:allocator_may_return_null=1:strict_string_checks=1:fast_unwind_on_malloc=0:strict_init_order=1:detect_invalid_pointer_pairs=0:detect_write_exec=1:alloc_dealloc_mismatch=true:use_odr_indicator=true:detect_leaks=1:abort_on_error=1
export LSAN_OPTIONS=use_unaligned=1:abort_on_error=1

# Add OpenSSL
export LDFLAGS="$BASE_CFLAGS -Wl,--whole-archive -lcrypto -Wl,--no-whole-archive"

do_honggfuzz_build() {
    export CC=hfuzz-cc
    export CXX=hfuzz-c++
    export CFLAGS="$BASE_CFLAGS -DFUZZER -fsanitize=fuzzer"

    cmake -B build/honggfuzz && cmake --build build/honggfuzz --parallel 3
    # ctest --test-dir build/honggfuzz --parallel 14
}

do_libfuzzer_build() {
    export CC=clang
    export CXX=clang++
    export CFLAGS="$BASE_CFLAGS -DFUZZER -DLIBFUZZER -fsanitize=fuzzer-no-link"
    export LDFLAGS="$LDFLAGS -L/usr/lib/clang/21/lib/x86_64-redhat-linux-gnu/ -Wl,--whole-archive -lclang_rt.fuzzer_no_main -Wl,--no-whole-archive"

    cmake -B build/libfuzzer && cmake --build build/libfuzzer --parallel 3
}

do_normal_build() {
    export CC=cc
    export CXX=c++
    export CFLAGS="$BASE_CFLAGS"

    cmake -B build/normal -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build/normal --parallel 3 && ctest --test-dir build/normal --parallel 14
}

do_debug_build() {
    export CC=afl-cc
    export CXX=afl-c++
    export CFLAGS="$BASE_CFLAGS -O0"

    cmake -B build/debug && cmake --build build/debug --parallel 3
}

do_afl_build() {
    export CC=afl-cc
    export CXX=afl-c++
    export CFLAGS="$BASE_CFLAGS -DFUZZER -DAFL"
    export AFL_USE_ASAN=1
    export AFL_USE_UBSAN=1
    export AFL_USE_LSAN=1

    cmake -B build/afl && cmake --build build/afl --parallel 3
}


FUZZ_BUILDS=(
    do_honggfuzz_build
    do_libfuzzer_build
    do_normal_build
    do_debug_build
    do_afl_build
)

export -f "${FUZZ_BUILDS[@]}"

make DEBUG=1 COUNTEREXAMPLES=1 generators || exit
printf '%s\n' "${FUZZ_BUILDS[@]}" | parallel --line-buffer --tagstring '[{#}/{%}]' --progress '{}'
