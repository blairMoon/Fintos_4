/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "lib/round.h" //  ROUND_UP, DIV_ROUND_UP
#include "filesys/file.h"

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
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)page->uninit.aux;

	file_page->file = lazy_load_arg->file;
	file_page->ofs = lazy_load_arg->ofs;
	file_page->read_bytes = lazy_load_arg->read_bytes;

	return true;
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
void file_backed_destroy(struct page *page)
{
	struct file_page *file_page = &page->file;

	// dirty면 파일에 다시 쓰기
	if (page->frame != NULL &&
			pml4_is_dirty(thread_current()->pml4, page->va))
	{
		file_write_at(file_page->file,
									page->frame->kva,
									file_page->read_bytes,
									file_page->ofs);
	}

	// 프레임 해제 (함수 안 만들고 직접 처리)
	if (page->frame != NULL)
	{
		palloc_free_page(page->frame->kva); // 물리 페이지 반환
		free(page->frame);									// 프레임 구조체 반환
		page->frame = NULL;
	}
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
				struct file *file, off_t offset)
{
	// 초기 정보 출력
	// printf("[do_mmap] called: addr=%p, length=%zu, offset=%d, writable=%d\n", addr, length, offset, writable);

	// NULL 주소일 경우 처리: 스택 기준 자동 할당
	if (addr == NULL)
	{
		addr = pg_round_down((void *)(USER_STACK - length));
		// printf("[do_mmap] addr was NULL, assigned to %p\n", addr);
	}

	// 정렬 조건 검사
	if (!addr || pg_round_down(addr) != addr || is_kernel_vaddr(addr) || is_kernel_vaddr(addr + length))
	{
		// printf("[do_mmap] addr is invalid (alignment or kernel address)\n");
		return NULL;
	}

	if (offset % PGSIZE != 0)
	{
		// printf("[do_mmap] offset is not page-aligned\n");
		return NULL;
	}

	struct file *f = file_reopen(file);
	if (f == NULL)
	{
		// printf("[do_mmap] file_reopen failed\n");
		return NULL;
	}

	size_t file_len = file_length(f);
	size_t read_bytes = length < file_len ? length : file_len;
	size_t zero_bytes = ROUND_UP(length, PGSIZE) - read_bytes;
	int total_page_count = DIV_ROUND_UP(length, PGSIZE);

	void *start_addr = addr;

	// 페이지 단위로 매핑 수행
	while (read_bytes > 0 || zero_bytes > 0)
	{
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct lazy_load_arg *aux = malloc(sizeof(struct lazy_load_arg));
		if (!aux)
		{
			// printf("[do_mmap] aux malloc failed\n");
			return NULL;
		}

		aux->file = f;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux))
		{
			// printf("[do_mmap] vm_alloc_page_with_initializer failed at addr=%p\n", addr);
			free(aux);
			return NULL;
		}

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}

	// 첫 페이지 찾아서 page_count 저장
	struct page *first_page = spt_find_page(&thread_current()->spt, start_addr);
	if (first_page == NULL)
	{
		// printf("[do_mmap] failed to find first page at %p\n", start_addr);
		return NULL;
	}
	first_page->mapped_page_count = total_page_count;

	// 성공 로그
	// printf("[do_mmap] success: start_addr=%p, total_pages=%d\n", start_addr, total_page_count);
	return start_addr;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *p = spt_find_page(spt, addr);
	if (!p)
		return;

	int count = p->mapped_page_count;

	for (int i = 0; i < count; i++)
	{
		if (p != NULL)
		{
			spt_remove_page(spt, p); // 이 시점에서 p는 제거됨
		}
		addr += PGSIZE;
		p = spt_find_page(spt, addr); // 이미 제거된 페이지는 NULL로 나올 것
	}
}