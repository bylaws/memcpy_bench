#include <benchmark/benchmark.h>
#include <cstring>
#include "asm.h"

#  define DECLSPEC_ALIGN(x) __attribute__((aligned(x)))

static void BM_memcpy(benchmark::State& state) {
  char* src = new char[state.range(0)]; char* dst = new char[state.range(0)];
  memset(src, 'x', state.range(0));
  for (auto _ : state)
    memcpy(dst, src, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] src; delete[] dst;
}

void *  memcpy_ntdll( void *dst, const void *src, size_t n )
{
    volatile unsigned char *d = (volatile unsigned char *)dst;  /* avoid gcc optimizations */
    const unsigned char *s = (const unsigned char *)src;

    if ((size_t)dst - (size_t)src >= n)
    {
        while (n--) *d++ = *s++;
    }
    else
    {
        d += n - 1;
        s += n - 1;
        while (n--) *d-- = *s--;
    }
    return dst;
}



static void BM_memcpy_ntdll(benchmark::State& state) {
  char* src = new char[state.range(0)]; char* dst = new char[state.range(0)];
  memset(src, 'x', state.range(0));
  for (auto _ : state)
    memcpy_ntdll(dst, src, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] src; delete[] dst;
}



void *  memcpy_ntdll_novol( void *dst, const void *src, size_t n )
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if ((size_t)dst - (size_t)src >= n)
    {
        while (n--) *d++ = *s++;
    }
    else
    {
        d += n - 1;
        s += n - 1;
        while (n--) *d-- = *s--;
    }
    return dst;
}



static void BM_memcpy_ntdll_novol(benchmark::State& state) {
  char* src = new char[state.range(0)]; char* dst = new char[state.range(0)];
  memset(src, 'x', state.range(0));
  for (auto _ : state)
    memcpy_ntdll_novol(dst, src, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] src; delete[] dst;
}


#ifdef WORDS_BIGENDIAN
# define MERGE(w1, sh1, w2, sh2) ((w1 << sh1) | (w2 >> sh2))
#else
# define MERGE(w1, sh1, w2, sh2) ((w1 >> sh1) | (w2 << sh2))
#endif
void *  memcpy_msvcrt(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    int sh1;

    if (!n) return dst;

    if ((size_t)dst - (size_t)src >= n)
    {
        for (; (size_t)d % sizeof(size_t) && n; n--) *d++ = *s++;

        sh1 = 8 * ((size_t)s % sizeof(size_t));
        if (!sh1)
        {
            while (n >= sizeof(size_t))
            {
                *(size_t*)d = *(size_t*)s;
                s += sizeof(size_t);
                d += sizeof(size_t);
                n -= sizeof(size_t);
            }
        }
        else if (n >= 2 * sizeof(size_t))
        {
            int sh2 = 8 * sizeof(size_t) - sh1;
            size_t x, y;

            s -= sh1 / 8;
            x = *(size_t*)s;
            do
            {
                s += sizeof(size_t);
                y = *(size_t*)s;
                *(size_t*)d = MERGE(x, sh1, y, sh2);
                d += sizeof(size_t);

                s += sizeof(size_t);
                x = *(size_t*)s;
                *(size_t*)d = MERGE(y, sh1, x, sh2);
                d += sizeof(size_t);

                n -= 2 * sizeof(size_t);
            } while (n >= 2 * sizeof(size_t));
            s += sh1 / 8;
        }
        while (n--) *d++ = *s++;
        return dst;
    }
    else
    {
        d += n;
        s += n;

        for (; (size_t)d % sizeof(size_t) && n; n--) *--d = *--s;

        sh1 = 8 * ((size_t)s % sizeof(size_t));
        if (!sh1)
        {
            while (n >= sizeof(size_t))
            {
                s -= sizeof(size_t);
                d -= sizeof(size_t);
                *(size_t*)d = *(size_t*)s;
                n -= sizeof(size_t);
            }
        }
        else if (n >= 2 * sizeof(size_t))
        {
            int sh2 = 8 * sizeof(size_t) - sh1;
            size_t x, y;

            s -= sh1 / 8;
            x = *(size_t*)s;
            do
            {
                s -= sizeof(size_t);
                y = *(size_t*)s;
                d -= sizeof(size_t);
                *(size_t*)d = MERGE(y, sh1, x, sh2);

                s -= sizeof(size_t);
                x = *(size_t*)s;
                d -= sizeof(size_t);
                *(size_t*)d = MERGE(x, sh1, y, sh2);

                n -= 2 * sizeof(size_t);
            } while (n >= 2 * sizeof(size_t));
            s += sh1 / 8;
        }
        while (n--) *--d = *--s;
    }
    return dst;
}
#undef MERGE

