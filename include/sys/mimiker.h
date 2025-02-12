#ifndef _SYS_MIMIKER_H_
#define _SYS_MIMIKER_H_

/* Common definitions that may be used only in kernel source tree. */

#ifndef _KERNEL
#error "<sys/mimiker.h> may be used only inside kernel source tree!"
#endif

#include <limits.h>    /* UINT_MAX, LONG_MIN, ... */
#include <stdbool.h>   /* bool, true, false */
#include <stdalign.h>  /* alignof, alignas */
#include <stdatomic.h> /* atomic_{load,store,fetch_*,...} */
#include <inttypes.h>  /* PRIdN, PRIxPTR, ... */
#include <sys/param.h>
#include <sys/types.h>

#define log2(x) (CHAR_BIT * sizeof(unsigned long) - __builtin_clzl(x) - 1)
#define ffs(x) ((u_long)__builtin_ffsl(x))
#define clz(x) ((u_long)__builtin_clzl(x))
#define ctz(x) ((u_long)__builtin_ctzl(x))

#define abs(x)                                                                 \
  ({                                                                           \
    typeof(x) _x = (x);                                                        \
    (_x < 0) ? -_x : _x;                                                       \
  })

#define min(a, b)                                                              \
  ({                                                                           \
    typeof(a) _a = (a);                                                        \
    typeof(b) _b = (b);                                                        \
    _a < _b ? _a : _b;                                                         \
  })

#define max(a, b)                                                              \
  ({                                                                           \
    typeof(a) _a = (a);                                                        \
    typeof(b) _b = (b);                                                        \
    _a > _b ? _a : _b;                                                         \
  })

#define swap(a, b)                                                             \
  ({                                                                           \
    typeof(a) _a = (a);                                                        \
    typeof(a) _b = (b);                                                        \
    (a) = _b;                                                                  \
    (b) = _a;                                                                  \
  })

/* Aligns the address to given size (must be power of 2) */
#define align(addr, size)                                                      \
  ({                                                                           \
    intptr_t _addr = (intptr_t)(addr);                                         \
    intptr_t _size = (intptr_t)(size);                                         \
    _addr = (_addr + (_size - 1)) & -_size;                                    \
    (typeof(addr))_addr;                                                       \
  })

#define is_aligned(addr, size)                                                 \
  ({                                                                           \
    intptr_t _addr = (intptr_t)(addr);                                         \
    intptr_t _size = (intptr_t)(size);                                         \
    !(_addr & (_size - 1));                                                    \
  })

#define container_of(p, type, field)                                           \
  ((type *)((char *)(p)-offsetof(type, field)))

/* Checks often used in assert statements. */
bool preempt_disabled(void);
bool intr_disabled(void) __no_profile;

#define CLEANUP_FUNCTION(func) __CONCAT(__cleanup_, func)
#define DEFINE_CLEANUP_FUNCTION(type, func)                                    \
  static inline void __cleanup_##func(type *ptr) {                             \
    if (*ptr)                                                                  \
      func(*ptr);                                                              \
  }                                                                            \
  struct __force_semicolon__

#define SCOPED_STMT(TYP, ACQUIRE, RELEASE, VAL, ...)                           \
  __unused TYP *__UNIQUE(__scoped) __cleanup(RELEASE) = ({                     \
    ACQUIRE(VAL, ##__VA_ARGS__);                                               \
    VAL;                                                                       \
  })

#define WITH_STMT(TYP, ACQUIRE, RELEASE, VAL, ...)                             \
  for (SCOPED_STMT(TYP, ACQUIRE, RELEASE, VAL, ##__VA_ARGS__),                 \
       *__UNIQUE(__loop) = (TYP *)1;                                           \
       __UNIQUE(__loop); __UNIQUE(__loop) = NULL)

int copystr(const void *restrict kfaddr, void *restrict kdaddr, size_t len,
            size_t *restrict lencopied) __nonnull(1)
  __nonnull(2) __no_sanitize __no_instrument_function;
int copyinstr(const void *restrict udaddr, void *restrict kaddr, size_t len,
              size_t *restrict lencopied) __nonnull(1)
  __nonnull(2) __no_sanitize;
int copyin(const void *restrict udaddr, void *restrict kaddr, size_t len)
  __nonnull(1) __nonnull(2) __no_sanitize;
int copyout(const void *restrict kaddr, void *restrict udaddr, size_t len)
  __nonnull(1) __nonnull(2) __no_sanitize;

#define copyin_s(udaddr, _what) copyin((udaddr), &(_what), sizeof(_what))
#define copyout_s(_what, udaddr) copyout(&(_what), (udaddr), sizeof(_what))

/* Global definitions used throught kernel. */
__noreturn void kernel_init(void);

/*! \brief Called during kernel initialization. */
void init_clock(void);

/* Initial range of virtual addresses used by kernel image. */
extern char __kernel_start[];
extern char __kernel_end[];
/* TODO(cahir) Should be exposed only when _MACHDEP or KGPROF. */
extern char __text[];
extern char __etext[];

#ifdef _MACHDEP
/* Symbols defined by linker and used during kernel boot phase. */
extern char __boot[];
extern char __eboot[];
extern char __rodata[];
extern char __data[];
extern char __bss[];
extern char __ebss[];
#endif /* !_MACHDEP */

#endif /* !_SYS_MIMIKER_H_ */
