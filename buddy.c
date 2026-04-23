#include "buddy.h"
#include <stdlib.h>
#define NULL ((void *)0)

#define PAGE_SIZE (4096)
#define MAX_RANK (16)
#define MAX_BLOCKS (65536)

typedef struct block {
    struct block *next;
    struct block *prev;
    int rank;
    int allocated;
} block_t;

static void *base_addr = NULL;
static int total_pages = 0;
static block_t *blocks = NULL;
static block_t *free_lists[MAX_RANK + 1];
static int block_count = 0;

static int get_block_index(void *addr) {
    if (addr < base_addr || addr >= base_addr + total_pages * PAGE_SIZE) {
        return -1;
    }
    return (int)((char *)addr - (char *)base_addr) / PAGE_SIZE;
}

static void *get_block_addr(int index) {
    return (char *)base_addr + index * PAGE_SIZE;
}

static int get_buddy_index(int index, int rank) {
    int block_size = 1 << (rank - 1);
    return index ^ block_size;
}

static void add_to_free_list(int index, int rank) {
    block_t *block = &blocks[index];
    block->rank = rank;
    block->allocated = 0;
    
    block->next = free_lists[rank];
    block->prev = NULL;
    
    if (free_lists[rank]) {
        free_lists[rank]->prev = block;
    }
    free_lists[rank] = block;
}

static void remove_from_free_list(int index) {
    block_t *block = &blocks[index];
    
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        free_lists[block->rank] = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    block->allocated = 1;
    block->next = NULL;
    block->prev = NULL;
}

int init_page(void *p, int pgcount) {
    if (!p || pgcount <= 0) {
        return -EINVAL;
    }
    
    base_addr = p;
    total_pages = pgcount;
    
    blocks = (block_t *)calloc(MAX_BLOCKS, sizeof(block_t));
    if (!blocks) {
        return -ENOSPC;
    }
    
    for (int i = 0; i <= MAX_RANK; i++) {
        free_lists[i] = NULL;
    }
    
    int max_rank = 1;
    int size = pgcount;
    while (size > 1) {
        size = (size + 1) / 2;
        max_rank++;
    }
    if (max_rank > MAX_RANK) {
        max_rank = MAX_RANK;
    }
    
    add_to_free_list(0, max_rank);
    block_count = pgcount;
    
    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) {
        return ERR_PTR(-EINVAL);
    }
    
    int current_rank = rank;
    while (current_rank <= MAX_RANK && !free_lists[current_rank]) {
        current_rank++;
    }
    
    if (current_rank > MAX_RANK) {
        return ERR_PTR(-ENOSPC);
    }
    
    block_t *block = free_lists[current_rank];
    int block_index = block - blocks;
    remove_from_free_list(block_index);
    
    while (current_rank > rank) {
        current_rank--;
        int buddy_index = block_index + (1 << (current_rank - 1));
        add_to_free_list(buddy_index, current_rank);
    }
    
    blocks[block_index].rank = rank;
    blocks[block_index].allocated = 1;
    
    return get_block_addr(block_index);
}

int return_pages(void *p) {
    if (!p) {
        return -EINVAL;
    }
    
    int block_index = get_block_index(p);
    if (block_index < 0 || block_index >= block_count) {
        return -EINVAL;
    }
    
    block_t *block = &blocks[block_index];
    if (!block->allocated) {
        return -EINVAL;
    }
    
    int rank = block->rank;
    int current_index = block_index;
    
    while (rank < MAX_RANK) {
        int buddy_index = get_buddy_index(current_index, rank);
        
        if (buddy_index >= block_count || 
            blocks[buddy_index].allocated || 
            blocks[buddy_index].rank != rank) {
            break;
        }
        
        remove_from_free_list(buddy_index);
        
        if (buddy_index < current_index) {
            current_index = buddy_index;
        }
        
        rank++;
    }
    
    add_to_free_list(current_index, rank);
    return OK;
}

int query_ranks(void *p) {
    if (!p) {
        return -EINVAL;
    }
    
    int block_index = get_block_index(p);
    if (block_index < 0 || block_index >= block_count) {
        return -EINVAL;
    }
    
    block_t *block = &blocks[block_index];
    if (block->allocated) {
        return block->rank;
    }
    
    int max_rank = 1;
    int size = total_pages;
    while (size > 1) {
        size = (size + 1) / 2;
        max_rank++;
    }
    if (max_rank > MAX_RANK) {
        max_rank = MAX_RANK;
    }
    
    return max_rank;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) {
        return -EINVAL;
    }
    
    int count = 0;
    block_t *current = free_lists[rank];
    while (current) {
        count++;
        current = current->next;
    }
    
    return count;
}
