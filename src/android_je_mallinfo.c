/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

static size_t accumulate_large_allocs(arena_t* arena) {
  size_t total_bytes = 0;

  /* Accumulate the large allocation stats.
   * Do not include stats.allocated_large, it is only updated by
   * arena_stats_merge, and would include the data counted below.
   */
  LOCKEDINT_MTX_LOCK(TSDN_NULL, arena->stats.mtx);
  for (unsigned i = 0; i < SC_NSIZES - SC_NBINS; i++) {
    /* Read ndalloc first so that we guarantee nmalloc >= ndalloc. */
    uint64_t ndalloc = locked_read_u64(TSDN_NULL, LOCKEDINT_MTX(arena->stats.mtx), &arena->stats.lstats[i].ndalloc);
    uint64_t nmalloc = locked_read_u64(TSDN_NULL, LOCKEDINT_MTX(arena->stats.mtx), &arena->stats.lstats[i].nmalloc);
    size_t curlextents = (size_t)(nmalloc - ndalloc);
    total_bytes += sz_index2size(SC_NBINS + i) * curlextents;
  }
  LOCKEDINT_MTX_UNLOCK(TSDN_NULL, arena->stats.mtx);
  return total_bytes;
}

static size_t accumulate_small_allocs(arena_t* arena) {
  size_t total_bytes = 0;
  for (unsigned i = 0; i < SC_NBINS; i++) {
    for (unsigned j = 0; j < bin_infos[i].n_shards; j++) {
      bin_t* bin = arena_get_bin(arena, i, j);

      /* NOTE: This includes allocations cached on every thread. */
      malloc_mutex_lock(TSDN_NULL, &bin->lock);
      total_bytes += bin_infos[i].reg_size * bin->stats.curregs;
      malloc_mutex_unlock(TSDN_NULL, &bin->lock);
    }
  }
  return total_bytes;
}


/* Only use bin locks since the stats are now all atomic and can be read
 * without taking the stats lock.
 */
struct mallinfo je_mallinfo() {
  struct mallinfo mi = { 0 };

  malloc_mutex_lock(TSDN_NULL, &arenas_lock);
  for (unsigned i = 0; i < narenas_total_get(); i++) {
    arena_t* arena = atomic_load_p(&arenas[i], ATOMIC_ACQUIRE);
    if (arena != NULL) {
      mi.hblkhd += atomic_load_zu(&arena->pa_shard.pac.stats->pac_mapped, ATOMIC_ACQUIRE);

      mi.uordblks += accumulate_small_allocs(arena);
      mi.uordblks += accumulate_large_allocs(arena);
    }
  }
  malloc_mutex_unlock(TSDN_NULL, &arenas_lock);
  mi.fordblks = mi.hblkhd - mi.uordblks;
  mi.usmblks = mi.hblkhd;
  return mi;
}

size_t je_mallinfo_narenas() {
  return narenas_auto;
}

size_t je_mallinfo_nbins() {
  return SC_NBINS;
}

struct mallinfo je_mallinfo_arena_info(size_t aidx) {
  struct mallinfo mi = { 0 };

  malloc_mutex_lock(TSDN_NULL, &arenas_lock);
  if (aidx < narenas_auto) {
    arena_t* arena = atomic_load_p(&arenas[aidx], ATOMIC_ACQUIRE);
    if (arena != NULL) {
      mi.hblkhd = atomic_load_zu(&arena->pa_shard.pac.stats->pac_mapped, ATOMIC_ACQUIRE);
      mi.ordblks = accumulate_large_allocs(arena);
      mi.fsmblks = accumulate_small_allocs(arena);
    }
  }
  malloc_mutex_unlock(TSDN_NULL, &arenas_lock);
  return mi;
}

struct mallinfo je_mallinfo_bin_info(size_t aidx, size_t bidx) {
  struct mallinfo mi = { 0 };

  malloc_mutex_lock(TSDN_NULL, &arenas_lock);
  if (aidx < narenas_auto && bidx < SC_NBINS) {
    arena_t* arena = atomic_load_p(&arenas[aidx], ATOMIC_ACQUIRE);
    if (arena != NULL) {
      bin_t* bin = arena_bin_choose(TSDN_NULL, arena, (szind_t)bidx, NULL);

      malloc_mutex_lock(TSDN_NULL, &bin->lock);
      mi.ordblks = bin_infos[bidx].reg_size * bin->stats.curregs;
      mi.uordblks = (size_t) bin->stats.nmalloc;
      mi.fordblks = (size_t) bin->stats.ndalloc;
      malloc_mutex_unlock(TSDN_NULL, &bin->lock);
    }
  }
  malloc_mutex_unlock(TSDN_NULL, &arenas_lock);
  return mi;
}
