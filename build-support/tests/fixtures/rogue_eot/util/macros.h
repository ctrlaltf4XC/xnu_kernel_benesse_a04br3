/* Host-only dependency shim; no instruction fields or encodings. */
#include <stdint.h>
#include <assert.h>
#define UNREACHABLE(x) __builtin_trap()
