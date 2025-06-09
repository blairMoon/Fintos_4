/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "lib/round.h" //  ROUND_UP, DIV_ROUND_UP
#include <string.h>
#include <stdlib.h>
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
	file_page->zero_bytes = lazy_load_arg->zero_bytes;

	return true;
}

/* Swap in the page by read contents from the file. */
// static bool
// file_backed_swap_in(struct page *page, void *kva)
// {
// 	page->operations = &file_ops;
// 	struct file_page *file_page UNUSED = &page->file;
// 	struct lazy_load_arg *aux = page->uninit.aux;
// 	file_page->file = aux->file;
// 	file_page->ofs = aux->ofs;
// 	file_page->read_bytes = aux->read_bytes;
// 	file_page->zero_bytes = aux->zero_bytes;
// 	return true;
// }

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
	/** Project 3-Swap In/Out */
	lock_acquire(&filesys_lock);
	int read = file_read_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->ofs);
	lock_release(&filesys_lock); // 🔓

	memset(page->frame->kva + read, 0, PGSIZE - read);
	return true;
}
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page = &page->file;
	struct frame *frame = page->frame;

	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		lock_acquire(&filesys_lock); // 🔒 반드시 락 걸기

		file_write_at(file_page->file, frame->kva,
									file_page->read_bytes, file_page->ofs);

		lock_release(&filesys_lock); // 🔓 해제

		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}

	frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	// page struct를 해제할 필요는 없습니다. (file_backed_destroy의 호출자가 해야 함)
	struct file_page *file_page UNUSED = &page->file;
	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		lock_acquire(&filesys_lock);
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		lock_release(&filesys_lock);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
				struct file *file, off_t offset)
{

	// TODO: 2. fd에 대응하는 struct file * 구하기
	// - 열린 파일 디스크립터 테이블에서 찾고, 실패 시 NULL 반환
	// - file을 reopen하여 별도 참조를 유지 (중복 닫힘 방지)
	lock_acquire(&filesys_lock);
	struct file *f = file_reopen(file);
	lock_release(&filesys_lock);

	if (file == NULL)
	{
		return NULL;
	}

	int total_page_count = length / PGSIZE;
	if (length % PGSIZE != 0)
		total_page_count += 1;
	void *start_addr = addr;

	/* 여는 파일이 length보다 작으면 그냥 file_length 사용
	 * 만약 5000바이트 짜리를 매핑해야 한다면 첫 페이지에 4096바이트 두번째 페이지에 904 바이트를 읽고
	 * 나머지 3192 바이트는 0으로 채워야 한다.
	 */
	size_t read_bytes = file_length(f) < length ? file_length(f) : length;
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	// TODO: 3. 파일 길이 검사 및 전체 매핑 길이 조정
	// - 파일 길이를 구하고, 파일 끝까지 매핑 가능한지 확인
	// - length가 파일 길이보다 크면 남는 부분은 0으로 채우도록 기록
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0); // 전체 매핑 크기가 페이지 크기의 배수여야 함을 보장한다.
	ASSERT(pg_ofs(addr) == 0);											 // 페이지 오프셋이 0 즉 page_aligned address 임을 보장
	ASSERT(offset % PGSIZE == 0);										 // 파일 내의 오프셋 ofs도 페이지 크기의 배수여야한다.

	// TODO: 4. 페이지 단위로 loop를 돌며 각 가상 페이지를 uninit으로 등록
	// - vm_alloc_page_with_initializer() 사용
	// - 이 때 lazy_load_file을 initializer로 넘김
	// - struct file_info(aux)에 file, offset, read_bytes 등 저장

	// TODO: 5. 모든 페이지가 성공적으로 매핑되었으면 addr 반환
	// - 실패 시 중간에 등록한 페이지들을 모두 해제하고 NULL 반환
	int mapped_pages = 0;

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
		lazy_load_arg->file = f;										 // 내용이 담긴 파일 객체
		lazy_load_arg->ofs = offset;								 // 이 페이지에서 읽기 시작할 위치
		lazy_load_arg->read_bytes = page_read_bytes; // 이 페이지에서 읽어야 하는 바이트 수
		lazy_load_arg->zero_bytes = page_zero_bytes; // 이 페이지에서 read_bytes만큼 읽고 공간이 남아 0으로 채워야 하는 바이트 수
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, lazy_load_arg))
		{
			free(lazy_load_arg);
			for (int i = 0; i < mapped_pages; i++)
			{
				void *rollback_addr = start_addr + i * PGSIZE;
				struct page *rollback_page = spt_find_page(&thread_current()->spt, rollback_addr);
				if (rollback_page)
					spt_remove_page(&thread_current()->spt, rollback_page);
			}
			return NULL;
		}
		mapped_pages++;
		struct page *p = spt_find_page(&thread_current()->spt, start_addr);
		p->mapped_page_count = total_page_count;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
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
			destroy(p);
		addr += PGSIZE;
		p = spt_find_page(spt, addr);
	}
}