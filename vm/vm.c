/* vm.c:
 * 가상 메모리(Virtual Memory) 전반에 대한 인터페이스를 제공
 */

#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "userprog/process.h"
/* 25.05.30 고재웅 작성 */
#include <hash.h>
#include "threads/vaddr.h"
static struct lock frame_table_lock;
struct lock filesys_lock;
struct list frame_table;

/* 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을 초기화합니다. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* 이 위쪽은 수정하지 마세요 !! */
	/* TODO: 이 아래쪽부터 코드를 추가하세요 */
	list_init(&frame_table); /* 25.05.30 고재웅 작성 */
	lock_init(&frame_table_lock);
}

/* 페이지의 타입을 가져옵니다. 이 함수는 페이지가 초기화된 후 타입을 알고 싶을 때 유용합니다.
 * 이 함수는 이미 완전히 구현되어 있습니다. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);
struct frame *vm_get_victim(void)
{
	// TODO: 나중에 구현
	return NULL;
}

/* 25.06.01 고재웅 작성
 * 초기화 함수와 함께 대기 중인 페이지 객체를 생성한다. 페이지를 직접 생성하지 말고,
 * 반드시 이 함수나 `vm_alloc_page`를 통해 생성하라.
 * 페이지를 할당하고 타입을 uninit으로 설정한다.
 * */
void supplemental_page_table_init(struct supplemental_page_table *spt)
{
	hash_init(&spt->pages, page_hash, page_less, NULL);
}
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
																		vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화 파일을 가져옵니다.Add commentMore actions
		 * TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		 * TODO: uninit_new를 호출한 후 필드를 수정해야 합니다. */
		struct page *p = (struct page *)malloc(sizeof(struct page));

		if (p == NULL)
		{
			return false;
		}

		bool (*page_initializer)(struct page *, enum vm_type, void *);

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		default:
			free(p);
			return false;
		}

		/* TODO: spt에 페이지를 삽입합니다. */
		uninit_new(p, upage, init, type, aux, page_initializer);
		p->writable = writable;

		return spt_insert_page(spt, p);
	}
err:
	return false;
}

/* 25.05.30 고재웅 작성
 * 25.06.02 고재웅 수정
 * malloc으로 page를 임시 할당해서 사용했는데 free를 해야 하니 제거 하고 지역 변수로 잠깐 사용
 * 가상 주소를 통해 SPT에서 페이지를 찾아 리턴한다.
 * 에러가 발생하면 NULL을 리턴 */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{

	/* TODO: Fill this function. */
	struct page key;
	key.va = pg_round_down(va);

	struct hash_elem *e = hash_find(&spt->pages, &key.hash_elem);
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* 25.05.30 고재웅 작성
 * 25.05.30 정진영 수정
 * 25.06.01 고재웅 수정
 * PAGE를 spt에 삽입하며 검증을 수행합다.
 * 가상 주소가 이미 존재하면 삽입하지 않는다. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED, struct page *page UNUSED)
{
	/* TODO: Fill this function. */
	return hash_insert(&spt->pages, &page->hash_elem) == NULL ? true : false;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{

	if (page == NULL)
		return;

	pml4_clear_page(thread_current()->pml4, page->va);
	hash_delete(&spt->pages, &page->hash_elem);

	// 🔥 destroy() 직접 호출 금지!
	vm_dealloc_page(page); // 내부에서 destroy → free 자동으로 처리됨
}

/* 한 페이지를 교체(evict)하고 해당 프레임을 반환합니다.
 * 에러가 발생하면 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	/* TODO: 여기서 swap_out 매크로를 호출??
	 *	pml4_clear_page를 아마 사용?? (잘 모름)
	 */
	return NULL;
}

/* 25.05.30 고재웅 작성
 * palloc()을 사용하여 프레임을 할당합니다.
 * 사용 가능한 페이지가 없으면 페이지를 교체(evict)하여 반환합니다.
 * 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면,
 * 이 함수는 프레임을 교체하여 사용 가능한 메모리 공간을 확보합니다.*/
static struct frame *
vm_get_frame(void)
{
	// 물리 페이지 할당
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO);
	if (kva == NULL)
		return vm_evict_frame(); // 페이지 교체 전략 필요

	// 프레임 구조체 할당
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	ASSERT(frame != NULL);

	frame->kva = kva;
	frame->page = NULL;

	// 프레임 테이블에 등록
	lock_acquire(&frame_table_lock);

	list_push_back(&frame_table, &frame->elem);
	lock_release(&frame_table_lock);

	return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr)
{
	void *stack_bottom = pg_round_down(addr);
	vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, true);
}
/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* 25.06.01 고재웅 작성 */
/* Return true on success
 * 페이지 폴트 핸들러 - 페이지 폴트 발생시 제어권을 전달 받는다.
 * 물리 프레임이 존재하지 않아서 발생한 예외는 not_present 가 true다
 * 그 경우 물리 프레임 할당을 요청하는 vm_do_claim_page를 호출한다.
 * 반대로 not_present 가 false인 경우는 물리 프레임이 할당되어 있지만 폴트가 발생한 것이다.
 * read-only page에 write를 한경우 등 이 때에는 예외 처리를 하면 된다.
 * 그렇다고 해서 not_present가 true인 경우에서 read-only page에 요청을 할 수 있으니 이에
 * 대한 예외를 처리하라
 */

