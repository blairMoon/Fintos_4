/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "lib/round.h" //  ROUND_UP, DIV_ROUND_UP

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
		.swap_in = file_backed_swap_in,
		.swap_out = file_backed_swap_out,
		.destroy = file_backed_destroy,
		.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	page->operations = &file_ops;
	struct file_page *file_page UNUSED = &page->file;
	struct lazy_load_arg *aux = page->uninit.aux;
	file_page->file = aux->file;
	file_page->ofs = aux->ofs;
	file_page->read_bytes = aux->read_bytes;
	file_page->zero_bytes = aux->zero_bytes;
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */

void *
do_mmap(void *addr, size_t length, int writable,
				struct file *file, off_t offset)
{
	ASSERT(addr != NULL);
	ASSERT(pg_ofs(addr) == 0);		// addr is page-aligned
	ASSERT(offset % PGSIZE == 0); // offset is page-aligned

	struct file *f = file_reopen(file);
	if (f == NULL)
		return NULL;

	void *start_addr = addr;

	// 실제 읽을 파일 길이: 요청한 length vs 파일 크기 중 작은 값
	size_t file_len = file_length(f);
	size_t read_bytes = length < file_len ? length : file_len;
	size_t zero_bytes = ROUND_UP(length, PGSIZE) - read_bytes;

	int total_page_count = DIV_ROUND_UP(length, PGSIZE);

	while (read_bytes > 0 || zero_bytes > 0)
	{
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		// lazy_load_segment에 넘겨줄 보조 데이터 구성
		struct lazy_load_arg *aux = malloc(sizeof(struct lazy_load_arg));
		if (!aux)
			return NULL;
		aux->file = f;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux))
			return NULL;

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}

	// 첫 번째 page에 mapped_page_count 저장 (unmap 시 참조)
	struct page *first_page = spt_find_page(&thread_current()->spt, start_addr);
	if (first_page == NULL)
		return NULL;
	first_page->mapped_page_count = total_page_count;

	return start_addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *p = spt_find_page(spt, addr);
	int count = p->mapped_page_count;

	for (int i = 0; i < count; i++)
	{
		if (p)
			destroy(p); // 내부적으로 file_backed_destroy를 호출하게 됨
		addr += PGSIZE;
		p = spt_find_page(spt, addr);
	}
}
