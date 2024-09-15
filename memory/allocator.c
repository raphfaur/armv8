#include "./allocator.h"
#include "../utils/debug.h"
#include "math.h"

#define BLOCK_FREE 0
#define BLOCK_ALLOCATED 1

#define BLOCK_SPLITTED 0
#define BLOCK_MERGED 1

#define MAX_BLOCK_INDEX 31

/* 2^(order + 1) - 1 */
#define BLOCK_COUNT 31

static block_t* allocated_blocks[MAX_BLOCK_INDEX];
static void * allocated_address[MAX_BLOCK_INDEX];
static block_t blocks_pool[BLOCK_COUNT]; 

static block_t root_block;



/* BT helper */
void _set_all_children_allocated(block_t *block, int allocated) {
    block->allocated = allocated;
    if (block->left != NULL)
    {
        _set_all_children_allocated(block->left, allocated);
    }
    if (block->right != NULL)
    {
        _set_all_children_allocated(block->right, allocated);
    }
}

void _set_all_children_split(block_t *block, int split) {
    block->split = split;
    if (block->left != NULL)
    {
        _set_all_children_split(block->left, split);
    }
    if (block->right != NULL)
    {
        _set_all_children_split(block->right, split);
    }
}

/* Merge parent if possible, return 1 is merged, else 0 */
int _merge_if_possible(block_t * parent) {
    if (parent->left->allocated == BLOCK_FREE && parent->right->allocated == BLOCK_FREE) {
        DEBUG("Merging")
        parent->split = BLOCK_MERGED;
        parent->allocated = BLOCK_FREE;
        _set_all_children_split(parent, BLOCK_MERGED);
        return 1;
    }
    return 0;
}

void _free_block(block_t * block) {
    /* Set block as free and do the same for alll children */
    _set_all_children_allocated(block, BLOCK_FREE);
    _merge_if_possible(block->parent);
}

void mark_block_allocated(block_t *block) {
    _set_all_children_allocated(block, BLOCK_ALLOCATED);
}

block_t * _allocate_order(block_t *block, int order) {
    if (block->order == order && block->split == BLOCK_MERGED &&  block->allocated == BLOCK_FREE)
    {
        mark_block_allocated(block);
        return block;
    }
    if (block->order == order && block->allocated == BLOCK_ALLOCATED) {};
    if (block->order > order)
    {
        if (block->allocated != BLOCK_ALLOCATED)
        {
            block->split = BLOCK_SPLITTED;
            block_t *left_allocate = _allocate_order(block->left, order);
            if ( left_allocate != NULL) {
                return left_allocate;
            }
            block_t *right_allocate = _allocate_order(block->right, order);
            if ( right_allocate != NULL) {
                return right_allocate;
            }
        }
    }
    return NULL;
}

int _save_address_block(void * address, block_t* block) {
    for (size_t i = 0; i < MAX_BLOCK_INDEX; i++)
    {
        if (allocated_address[i] == NULL) {
            allocated_address[i] = address;
            allocated_blocks[i] = block;
            return 0;
        }
    }
    return -1;
}

block_t * _erase_address_block(void * address) {
    for (size_t i = 0; i < MAX_BLOCK_INDEX; i++)
    {
        if (allocated_address[i] == address) {
            allocated_address[i] = NULL;
            block_t * block = allocated_blocks[i];
            allocated_blocks[i] = NULL;
            return block;
        }
    }
    return NULL;
}

void * alloc(int order) {
    block_t * allocated_block = _allocate_order(&root_block, order);
    if (allocated_block == NULL)
    {
        DEBUG("Could not allocate")
        return NULL;
    }
    DEBUGD(allocated_block->index);
    int order_size = pow(2, order) * PAGE_SIZE;
    void * address = (void *) (allocated_block->index * order_size + 0x10000000);
    DEBUGH((uint64_t)address);
    /* Error, unexcpected behavior */
    if(_save_address_block(address, allocated_block) < 0){
        DEBUG("Could not save address")
        return NULL;
    }
    return address;
}

void free(void * address) {
    DEBUG("Erasing block")
    block_t * block = _erase_address_block(address);
    if (block != NULL)
    {
        DEBUG("Freeing block")
        _free_block(block);
    }
}

void populate_bt(block_t * block, int order, int index, block_t * parent) {
    block->order = order;
    block->index = index;
    block->split = BLOCK_MERGED;
    block->allocated = BLOCK_FREE;
    block->parent = parent;
    int depth = MAX_ORDER - order;
    int base_index = pow(2, depth + 1) - 1;
    if (order != 0)
    {
        int left_index = index * 2;
        block->left = &blocks_pool[base_index + left_index];
        populate_bt(block->left, order - 1, left_index, block);
        int right_index = left_index + 1;
        block->right = &blocks_pool[base_index + right_index];
        populate_bt(block->right, order - 1, right_index, block);
    }
}

void init_allocator() {
    root_block = blocks_pool[0];
    root_block.order = MAX_ORDER;
    root_block.index = 0;
    populate_bt(&root_block, MAX_ORDER, 0, NULL);
}