static void BM_memcpy_msvcrt(benchmark::State& state) {
  char* src = new char[state.range(0)]; char* dst = new char[state.range(0)];
  memset(src, 'x', state.range(0));
  for (auto _ : state)
    memcpy_msvcrt(dst, src, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] src; delete[] dst;
}

#ifdef __x86_64__
#define DEST_REG "%rdi"
#define SRC_REG "%rsi"
#define LEN_REG "%r8"
#define TMP_REG "%r9"

#define MEMMOVE_INIT \
    "pushq " SRC_REG "\n\t" \
    __ASM_SEH(".seh_pushreg " SRC_REG "\n\t") \
    __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t") \
    "pushq " DEST_REG "\n\t" \
    __ASM_SEH(".seh_pushreg " DEST_REG "\n\t") \
    __ASM_SEH(".seh_endprologue\n\t") \
    __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t") \
    "movq %rcx, " DEST_REG "\n\t" \
    "movq %rdx, " SRC_REG "\n\t"

#define MEMMOVE_CLEANUP \
    "movq %rcx, %rax\n\t" \
    "popq " DEST_REG "\n\t" \
    __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t") \
    "popq " SRC_REG "\n\t" \
    __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")

void *  sse2_memmove(void *dst, const void *src, size_t n);
__ASM_GLOBAL_FUNC( sse2_memmove,
        MEMMOVE_INIT
        "mov " DEST_REG ", " TMP_REG "\n\t" /* check copying direction */
        "sub " SRC_REG ", " TMP_REG "\n\t"
        "cmp " LEN_REG ", " TMP_REG "\n\t"
        "jb copy_bwd\n\t"
        /* copy forwards */
        "cmp $4, " LEN_REG "\n\t" /* 4-bytes align */
        "jb copy_fwd3\n\t"
        "mov " DEST_REG ", " TMP_REG "\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "movsb\n\t"
        "dec " LEN_REG "\n\t"
        "inc " TMP_REG "\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "movsw\n\t"
        "sub $2, " LEN_REG "\n\t"
        "inc " TMP_REG "\n\t"
        "1:\n\t" /* 16-bytes align */
        "cmp $16, " LEN_REG "\n\t"
        "jb copy_fwd15\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "movsl\n\t"
        "sub $4, " LEN_REG "\n\t"
        "inc " TMP_REG "\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "movsl\n\t"
        "movsl\n\t"
        "sub $8, " LEN_REG "\n\t"
        "1:\n\t"
        "cmp $64, " LEN_REG "\n\t"
        "jb copy_fwd63\n\t"
        "1:\n\t" /* copy 64-bytes blocks in loop, dest 16-bytes aligned */
        "movdqu 0x00(" SRC_REG "), %xmm0\n\t"
        "movdqu 0x10(" SRC_REG "), %xmm1\n\t"
        "movdqu 0x20(" SRC_REG "), %xmm2\n\t"
        "movdqu 0x30(" SRC_REG "), %xmm3\n\t"
        "movdqa %xmm0, 0x00(" DEST_REG ")\n\t"
        "movdqa %xmm1, 0x10(" DEST_REG ")\n\t"
        "movdqa %xmm2, 0x20(" DEST_REG ")\n\t"
        "movdqa %xmm3, 0x30(" DEST_REG ")\n\t"
        "add $64, " SRC_REG "\n\t"
        "add $64, " DEST_REG "\n\t"
        "sub $64, " LEN_REG "\n\t"
        "cmp $64, " LEN_REG "\n\t"
        "jae 1b\n\t"
        "copy_fwd63:\n\t" /* copy last 63 bytes, dest 16-bytes aligned */
        "mov " LEN_REG ", " TMP_REG "\n\t"
        "and $15, " LEN_REG "\n\t"
        "shr $5, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "movdqu 0(" SRC_REG "), %xmm0\n\t"
        "movdqa %xmm0, 0(" DEST_REG ")\n\t"
        "add $16, " SRC_REG "\n\t"
        "add $16, " DEST_REG "\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc copy_fwd15\n\t"
        "movdqu 0x00(" SRC_REG "), %xmm0\n\t"
        "movdqu 0x10(" SRC_REG "), %xmm1\n\t"
        "movdqa %xmm0, 0x00(" DEST_REG ")\n\t"
        "movdqa %xmm1, 0x10(" DEST_REG ")\n\t"
        "add $32, " SRC_REG "\n\t"
        "add $32, " DEST_REG "\n\t"
        "copy_fwd15:\n\t" /* copy last 15 bytes, dest 4-bytes aligned */
        "mov " LEN_REG ", " TMP_REG "\n\t"
        "and $3, " LEN_REG "\n\t"
        "shr $3, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "movsl\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc copy_fwd3\n\t"
        "movsl\n\t"
        "movsl\n\t"
        "copy_fwd3:\n\t" /* copy last 3 bytes */
        "shr $1, " LEN_REG "\n\t"
        "jnc 1f\n\t"
        "movsb\n\t"
        "1:\n\t"
        "shr $1, " LEN_REG "\n\t"
        "jnc 1f\n\t"
        "movsw\n\t"
        "1:\n\t"
        MEMMOVE_CLEANUP
        "ret\n\t"
        "copy_bwd:\n\t"
        "lea (" DEST_REG ", " LEN_REG "), " DEST_REG "\n\t"
        "lea (" SRC_REG ", " LEN_REG "), " SRC_REG "\n\t"
        "cmp $4, " LEN_REG "\n\t" /* 4-bytes align */
        "jb copy_bwd3\n\t"
        "mov " DEST_REG ", " TMP_REG "\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "dec " SRC_REG "\n\t"
        "dec " DEST_REG "\n\t"
        "movb (" SRC_REG "), %al\n\t"
        "movb %al, (" DEST_REG ")\n\t"
        "dec " LEN_REG "\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "sub $2, " SRC_REG "\n\t"
        "sub $2, " DEST_REG "\n\t"
        "movw (" SRC_REG "), %ax\n\t"
        "movw %ax, (" DEST_REG ")\n\t"
        "sub $2, " LEN_REG "\n\t"
        "1:\n\t" /* 16-bytes align */
        "cmp $16, " LEN_REG "\n\t"
        "jb copy_bwd15\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "sub $4, " SRC_REG "\n\t"
        "sub $4, " DEST_REG "\n\t"
        "movl (" SRC_REG "), %eax\n\t"
        "movl %eax, (" DEST_REG ")\n\t"
        "sub $4, " LEN_REG "\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "sub $8, " SRC_REG "\n\t"
        "sub $8, " DEST_REG "\n\t"
        "movl 4(" SRC_REG "), %eax\n\t"
        "movl %eax, 4(" DEST_REG ")\n\t"
        "movl (" SRC_REG "), %eax\n\t"
        "movl %eax, (" DEST_REG ")\n\t"
        "sub $8, " LEN_REG "\n\t"
        "1:\n\t"
        "cmp $64, " LEN_REG "\n\t"
        "jb copy_bwd63\n\t"
        "1:\n\t" /* copy 64-bytes blocks in loop, dest 16-bytes aligned */
        "sub $64, " SRC_REG "\n\t"
        "sub $64, " DEST_REG "\n\t"
        "movdqu 0x00(" SRC_REG "), %xmm0\n\t"
        "movdqu 0x10(" SRC_REG "), %xmm1\n\t"
        "movdqu 0x20(" SRC_REG "), %xmm2\n\t"
        "movdqu 0x30(" SRC_REG "), %xmm3\n\t"
        "movdqa %xmm0, 0x00(" DEST_REG ")\n\t"
        "movdqa %xmm1, 0x10(" DEST_REG ")\n\t"
        "movdqa %xmm2, 0x20(" DEST_REG ")\n\t"
        "movdqa %xmm3, 0x30(" DEST_REG ")\n\t"
        "sub $64, " LEN_REG "\n\t"
        "cmp $64, " LEN_REG "\n\t"
        "jae 1b\n\t"
        "copy_bwd63:\n\t" /* copy last 63 bytes, dest 16-bytes aligned */
        "mov " LEN_REG ", " TMP_REG "\n\t"
        "and $15, " LEN_REG "\n\t"
        "shr $5, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "sub $16, " SRC_REG "\n\t"
        "sub $16, " DEST_REG "\n\t"
        "movdqu (" SRC_REG "), %xmm0\n\t"
        "movdqa %xmm0, (" DEST_REG ")\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc copy_bwd15\n\t"
        "sub $32, " SRC_REG "\n\t"
        "sub $32, " DEST_REG "\n\t"
        "movdqu 0x00(" SRC_REG "), %xmm0\n\t"
        "movdqu 0x10(" SRC_REG "), %xmm1\n\t"
        "movdqa %xmm0, 0x00(" DEST_REG ")\n\t"
        "movdqa %xmm1, 0x10(" DEST_REG ")\n\t"
        "copy_bwd15:\n\t" /* copy last 15 bytes, dest 4-bytes aligned */
        "mov " LEN_REG ", " TMP_REG "\n\t"
        "and $3, " LEN_REG "\n\t"
        "shr $3, " TMP_REG "\n\t"
        "jnc 1f\n\t"
        "sub $4, " SRC_REG "\n\t"
        "sub $4, " DEST_REG "\n\t"
        "movl (" SRC_REG "), %eax\n\t"
        "movl %eax, (" DEST_REG ")\n\t"
        "1:\n\t"
        "shr $1, " TMP_REG "\n\t"
        "jnc copy_bwd3\n\t"
        "sub $8, " SRC_REG "\n\t"
        "sub $8, " DEST_REG "\n\t"
        "movl 4(" SRC_REG "), %eax\n\t"
        "movl %eax, 4(" DEST_REG ")\n\t"
        "movl (" SRC_REG "), %eax\n\t"
        "movl %eax, (" DEST_REG ")\n\t"
        "copy_bwd3:\n\t" /* copy last 3 bytes */
        "shr $1, " LEN_REG "\n\t"
        "jnc 1f\n\t"
        "dec " SRC_REG "\n\t"
        "dec " DEST_REG "\n\t"
        "movb (" SRC_REG "), %al\n\t"
        "movb %al, (" DEST_REG ")\n\t"
        "1:\n\t"
        "shr $1, " LEN_REG "\n\t"
        "jnc 1f\n\t"
        "movw -2(" SRC_REG "), %ax\n\t"
        "movw %ax, -2(" DEST_REG ")\n\t"
        "1:\n\t"
        MEMMOVE_CLEANUP
        "ret" )

