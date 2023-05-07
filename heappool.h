#pragma once

#include <cstddef>


void log(char const* message, ...);


struct HeapPool
{
    struct Pool
    {
        int bytes = 0;
        int entries = 0;
    };

    struct Table
    {
        int entries;
        Pool lst[];
    };

    enum Masks
    {
        USED    = 0x01
    };

    const int MASK_SIZE = 1;

    HeapPool(int nof_pools, int const* setup);

    void* allocate_slot(int count);

    int bytes(int pool_index);

    int entries(int pool_index);

    void* find_free_slot(size_t count);

    void* safe_allocate_slot(size_t bytes, void* p);

    void free_slot(void* p);

    void print_allocation_table();

    int m_nofPools = 0;
    int const* m_setup = 0;
    int m_LargestCount = 0;
    void* m_heap_start = 0;
    void* m_heap_end = 0;
    size_t m_heap_bytes = 0;

};

extern HeapPool *hp;
