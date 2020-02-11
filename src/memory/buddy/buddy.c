#include <memory/buddy/buddy.h>
#include <memory/buddy/buddy_internal.h>
#include <idt/idt.h>
#include <libc/errno.h>

/*
 * This files handles the buddy allocator which allows to allocate 2^^n pages
 * large blocks of memory.
 *
 * This allocator works by dividing blocks of memory in two until the a block of
 * the required size is available.
 *
 * The order of a block is the `n` in the expression `2^^n` that represents the
 * size of a block in pages.
 */

 /*
  * The list of linked lists containing free blocks, sorted according to blocks'
  * order.
  */
ATTR_BSS
static buddy_free_block_t *free_list[BUDDY_MAX_ORDER + 1];

/*
 * The tree containing free blocks sorted according to their address.
 */
static avl_tree_t *free_tree = NULL;

/*
 * The spinlock used for buddy allocator operations.
 */
static spinlock_t spinlock = 0;

/*
 * Returns the buddy order required to fit the given number of pages.
 */
ATTR_HOT
block_order_t buddy_get_order(const size_t pages)
{
	block_order_t order = 0;
	size_t i = 1;

	while(i < pages)
	{
		i <<= 1;
		++order;
	}
	return order;
}

/*
 * Returns the AVL node of the nearest free block from the given block.
 */
static avl_tree_t *get_nearest_free_block(buddy_free_block_t *block)
{
	avl_tree_t *n;

	if(!(n = free_tree))
		return NULL;
	while(n)
	{
		if(block == (void *) n->value)
			break;
		if(ABS((intptr_t) block - (intptr_t) n->left)
			< ABS((intptr_t) block - (intptr_t) n->right))
			n = n->left;
		else
			n = n->right;
	}
	return n;
}

/*
 * Links a free block for the given pointer with the given order.
 * The block must not be inserted yet.
 */
static void link_free_block(buddy_free_block_t *ptr,
	const block_order_t order)
{
	avl_tree_t *n;
	buddy_free_block_t *b;

	ptr->prev_free = NULL;
	if((ptr->next_free = free_list[order]))
		ptr->next_free->prev_free = ptr;
	if((n = get_nearest_free_block(ptr)))
	{
		b = CONTAINER_OF(n, buddy_free_block_t, node);
		if(b < ptr)
		{
			if((ptr->next = b->next))
				ptr->next->prev = ptr;
			if((ptr->prev = b))
				ptr->prev->next = ptr;
		}
		else
		{
			if((ptr->prev = b->prev))
				ptr->prev->next = ptr;
			if((ptr->next = b))
				ptr->next->prev = ptr;
		}
	}
	else
	{
		ptr->prev = NULL;
		ptr->next = NULL;
	}
	ptr->node.value = (avl_value_t) ptr;
	avl_tree_insert(&free_tree, n, ptr_cmp);
	ptr->order = order;
}

/*
 * Unlinks the given block from the free list and free tree.
 */
static void unlink_free_block(buddy_free_block_t *block)
{
	if(block == free_list[block->order])
		free_list[block->order] = free_list[block->order]->next;
	else
		block->prev->next = block->next;
	avl_tree_remove(&free_tree, &block->node);
}

/*
 * Initializes the buddy allocator.
 */
ATTR_COLD
void buddy_init(void)
{
	void *i = mem_info.heap_begin;
	block_order_t order;

	while(i < mem_info.heap_end)
	{
		order = MIN(mem_info.heap_end - i, MAX_BLOCK_SIZE);
		link_free_block(i, order);
		i += BLOCK_SIZE(order);
	}
}

/*
 * Splits the given block until a block of the required order is created and
 * returns it.
 * The input block will be unlinked and the new blocks created will be inserted
 * into the free list and free tree except the returned block.
 */
static buddy_free_block_t *split_block(buddy_free_block_t *block,
	const block_order_t order)
{
	unlink_free_block(block);
	while(block->order > order)
	{
		--block->order;
		link_free_block((void *) block + BLOCK_SIZE(block->order),
			block->order);
	}
	return block;
}

/*
 * Allocates a block of memory using the buddy allocator.
 */
ATTR_HOT
ATTR_MALLOC
void *buddy_alloc(const block_order_t order)
{
	size_t i;

	errno = 0;
	if(order > BUDDY_MAX_ORDER)
		return NULL;
	i = order;
	while(i < BUDDY_MAX_ORDER + 1 && !free_list[i])
		++i;
	if(!free_list[i])
	{
		errno = ENOMEM;
		return NULL;
	}
	return split_block(free_list[i], order);
}

/*
 * Uses `buddy_alloc` and applies `bzero` on the allocated block.
 */
ATTR_HOT
ATTR_MALLOC
void *buddy_alloc_zero(const block_order_t order)
{
	void *ptr;

	if((ptr = buddy_alloc(order)))
		bzero(ptr, BLOCK_SIZE(order));
	return ptr;
}

/*
 * Allocates a block of memory using the buddy allocator in the specified range.
 */
ATTR_HOT
ATTR_MALLOC
void *buddy_alloc_inrange(const block_order_t order, void *begin, void *end)
{
	avl_tree_t *n;
	buddy_free_block_t *b;

	errno = 0;
	begin = ALIGN(begin, PAGE_SIZE);
	end = DOWN_ALIGN(end, PAGE_SIZE);
	if(!(n = get_nearest_free_block(begin)))
	{
		errno = ENOMEM;
		return NULL;
	}
	b = CONTAINER_OF(n, buddy_free_block_t, node);
	// TODO Some previous blocks might be in the range?
	while(b && (void *) b < end && b->order < order)
		b = b->next;
	if(!b || b->order < order)
	{
		errno = ENOMEM;
		return NULL;
	}
	return split_block(b, order);
}

/*
 * Uses `buddy_alloc_inrange` and applies `bzero` on the allocated block.
 */
ATTR_MALLOC
void *buddy_alloc_zero_inrange(block_order_t order, void *begin, void *end)
{
	void *ptr;

	if((ptr = buddy_alloc_inrange(order, begin, end)))
		bzero(ptr, BLOCK_SIZE(order));
	return ptr;
}

/*
 * Returns the given block's buddy.
 * Returns `NULL` if the buddy block is not free.
 */
static buddy_free_block_t *get_buddy(void *ptr, const block_order_t order)
{
	void *buddy_addr;

	buddy_addr = (void *) ((ptr - mem_info.heap_begin) ^ (PAGE_SIZE << order));
	if(!avl_tree_search(free_tree, (avl_value_t) buddy_addr, ptr_cmp))
		return NULL;
	return buddy_addr;
}

/*
 * Frees the given memory block that was allocated using the buddy allocator.
 * The given order must be the same as the one given to allocate the block.
 */
ATTR_HOT
void buddy_free(void *ptr, block_order_t order)
{
	void *buddy;

	link_free_block(ptr, order);
	while(order < BUDDY_MAX_ORDER && (buddy = get_buddy(ptr, order)))
	{
		if(buddy < ptr)
			swap_ptr(&ptr, &buddy);
		unlink_free_block(ptr);
		unlink_free_block(buddy);
		((buddy_free_block_t *) ptr)->order = ++order;
		link_free_block(ptr, order);
	}
}

/*
 * Returns the total number of pages allocated by the buddy allocator.
 */
ATTR_HOT
size_t allocated_pages(void)
{
	// TODO
	return 0;
}