static void BM_memcpy_msvcrt_sse2(benchmark::State& state) {
  char* src = new char[state.range(0)]; char* dst = new char[state.range(0)];
  memset(src, 'x', state.range(0));
  for (auto _ : state)
    sse2_memmove(dst, src, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] src; delete[] dst;
}
BENCHMARK(BM_memcpy_msvcrt_sse2)->Range(8, 8<<10);

#endif

static void BM_memset(benchmark::State& state) {
  char* buf = new char[state.range(0)];
  memset(buf, 'x', state.range(0));
  unsigned int i = 0;
  for (auto _ : state)
    memset(buf, (i++)&0xff, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] buf;
}


static inline void memset_ntdll_aligned_32( unsigned char *d, uint64_t v, size_t n )
{
    unsigned char *end = d + n;
    while (d < end)
    {
        *(uint64_t *)(d + 0) = v;
        *(uint64_t *)(d + 8) = v;
        *(uint64_t *)(d + 16) = v;
        *(uint64_t *)(d + 24) = v;
        d += 32;
    }
}

/*********************************************************************
 *                  memset   (NTDLL.@)
 */
void *memset_ntdll( void *dst, int c, size_t n )
{
    typedef uint64_t DECLSPEC_ALIGN(1) unaligned_ui64;
    typedef uint32_t DECLSPEC_ALIGN(1) unaligned_ui32;
    typedef uint16_t DECLSPEC_ALIGN(1) unaligned_ui16;

    uint64_t v = 0x101010101010101ull * (unsigned char)c;
    unsigned char *d = (unsigned char *)dst;
    size_t a = 0x20 - ((uintptr_t)d & 0x1f);

    if (n >= 16)
    {
        *(unaligned_ui64 *)(d + 0) = v;
        *(unaligned_ui64 *)(d + 8) = v;
        *(unaligned_ui64 *)(d + n - 16) = v;
        *(unaligned_ui64 *)(d + n - 8) = v;
        if (n <= 32) return dst;
        *(unaligned_ui64 *)(d + 16) = v;
        *(unaligned_ui64 *)(d + 24) = v;
        *(unaligned_ui64 *)(d + n - 32) = v;
        *(unaligned_ui64 *)(d + n - 24) = v;
        if (n <= 64) return dst;

        n = (n - a) & ~0x1f;
        memset_ntdll_aligned_32( d + a, v, n );
        return dst;
    }
    if (n >= 8)
    {
        *(unaligned_ui64 *)d = v;
        *(unaligned_ui64 *)(d + n - 8) = v;
        return dst;
    }
    if (n >= 4)
    {
        *(unaligned_ui32 *)d = v;
        *(unaligned_ui32 *)(d + n - 4) = v;
        return dst;
    }
    if (n >= 2)
    {
        *(unaligned_ui16 *)d = v;
        *(unaligned_ui16 *)(d + n - 2) = v;
        return dst;
    }
    if (n >= 1)
    {
        *(uint8_t *)d = v;
        return dst;
    }
    return dst;
}