bool is_stack_access(void *addr, void *rsp)
{
	return addr >= rsp - 8 && addr < USER_STACK && addr >= USER_STACK - (1 << 20);
}
bool vm_try_handle_fault(struct intr_frame *f, void *addr,
												 bool user, bool write, bool not_present)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = NULL;

	// 1. 주소 유효성 검사
	if (addr == NULL || is_kernel_vaddr(addr))
		return false;

	// 2. page가 존재하지 않은 경우 (not-present fault)
	if (not_present)
	{
		// 📌 스택 확장 여부 판단
		void *rsp = user ? f->rsp : thread_current()->rsp;

		if (is_stack_access(addr, rsp))
		{
			vm_stack_growth(addr); // 📌 스택 페이지 할당
		}

		// 3. SPT에서 페이지 찾기 → 위에서 stack_growth 했으면 있을 수도 있음
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;

		if (write && !page->writable)
			return false;

		return vm_do_claim_page(page);
	}

	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* 25.06.01 고재웅 수정
 * VA에 해당하는 페이지를 가져온다.
 * 해당 페이지로 vm_do_claim_page를 호출한다. */
bool vm_claim_page(void *va UNUSED)
{
	/* TODO: Fill this function */
	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
	{
		return false;
	}
	return vm_do_claim_page(page);
}

/* 25.06.01 고재웅 수정
 * 인자로 주어진 page에 frame을 할당 한다. --> vm_get_frame()
 * mmu를 설정한다.(pml4) */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();
	/* TODO: vm_get_frame이 실패하면 swap_out */

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 페이지의 VA와 프레임의 KVA를 페이지 테이블에 매핑 */
	struct thread *current = thread_current();

	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable))
	{
		return false;
	}
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
/* 25.05.30 고재웅 작성 */

/* 프로세스가 시작될 때(initd) or 포크될 때(__do_fork) 호출되는 함수 */
bool supplemental_page_table_copy(struct supplemental_page_table *dst, struct supplemental_page_table *src)
{
	struct hash_iterator iter;
	hash_first(&iter, &src->pages);

	while (hash_next(&iter))
	{
		struct page *src_page = hash_entry(hash_cur(&iter), struct page, hash_elem);
		enum vm_type type = src_page->operations->type;
		void *upage = src_page->va;
		bool writable = src_page->writable;

		if (type == VM_UNINIT)
		{
			// UNINIT 페이지는 lazy 로딩을 위해 aux 구조체 복사
			enum vm_type real_type = page_get_type(src_page);
			void *aux = src_page->uninit.aux;

			if (real_type == VM_FILE)
			{
				struct lazy_load_arg *src_aux = aux;
				struct lazy_load_arg *dst_aux = malloc(sizeof(struct lazy_load_arg));
				if (dst_aux == NULL)
					return false;

				dst_aux->file = file_reopen(src_aux->file);
				if (dst_aux->file == NULL)
				{
					free(dst_aux);
					return false;
				}
				dst_aux->ofs = src_aux->ofs;
				dst_aux->read_bytes = src_aux->read_bytes;
				dst_aux->zero_bytes = src_aux->zero_bytes;

				if (!vm_alloc_page_with_initializer(real_type, upage, writable,
																						src_page->uninit.init, dst_aux))
				{
					free(dst_aux);
					return false;
				}
			}
			else
			{
				// anon 등의 나머지 타입 처리
				if (!vm_alloc_page_with_initializer(real_type, upage, writable,
																						src_page->uninit.init, aux))
					return false;
			}
		}

		else
		{
			// 이미 메모리에 올라온 page → anon일 경우만 처리
			if (!vm_alloc_page(type, upage, writable))
				return false;
			if (!vm_claim_page(upage))
				return false;

			struct page *dst_page = spt_find_page(dst, upage);
			if (dst_page == NULL)
				return false;

			// file-backed는 여기서 처리 ❌ (이미 UNINIT로 처리해야 하므로)
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}

	return true;
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: 스레드가 보유한 모든 supplemental_page_table을 제거하고,
	 * TODO: 수정된 내용을 스토리지에 기록(writeback)하세요. */
	hash_clear(&spt->pages, hash_page_destroy);
}

/* 25.05.30 고재웅 작성 */
/* SPT 해시 테이블에 넣기 위한 hash_func & less_func 함수 구현 */

/* page_hash 가상 주소를 바탕으로 해시값을 계산한다. */
uint64_t page_hash(const struct hash_elem *e, void *aux)
{
	const struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

/* 두 page의 va를 기준으로 정렬을 비교한다. */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	/* 이 함수는 해시 테이블 충돌 시 내부 정렬에 사용된다고 한다. */
	struct page *pa = hash_entry(a, struct page, hash_elem);
	struct page *pb = hash_entry(b, struct page, hash_elem);
	return pa->va < pb->va;
}

void hash_page_destroy(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	pml4_clear_page(thread_current()->pml4, page->va);
	destroy(page);
	free(page);
}