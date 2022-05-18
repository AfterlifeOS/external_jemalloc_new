/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "jemalloc/internal/emap.h"

static pthread_mutex_t malloc_disabled_lock = PTHREAD_MUTEX_INITIALIZER;
static bool malloc_disabled_tcache;

int je_malloc_iterate(uintptr_t base, size_t size,
    void (*callback)(uintptr_t ptr, size_t size, void* arg), void* arg) {
      /*
  size_t pagesize = getpagesize();
  tsd_t* tsd = tsd_fetch_min();
  rtree_ctx_t *rtree_ctx = tsd_rtree_ctx(tsd);

  // Make sure the pointer is aligned to at least 8 bytes.
  uintptr_t ptr = (base + 7) & ~7;
  uintptr_t end_ptr = ptr + size;
  while (ptr < end_ptr) {
    edata_t* edata = emap_edata_lookup(tsd_tsdn(tsd), &arena_emap_global, (void*)ptr);
    if (edata == NULL) {
      // Skip to the next page, guaranteed no other pointers on this page.
      ptr += pagesize;
      continue;
    }

    if (edata_szind_get_maybe_invalid(edata) >= SC_NSIZES) {
      // Ignore this unused edata.
      ptr = (uintptr_t)edata_past_get(edata);
      continue;
    }

    szind_t szind;
    bool slab;
    rtree_metadata_t metadata = rtree_metadata_read(tsd_tsdn(tsd), &arena_emap_global.rtree, rtree_ctx, ptr);
    szind = metadata.szind;
    slab = metadata.slab;

    if (slab) {
      // Small allocation.
      szind_t binind = edata_szind_get(edata);
      const bin_info_t* bin_info = &bin_infos[binind];
      slab_data_t* slab_data = edata_slab_data_get(edata);

      uintptr_t first_ptr = (uintptr_t)edata_addr_get(edata);
      size_t bin_size = bin_info->reg_size;
      // Align the pointer to the bin size.
      ptr = (ptr + bin_size - 1) & ~(bin_size - 1);
      for (size_t bit = (ptr - first_ptr) / bin_size; bit < bin_info->bitmap_info.nbits; bit++) {
        if (bitmap_get(slab_data->bitmap, &bin_info->bitmap_info, bit)) {
          uintptr_t allocated_ptr = first_ptr + bin_size * bit;
          if (allocated_ptr >= end_ptr) {
            break;
          }
          callback(allocated_ptr, bin_size, arg);
        }
      }
    } else if (edata_state_get(edata) == extent_state_active) {
      // Large allocation.
      uintptr_t base_ptr = (uintptr_t)edata_addr_get(edata);
      if (ptr <= base_ptr) {
        // This edata is actually allocated and within the range to check.
        callback(base_ptr, edata_usize_get(edata), arg);
      }
    }
    ptr = (uintptr_t)edata_past_get(edata);
  }
  */
  return 0;
}

static void je_malloc_disable_prefork() {
  pthread_mutex_lock(&malloc_disabled_lock);
}

static void je_malloc_disable_postfork_parent() {
  pthread_mutex_unlock(&malloc_disabled_lock);
}

static void je_malloc_disable_postfork_child() {
  pthread_mutex_init(&malloc_disabled_lock, NULL);
}

void je_malloc_disable_init() {
  if (pthread_atfork(je_malloc_disable_prefork,
      je_malloc_disable_postfork_parent, je_malloc_disable_postfork_child) != 0) {
    malloc_write("<jemalloc>: Error in pthread_atfork()\n");
    if (opt_abort)
      abort();
  }
}

void je_malloc_disable() {
  static pthread_once_t once_control = PTHREAD_ONCE_INIT;
  pthread_once(&once_control, je_malloc_disable_init);

  pthread_mutex_lock(&malloc_disabled_lock);
  bool new_tcache = false;
  size_t old_len = sizeof(malloc_disabled_tcache);

  // Disable the tcache (if not already disabled) so that we don't
  // have to search the tcache for pointers.
  je_mallctl("thread.tcache.enabled",
      &malloc_disabled_tcache, &old_len,
      &new_tcache, sizeof(new_tcache));
  jemalloc_prefork();
}

void je_malloc_enable() {
  jemalloc_postfork_parent();
  if (malloc_disabled_tcache) {
    // Re-enable the tcache if it was enabled before the disabled call.
    je_mallctl("thread.tcache.enabled", NULL, NULL,
        &malloc_disabled_tcache, sizeof(malloc_disabled_tcache));
  }
  pthread_mutex_unlock(&malloc_disabled_lock);
}
