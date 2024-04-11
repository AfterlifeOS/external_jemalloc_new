#ifndef JEMALLOC_INTERNAL_EXTERNS_H
#define JEMALLOC_INTERNAL_EXTERNS_H

#include "jemalloc/internal/arena_types.h"
#include "jemalloc/internal/atomic.h"
#include "jemalloc/internal/hpa_opts.h"
#include "jemalloc/internal/nstime.h"
#include "jemalloc/internal/sec_opts.h"
#include "jemalloc/internal/tsd_types.h"

/* TSD checks this to set thread local slow state accordingly. */
extern bool malloc_slow;

/* Run-time options. */
#define opt_abort 0
#define opt_abort_conf 0
#define opt_confirm_conf 0
#define opt_hpa 0
extern hpa_shard_opts_t opt_hpa_opts;
extern sec_opts_t opt_hpa_sec_opts;

extern const char *opt_junk;
#define opt_junk_alloc 0
#define opt_junk_free 0
extern void (*JET_MUTABLE junk_free_callback)(void *ptr, size_t size);
extern void (*JET_MUTABLE junk_alloc_callback)(void *ptr, size_t size);
extern void (*JET_MUTABLE invalid_conf_abort)(void);
#define opt_utrace 0
#define opt_xmalloc 0
#define opt_experimental_infallible_new 0
extern bool opt_zero;
extern unsigned opt_narenas;
#ifdef JEMALLOC_ZERO_REALLOC_DEFAULT_FREE
  #define opt_zero_realloc_action zero_realloc_action_free
#else
  #define opt_zero_realloc_action zero_realloc_action_alloc
#endif
extern malloc_init_t malloc_init_state;
extern const char *const zero_realloc_mode_names[];
extern atomic_zu_t zero_realloc_count;
#ifdef JEMALLOC_CACHE_OBLIVIOUS
  #define opt_cache_oblivious true
#else
  #define opt_cache_oblivious false
#endif
#define opt_debug_double_free_max_scan 0

/* Escape free-fastpath when ptr & mask == 0 (for sanitization purpose). */
extern uintptr_t san_cache_bin_nonfast_mask;

/* Number of CPUs. */
#if defined(__BIONIC__) && defined(ANDROID_NUM_ARENAS)
  /* Hardcode since this value won't be used. */
  #define ncpus 2
#else
  extern unsigned ncpus;
#endif

/* Number of arenas used for automatic multiplexing of threads and arenas. */
extern unsigned narenas_auto;

/* Base index for manual arenas. */
extern unsigned manual_arena_base;

/*
 * Arenas that are used to service external requests.  Not all elements of the
 * arenas array are necessarily used; arenas are created lazily as needed.
 */
extern atomic_p_t arenas[];

void *a0malloc(size_t size);
void a0dalloc(void *ptr);
void *bootstrap_malloc(size_t size);
void *bootstrap_calloc(size_t num, size_t size);
void bootstrap_free(void *ptr);
void arena_set(unsigned ind, arena_t *arena);
unsigned narenas_total_get(void);
arena_t *arena_init(tsdn_t *tsdn, unsigned ind, const arena_config_t *config);
arena_t *arena_choose_hard(tsd_t *tsd, bool internal);
void arena_migrate(tsd_t *tsd, arena_t *oldarena, arena_t *newarena);
void iarena_cleanup(tsd_t *tsd);
void arena_cleanup(tsd_t *tsd);
size_t batch_alloc(void **ptrs, size_t num, size_t size, int flags);
void jemalloc_prefork(void);
void jemalloc_postfork_parent(void);
void jemalloc_postfork_child(void);
void sdallocx_default(void *ptr, size_t size, int flags);
void free_default(void *ptr);
void *malloc_default(size_t size);

#endif /* JEMALLOC_INTERNAL_EXTERNS_H */
