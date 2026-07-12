/**
 ******************************************************************************
 * @file    dram.c
 * @author  Typheye
 * @brief   Dual-region dynamic RAM allocator implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "dram.h"

#include <stdint.h>
#include <string.h>

#include "core/sys/include/syslog.h"
#include "stm32f4xx.h"

#ifndef SYSDRAM_RAM_HEAP_SIZE
#define SYSDRAM_RAM_HEAP_SIZE ((69U * 1024U) + 780U)
#endif

#ifndef SYSDRAM_CCM_HEAP_SIZE
#define SYSDRAM_CCM_HEAP_SIZE ((55U * 1024U) + 328U)
#endif

#define SYSDRAM_MAGIC_USED 0x44524D55UL
#define SYSDRAM_MAGIC_FREE 0x44524D46UL
#define SYSDRAM_ALIGN      8U
#define SYSDRAM_CODE __attribute__((noinline))
#define SYSDRAM_STACK_GUARD_SIZE (8U * 1024U)
#define SYSDRAM_PHYS_RAM_SIZE (128UL * 1024UL)
#define SYSDRAM_PHYS_CCM_SIZE (64UL * 1024UL)

typedef struct SysDram_Block {
  uint32_t magic;
  uint32_t size;
  struct SysDram_Block *next;
  struct SysDram_Block *prev;
  uint8_t used;
  uint8_t region;
  uint16_t reserved;
} SysDramBlock_t;

typedef struct {
  uint8_t *base;
  uint32_t size;
  SysDramBlock_t *first;
  uint32_t used;
  uint32_t peak_used;
  uint32_t alloc_count;
} SysDramPool_t;

static SysDramPool_t g_pools[2];
static volatile uint8_t g_inited = 0;

extern uint8_t __sysdram_ram_fixed_end__;
extern uint8_t _end;
extern uint8_t _estack;
extern uint32_t _Min_Stack_Size;
extern uint8_t _eccmram;

static SYSDRAM_CODE uint32_t align_up(uint32_t v) {
  return (v + (SYSDRAM_ALIGN - 1U)) & ~(SYSDRAM_ALIGN - 1U);
}

static SYSDRAM_CODE uint32_t irq_save(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static SYSDRAM_CODE void irq_restore(uint32_t primask) {
  if ((primask & 1U) == 0U) {
    __enable_irq();
  }
}

static SYSDRAM_CODE void init_pool(SysDramPool_t *pool, uint8_t *base,
                                   uint32_t size, uint8_t region) {
  uintptr_t start = ((uintptr_t)base + (SYSDRAM_ALIGN - 1U)) &
                    ~(uintptr_t)(SYSDRAM_ALIGN - 1U);
  uintptr_t end = ((uintptr_t)base + size) & ~(uintptr_t)(SYSDRAM_ALIGN - 1U);
  if (end <= start + sizeof(SysDramBlock_t)) {
    pool->base = NULL;
    pool->size = 0;
    pool->first = NULL;
    return;
  }

  pool->base = (uint8_t *)start;
  pool->size = (uint32_t)(end - start);
  pool->used = 0;
  pool->peak_used = 0;
  pool->alloc_count = 0;
  pool->first = (SysDramBlock_t *)pool->base;
  pool->first->magic = SYSDRAM_MAGIC_FREE;
  pool->first->size = pool->size - sizeof(SysDramBlock_t);
  pool->first->next = NULL;
  pool->first->prev = NULL;
  pool->first->used = 0;
  pool->first->region = region;
  pool->first->reserved = 0;
}

SYSDRAM_CODE void SysDram_Init(void) {
  uint32_t irq = irq_save();
  if (!g_inited) {
    uintptr_t ram_start = ((uintptr_t)&_end + (SYSDRAM_ALIGN - 1U)) &
                          ~(uintptr_t)(SYSDRAM_ALIGN - 1U);
    uintptr_t stack_reserve = (uintptr_t)&_Min_Stack_Size;
    if (stack_reserve < SYSDRAM_STACK_GUARD_SIZE) {
      stack_reserve = SYSDRAM_STACK_GUARD_SIZE;
    }
    uintptr_t ram_end = ((uintptr_t)&_estack - stack_reserve) &
                        ~(uintptr_t)(SYSDRAM_ALIGN - 1U);
    uintptr_t ccm_start = ((uintptr_t)&_eccmram + (SYSDRAM_ALIGN - 1U)) &
                          ~(uintptr_t)(SYSDRAM_ALIGN - 1U);
    uintptr_t ccm_end = (0x10000000UL + 64UL * 1024UL) &
                        ~(uintptr_t)(SYSDRAM_ALIGN - 1U);
    init_pool(&g_pools[SYSDRAM_REGION_RAM], (uint8_t *)ram_start,
              ram_end > ram_start ? (uint32_t)(ram_end - ram_start) : 0U,
              SYSDRAM_REGION_RAM);
    init_pool(&g_pools[SYSDRAM_REGION_CCM], (uint8_t *)ccm_start,
              ccm_end > ccm_start ? (uint32_t)(ccm_end - ccm_start) : 0U,
              SYSDRAM_REGION_CCM);
    g_inited = 1;
  }
  irq_restore(irq);
}

static SYSDRAM_CODE void ensure_init(void) {
  if (!g_inited) {
    SysDram_Init();
  }
}

static SYSDRAM_CODE void split_block(SysDramBlock_t *block, uint32_t size) {
  uint32_t remain = block->size - size;
  if (remain < sizeof(SysDramBlock_t) + SYSDRAM_ALIGN) {
    return;
  }

  SysDramBlock_t *next =
      (SysDramBlock_t *)((uint8_t *)block + sizeof(SysDramBlock_t) + size);
  next->magic = SYSDRAM_MAGIC_FREE;
  next->size = remain - sizeof(SysDramBlock_t);
  next->next = block->next;
  next->prev = block;
  next->used = 0;
  next->region = block->region;
  next->reserved = 0;
  if (next->next) {
    next->next->prev = next;
  }
  block->next = next;
  block->size = size;
}

static SYSDRAM_CODE void *pool_alloc(SysDramPool_t *pool, uint32_t size) {
  if (!pool || !pool->first || size == 0U) {
    return NULL;
  }

  uint32_t need = align_up(size);
  for (SysDramBlock_t *b = pool->first; b; b = b->next) {
    if (!b->used && b->magic == SYSDRAM_MAGIC_FREE && b->size >= need) {
      split_block(b, need);
      b->used = 1;
      b->magic = SYSDRAM_MAGIC_USED;
      pool->used += b->size;
      if (pool->used > pool->peak_used) {
        pool->peak_used = pool->used;
      }
      pool->alloc_count++;
      return (uint8_t *)b + sizeof(SysDramBlock_t);
    }
  }
  return NULL;
}

static SYSDRAM_CODE void merge_next(SysDramBlock_t *block) {
  SysDramBlock_t *next = block ? block->next : NULL;
  if (!next || next->used || next->magic != SYSDRAM_MAGIC_FREE) {
    return;
  }

  block->size += sizeof(SysDramBlock_t) + next->size;
  block->next = next->next;
  if (block->next) {
    block->next->prev = block;
  }
}

static SYSDRAM_CODE SysDramPool_t *ptr_pool(const void *ptr) {
  uintptr_t p = (uintptr_t)ptr;
  for (unsigned i = 0; i < 2U; ++i) {
    uintptr_t start = (uintptr_t)g_pools[i].base;
    uintptr_t end = start + g_pools[i].size;
    if (start != 0U && p >= start && p < end) {
      return &g_pools[i];
    }
  }
  return NULL;
}

SYSDRAM_CODE void SysDram_Free(void *ptr) {
  if (!ptr) {
    return;
  }
  ensure_init();

  uint32_t irq = irq_save();
  SysDramPool_t *pool = ptr_pool(ptr);
  if (!pool) {
    irq_restore(irq);
    return;
  }

  SysDramBlock_t *block =
      (SysDramBlock_t *)((uint8_t *)ptr - sizeof(SysDramBlock_t));
  if (block->magic != SYSDRAM_MAGIC_USED || !block->used) {
    irq_restore(irq);
    return;
  }

  block->used = 0;
  block->magic = SYSDRAM_MAGIC_FREE;
  if (pool->used >= block->size) {
    pool->used -= block->size;
  } else {
    pool->used = 0;
  }
  merge_next(block);
  if (block->prev && !block->prev->used) {
    merge_next(block->prev);
  }
  irq_restore(irq);
}

SYSDRAM_CODE void *SysDram_AllocFast(size_t size) {
  ensure_init();
  if (size == 0U || size > 0x7FFFFFFFU) {
    return NULL;
  }

  uint32_t irq = irq_save();
  void *p = pool_alloc(&g_pools[SYSDRAM_REGION_CCM], (uint32_t)size);
  if (!p) {
    p = pool_alloc(&g_pools[SYSDRAM_REGION_RAM], (uint32_t)size);
  }
  irq_restore(irq);
  return p;
}

SYSDRAM_CODE void *SysDram_AllocDma(size_t size) {
  ensure_init();
  if (size == 0U || size > 0x7FFFFFFFU) {
    return NULL;
  }

  uint32_t irq = irq_save();
  void *p = pool_alloc(&g_pools[SYSDRAM_REGION_RAM], (uint32_t)size);
  irq_restore(irq);
  return p;
}

SYSDRAM_CODE void *SysDram_Alloc(size_t size) {
  ensure_init();
  if (size == 0U || size > 0x7FFFFFFFU) {
    return NULL;
  }

  uint32_t irq = irq_save();
  void *p = pool_alloc(&g_pools[SYSDRAM_REGION_RAM], (uint32_t)size);
  if (!p) {
    p = pool_alloc(&g_pools[SYSDRAM_REGION_CCM], (uint32_t)size);
  }
  irq_restore(irq);
  return p;
}

SYSDRAM_CODE void *SysDram_Calloc(size_t count, size_t size) {
  if (size != 0U && count > ((size_t)-1) / size) {
    return NULL;
  }
  size_t total = count * size;
  void *p = SysDram_Alloc(total);
  if (p) {
    memset(p, 0, total);
  }
  return p;
}

SYSDRAM_CODE void *SysDram_Realloc(void *ptr, size_t size) {
  if (!ptr) {
    return SysDram_Alloc(size);
  }
  if (size == 0U) {
    SysDram_Free(ptr);
    return NULL;
  }

  ensure_init();
  SysDramBlock_t *block =
      (SysDramBlock_t *)((uint8_t *)ptr - sizeof(SysDramBlock_t));
  if (block->magic != SYSDRAM_MAGIC_USED) {
    return NULL;
  }
  if (block->size >= size) {
    return ptr;
  }

  void *np = SysDram_Alloc(size);
  if (!np) {
    return NULL;
  }
  memcpy(np, ptr, block->size);
  SysDram_Free(ptr);
  return np;
}

SYSDRAM_CODE int SysDram_IsCcmPtr(const void *ptr) {
  ensure_init();
  return ptr_pool(ptr) == &g_pools[SYSDRAM_REGION_CCM];
}

SYSDRAM_CODE int SysDram_IsRamPtr(const void *ptr) {
  ensure_init();
  return ptr_pool(ptr) == &g_pools[SYSDRAM_REGION_RAM];
}

SYSDRAM_CODE int SysDram_GetStats(SysDramRegion_t region,
                                  SysDramStats_t *out) {
  ensure_init();
  if (!out || region > SYSDRAM_REGION_CCM) {
    return 0;
  }

  uint32_t irq = irq_save();
  SysDramPool_t *pool = &g_pools[region];
  uint32_t free_total = 0;
  uint32_t largest = 0;
  for (SysDramBlock_t *b = pool->first; b; b = b->next) {
    if (!b->used && b->magic == SYSDRAM_MAGIC_FREE) {
      free_total += b->size;
      if (b->size > largest) {
        largest = b->size;
      }
    }
  }

  out->total = pool->size;
  out->used = pool->used;
  out->free = free_total;
  out->largest_free = largest;
  out->alloc_count = pool->alloc_count;
  out->peak_used = pool->peak_used;
  irq_restore(irq);
  return 1;
}

void SysDram_LogStats(void) {
  SysDramStats_t ram;
  SysDramStats_t ccm;
  if (SysDram_GetStats(SYSDRAM_REGION_RAM, &ram) &&
      SysDram_GetStats(SYSDRAM_REGION_CCM, &ccm)) {
    uint32_t dyn_total = ram.total + ccm.total;
    uint32_t phys_total = SYSDRAM_PHYS_RAM_SIZE + SYSDRAM_PHYS_CCM_SIZE;
    LOG_I("DRAM", "Dynamic cap RAM=%lu/%luB(%lu.%lu%%) CCM=%lu/%luB(%lu.%lu%%)",
          (unsigned long)ram.total, (unsigned long)SYSDRAM_PHYS_RAM_SIZE,
          (unsigned long)(ram.total * 1000UL / SYSDRAM_PHYS_RAM_SIZE / 10UL),
          (unsigned long)(ram.total * 1000UL / SYSDRAM_PHYS_RAM_SIZE % 10UL),
          (unsigned long)ccm.total, (unsigned long)SYSDRAM_PHYS_CCM_SIZE,
          (unsigned long)(ccm.total * 1000UL / SYSDRAM_PHYS_CCM_SIZE / 10UL),
          (unsigned long)(ccm.total * 1000UL / SYSDRAM_PHYS_CCM_SIZE % 10UL));
    LOG_I("DRAM", "Dynamic cap total=%lu/%luB(%lu.%lu%%)",
          (unsigned long)dyn_total, (unsigned long)phys_total,
          (unsigned long)(dyn_total * 1000UL / phys_total / 10UL),
          (unsigned long)(dyn_total * 1000UL / phys_total % 10UL));
    LOG_I("DRAM", "RAM pool total=%lu used=%lu free=%lu largest=%lu peak=%lu",
          (unsigned long)ram.total, (unsigned long)ram.used,
          (unsigned long)ram.free, (unsigned long)ram.largest_free,
          (unsigned long)ram.peak_used);
    LOG_I("DRAM", "CCM pool total=%lu used=%lu free=%lu largest=%lu peak=%lu",
          (unsigned long)ccm.total, (unsigned long)ccm.used,
          (unsigned long)ccm.free, (unsigned long)ccm.largest_free,
          (unsigned long)ccm.peak_used);
  }
}

SYSDRAM_CODE void *malloc(size_t size) {
  return SysDram_Alloc(size);
}

SYSDRAM_CODE void free(void *ptr) {
  SysDram_Free(ptr);
}

SYSDRAM_CODE void *calloc(size_t count, size_t size) {
  return SysDram_Calloc(count, size);
}

SYSDRAM_CODE void *realloc(void *ptr, size_t size) {
  return SysDram_Realloc(ptr, size);
}

struct _reent;

SYSDRAM_CODE void *_malloc_r(struct _reent *r, size_t size) {
  (void)r;
  return SysDram_Alloc(size);
}

SYSDRAM_CODE void _free_r(struct _reent *r, void *ptr) {
  (void)r;
  SysDram_Free(ptr);
}

SYSDRAM_CODE void *_calloc_r(struct _reent *r, size_t count, size_t size) {
  (void)r;
  return SysDram_Calloc(count, size);
}

SYSDRAM_CODE void *_realloc_r(struct _reent *r, void *ptr, size_t size) {
  (void)r;
  return SysDram_Realloc(ptr, size);
}
