/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/* 25.05.30 고재웅 작성 */
#include <hash.h>
#include "threads/vaddr.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	list_init(&frame_table);
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화 파일을 가져옵니다.
		* TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		* TODO: uninit_new를 호출한 후 필드를 수정해야 합니다. */
		page = malloc(sizeof(struct page));
		if (page == NULL){
			return false;
		}
		bool (*page_initializer) (struct page *, enum vm_type, void *kva) = NULL;

		switch (VM_TYPE(type))
		{
			case VM_ANON:
				page_initializer = anon_initializer;
				break;
			case VM_FILE:
				page_initializer = file_backed_initializer;
				break;
			default:
				goto err;
		}
		/* TODO: spt에 페이지를 삽입합니다. */
		uninit_new(page, upage, init, type, aux, page_initializer);

		page->writable = writable;

		if (!spt_insert_page(spt, page)){
			return true;
		}
	}
	else
		return false;
err:
	free(page);
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	/* 25.05.30 고재웅 작성 */
	struct page temp;
	temp.va = pg_round_down(va);
	struct hash_elem *e = hash_find(&spt->pages, &temp.hash_elem);
	if (e == NULL)
		return NULL;
	return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */

	/* 25.05.30 고재웅 작성 */
	// 먼저 페이지 테이블에서 가상 주소가 존재하지 않는지 검사한다.
	struct hash_elem *prev_elem = hash_find(&spt->pages, &page->hash_elem);
	if (prev_elem == NULL){
	  	// 페이지 테이블에 페이지 구조체를 삽입한다.
		if (hash_insert(&spt->pages, &page->hash_elem)){
			succ = true;
			return succ;
		}
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	// 1. 유저 풀에서 새로운 페이지 할당
	void *kva = palloc_get_page(PAL_USER);

	// 2. 할당 실패 시 PANIC
	if (kva == NULL) {
		PANIC("todo: implement eviction here");
	}

	// 3. 프레임 구조체 할당 및 초기화
	frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
		PANIC("frame allocation failed");
	}

	frame->kva = kva;
	frame->page = NULL;  // 아직 연결된 페이지 없음
	frame->owner = thread_current();  // 현재 스레드를 소유자로 설정

	// 4. 전역 frame_table에 프레임 등록
	list_push_back(&frame_table, &frame->elem);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 1. 주소 유효성 검사
	if (addr == NULL || is_kernel_vaddr(addr)) {
		return false;
	}

	// 2. 접근 권한 검사
	if (!not_present) {
		// 존재하는 페이지에 대해 쓰기 금지 등
		return false;
	}

	// 3. 주소를 페이지 기준으로 내림 (page alignment)
	void *page_va = pg_round_down(addr);

	// 4. 보조 페이지 테이블에서 해당 페이지 찾기
	page = spt_find_page(spt, page_va);
	if (page == NULL) {
		// 스택 확장을 고려해 여기서 vm_stack_growth 등을 호출할 수도 있음
		return false;
	}

	// 5. 쓰기 접근인데 읽기 전용 페이지면 오류
	if (write && !page->writable) {
		return false;
	}

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 페이지의 VA와 프레임의 KVA를 페이지 테이블에 매핑 */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
/* 25.05.30 고재웅 작성 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* SPT 초기화시 hash_init에 아래 작성한 page_hash, page_less를 포함한다. */
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

/* 25.05.30 고재웅 작성 */
/* SPT 해시 테이블에 넣기 위한 hash_func & less_func 함수 구현 */

/* page_hash 가상 주소를 바탕으로 해시값을 계산한다. */
uint64_t page_hash(const struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p, sizeof(p->va));
}

/* 두 page의 va를 기준으로 정렬을 비교한다. */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	/* 이 함수는 해시 테이블 충돌 시 내부 정렬에 사용된다고 한다. */
	struct page *pa = hash_entry(a, struct page, hash_elem);
	struct page *pb = hash_entry(b, struct page, hash_elem);
	return pa->va < pb->va;
}
