#include <cassert>
#include <cstddef>

// Newlib's assert prints via stdio and pulls the heap in - trap instead:
// HardFault, IWDG reset, warm recovery (I-3)
extern "C" void __assert_func(const char*, int, const char*, const char*) {
    __builtin_trap();
}

// libstdc++ hardening asserts (on at -O0 since GCC 15): keep the checks,
// drop their fprintf+abort machinery
// NOLINTBEGIN(cert-dcl58-cpp)
namespace std {
    void __glibcxx_assert_fail(const char*, int, const char*, const char*) noexcept {
        __builtin_trap();
    }
} // namespace std
// NOLINTEND(cert-dcl58-cpp)

// Deleting destructors reference operator delete, which drags in free.
// Never called (I-3) - trap keeps the images allocator-free
void operator delete(void*) noexcept {
    __builtin_trap();
}
void operator delete(void*, std::size_t) noexcept {
    __builtin_trap();
}
