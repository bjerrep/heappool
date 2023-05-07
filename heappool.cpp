
#include "heappool_setup.h"
#include "heappool.h"

#include <string.h>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>


HeapPool *hp = 0;


void* operator new(size_t count)
{
    if (hp and count < hp->m_LargestCount)
    {
        void* p = hp->allocate_slot(count);
        if (p)
        {
            VERBOSE("new(%lu) returning %p (heappool)\n", count, p);
            return p;
        }
    }

    void* p = malloc(count);
    VERBOSE("new(%lu) returning %p (malloc)\n", count, p);
    return p;
}


void* operator new[]( size_t count )
{
    VERBOSE("new[] -> ");
    return ::operator new(count);
}


void operator delete(void* p) _GLIBCXX_USE_NOEXCEPT
{
    if (!hp or p < hp->m_heap_start or p >= hp->m_heap_end)
    {
        VERBOSE("delete(%p) (free)\n", p);
        free(p);
        return;
    }

    VERBOSE("delete(%p) (heappool)\n", p);
    hp->free_slot(p);
}


void operator delete[](void* p) _GLIBCXX_USE_NOEXCEPT
{
    VERBOSE("delete[] -> ");
    ::operator delete(p);
}


void log(char const* message, ...)
{
    char output[1000];
    va_list args;
    va_start(args, message);
    vsnprintf(output, 1000, message, args);
    va_end(args);
    printf(output);
}


HeapPool::HeapPool(int nof_pools, int const* setup)
    : m_nofPools(nof_pools),
      m_setup(setup)
{
    m_heap_bytes = 0;
    m_LargestCount = 0;

    for(int i=0; i<m_nofPools; i++)
    {
        m_heap_bytes += bytes(i) * entries(i);
        if (bytes(i) > m_LargestCount)
        {
            m_LargestCount = bytes(i);
        }
    }

    m_heap_start = malloc(m_heap_bytes);
    if(!m_heap_start)
    {
        OUT_OF_MEMORY();
    }
    VERBOSE("mm_init %p with %i bytes\n", m_heap_start, m_heap_bytes);
    memset(m_heap_start, 0x00, m_heap_bytes);
    m_heap_end = (char*) m_heap_start + m_heap_bytes;
}


/* To be called from ::operator new().
 * In an attempt to minimize the running time with interrupts disabled it
 * first tries to find a free candidate. If a candidate free slot
 * was found it calls unsafe_allocate_slot() with the candidate as
 * a initial suggestion, this time with interrupts disabled.
 */
void* HeapPool::allocate_slot(int user_bytes)
{
    void* p = hp->find_free_slot(user_bytes);
    if (p)
    {
        p = hp->safe_allocate_slot(user_bytes, p);
    }
    if (p)
    {
        VERBOSE("new(%lu) returning %p (heappool)\n", user_bytes, p);
        return p;
    }
    return nullptr;
}


/* To be called from ::operator delete().
 */
void HeapPool::free_slot(void* p)
{
    char* maskp = (char*) p - MASK_SIZE;
    if (!(*maskp & Masks::USED))
    {
        DOUBLE_FREE(p);
    }

    DISABLE_INTERRUPT();
    *(char*) maskp &= !Masks::USED;
    ENABLE_INTERRUPT();
}


void HeapPool::print_allocation_table()
{
    int largest_pool = 0;
    int row_count = 0;
    log("   ");
    for (int pool=0; pool<m_nofPools; pool++)
    {
        int pool_slots = entries(pool);
        if (pool_slots > largest_pool)
        {
            largest_pool = pool_slots;
        }
        int pool_count = bytes(pool);
        log("%4i bytes [%3i]      ", pool_count, pool_slots);
        row_count += pool_count;
    }
    log("\n");

    for (int row=0; row<largest_pool; row++)
    {
        log("%-3i ", row);

        int pool_offset = 0;

        for (int pool=0; pool<m_nofPools; pool++)
        {
            int pool_slots = entries(pool);
            int pool_count = bytes(pool);

            if (row >= pool_slots)
            {
                pool_offset += pool_count * pool_slots;
                continue;
            }

            char* p = (char*) m_heap_start + pool_offset + row * pool_count;

            if (!(*p & Masks::USED))
            {
                log("%p -      ", p);
            }
            else
            {
                log("%p used   ", p);
            }

            pool_offset += pool_count * pool_slots;
        }
        log("\n");
    }
}


int HeapPool::bytes(int pool_index)
{
    return m_setup[pool_index<<1];
}


int HeapPool::entries(int pool_index)
{
    return m_setup[(pool_index<<1) + 1];
}


void* HeapPool::find_free_slot(size_t user_bytes)
{
    void* p = m_heap_start;

    for (int pool=0; pool<m_nofPools; pool++)
    {
        int pool_bytes = bytes(pool);

        #ifdef ALLOCATE_AT_LEAST_HALF
            // If about to allocate less than half of a slot
            // then give up. Does not apply to the first pool.
            if (pool and (user_bytes < pool_bytes/2))
            {
                return nullptr;
            }
        #endif

        if (user_bytes <= pool_bytes - MASK_SIZE)
        {
            for (int j=0; j<entries(pool); j++)
            {
                if (!(*(char*) p & 0x01))
                {
                    return p;
                }
                p = (char*) p + pool_bytes;
            }
            return nullptr;
        }
        else
        {
            p = (char*) p + entries(pool) * pool_bytes;
        }
    }
    return nullptr;
}


/* Run critical code paths with interrupts disabled. If given a non null p
 * it will try use that right away if it points to a unused slot.
 */
void* HeapPool::safe_allocate_slot(size_t user_bytes, void* p)
{
    DISABLE_INTERRUPT();
    if (p and !(*(char*) p and Masks::USED))
    {
        *((char*) p) |= Masks::USED;
        ENABLE_INTERRUPT();
        return (char*) p + MASK_SIZE;
    }

    // Now p is suddenly used, another thread managed to claim
    // the slot inbetween. Too bad, just do a regular search.

    void* free_p = find_free_slot(user_bytes);
    if (free_p)
    {
        *((char*) free_p) |= Masks::USED;
        ENABLE_INTERRUPT();
        return (char*) free_p + MASK_SIZE;
    }
    ENABLE_INTERRUPT();
    return nullptr;
}
