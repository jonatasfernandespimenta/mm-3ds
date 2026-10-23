// MM 3DS: unordered_map::at/vector::at lançam via std::__throw_out_of_range*.
// O backtrace do terminate só mostra o handler, então registramos o chamador
// antes de lançar. Resolva o endereço com arm-none-eabi-addr2line -e mm_3ds.
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