static void BM_memset_ntdll(benchmark::State& state) {
  char* buf = new char[state.range(0)];
  memset(buf, 'x', state.range(0));
  unsigned int i = 0;
  for (auto _ : state)
    memset_ntdll(buf, (i++)&0xff, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] buf;
}

void *memset_basic( void *dst, int c, size_t n )
{
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < n; i++)
	    d[i]=c;

    return dst;
}

static void BM_memset_basic(benchmark::State& state) {
  char* buf = new char[state.range(0)];
  memset(buf, 'x', state.range(0));
  unsigned int i = 0;
  for (auto _ : state)
    memset_basic(buf, (i++)&0xff, state.range(0));
  state.SetBytesProcessed(state.iterations() * state.range(0));
  delete[] buf;
}

BENCHMARK(BM_memset_basic)->Range(8, 8<<10);
BENCHMARK(BM_memcpy)->Range(8, 8<<10);
BENCHMARK(BM_memset)->Range(8, 8<<10);
BENCHMARK(BM_memcpy_ntdll)->Range(8, 8<<10);
BENCHMARK(BM_memset_ntdll)->Range(8, 8<<10);
BENCHMARK(BM_memcpy_ntdll_novol)->Range(8, 8<<10);
BENCHMARK(BM_memcpy_msvcrt)->Range(8, 8<<10);

BENCHMARK_MAIN();
