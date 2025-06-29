/* anon.c: 디스크 이미지가 아닌 페이지(익명 페이지)의 구현 */
#include <bitmap.h>

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "devices/disk.h"
#define SECTOR_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)
static struct bitmap *swap_bitmap;
static struct lock swap_lock;

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
		.swap_in = anon_swap_in,
		.swap_out = anon_swap_out,
		.destroy = anon_destroy,
		.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/** Project 3-Swap In/Out */
	swap_disk = disk_get(1, 1);
	swap_bitmap = bitmap_create(disk_size(swap_disk) / SECTOR_PER_PAGE);
	lock_init(&swap_lock);
}
/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/** Project 3-Swap In/Out */
	struct uninit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->page_no = BITMAP_ERROR;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;
	/** Project 3-Swap In/Out */

	if (anon_page->page_no == BITMAP_ERROR)
	{

		return false;
	}

	if (!bitmap_test(swap_bitmap, anon_page->page_no))
	{

		return false;
	}

	for (size_t i = 0; i < SECTOR_PER_PAGE; i++)
		disk_read(swap_disk, (anon_page->page_no * SECTOR_PER_PAGE) + i, kva + (i * DISK_SECTOR_SIZE));
	lock_acquire(&swap_lock);
	bitmap_set(swap_bitmap, anon_page->page_no, false);

	lock_release(&swap_lock);
	anon_page->page_no = BITMAP_ERROR;

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	/** Project 3-Swap In/Out */
	lock_acquire(&swap_lock);
	size_t page_no = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

	lock_release(&swap_lock);
	if (page_no == BITMAP_ERROR)
	{
		return false;
	}

	for (size_t i = 0; i < SECTOR_PER_PAGE; i++)
		disk_write(swap_disk, (page_no * SECTOR_PER_PAGE) + i, page->va + (i * DISK_SECTOR_SIZE));
	anon_page->page_no = page_no;
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;
	/** Project 3-Swap In/Out */
	if (anon_page->page_no != BITMAP_ERROR)
	{
		lock_acquire(&swap_lock);
		bitmap_reset(swap_bitmap, anon_page->page_no);
		lock_release(&swap_lock);
	}
	if (page->frame)
	{
		list_remove(&page->frame->elem);
		page->frame->page = NULL;
		free(page->frame);
		page->frame = NULL;
	}
}
