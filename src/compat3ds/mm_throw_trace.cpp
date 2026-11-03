// MM 3DS: unordered_map::at/vector::at throw via std::__throw_out_of_range*.
// The terminate backtrace only shows the handler, so we log the caller
// before throwing. Resolve the address with arm-none-eabi-addr2line -e mm_3ds.
#include <cstdio>

extern "C" {
[[noreturn]] void __real__ZSt20__throw_out_of_rangePKc(const char* what);
[[noreturn]] void __real__ZSt24__throw_out_of_range_fmtPKcz(const char* fmt, ...);

[[noreturn]] void __wrap__ZSt20__throw_out_of_rangePKc(const char* what) {
    fprintf(stderr, "mm-3ds: out_of_range \"%s\" caller=%p\n", what, __builtin_return_address(0));
    __real__ZSt20__throw_out_of_rangePKc(what);
}

[[noreturn]] void __wrap__ZSt24__throw_out_of_range_fmtPKcz(const char* fmt, ...) {
    fprintf(stderr, "mm-3ds: out_of_range_fmt \"%s\" caller=%p\n", fmt, __builtin_return_address(0));
    __real__ZSt20__throw_out_of_rangePKc(fmt);
}
}
