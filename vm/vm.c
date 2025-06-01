/* vm.c: 
 * 가상 메모리(Virtual Memory) 전반에 대한 인터페이스를 제공
 */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"

/* 25.05.30 고재웅 작성 */
#include <hash.h>
#include "threads/vaddr.h"

/* 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을 초기화합니다. */
void 
vm_init (void)
{
	list_init(&frame_table);	/* 25.05.30 고재웅 작성 */
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* 이 위쪽은 수정하지 마세요 !! */
	/* TODO: 이 아래쪽부터 코드를 추가하세요 */
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

/* 초기화 함수와 함께 대기 중인 페이지 객체를 생성합니다. 페이지를 직접 생성하지 말고,
 * 반드시 이 함수나 `vm_alloc_page`를 통해 생성하세요. */
bool 
vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화 파일을 가져옵니다.Add commentMore actions
		 * TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		 * TODO: uninit_new를 호출한 후 필드를 수정해야 합니다. */
		page = malloc(sizeof(struct page));
		if (page == NULL) {
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

		if (!spt_insert_page(spt, page)) {
			return true;
		}
	}
	else
		return false;
err:
	free(page);
	return false;
}

/* va → struct page *를 찾는 함수
 * 가상 주소를 통해 SPT에서 페이지를 찾아 리턴합니다.
 * 에러가 발생하면 NULL을 리턴하세요. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;

	/* 25.05.30 고재웅 작성 */
	struct page temp;
	temp.va = pg_round_down(va);
	struct hash_elem *e = hash_find(&spt->pages, &temp.hash_elem);
	if (e == NULL)
		return NULL;
	return hash_entry(e, struct page, hash_elem);
}

/* 주어진 PAGE를 spt에 삽입하며 검증을 수행합니다. 
 * 이미 해당 가상 주소가 등록되어 있다면 실패 처리해야 합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) 
{
	/* 25.05.30 정진영 작성 */
	/* hash_insert는 중복 키가 존재하면 기존 요소를 반환하고, 성공적으로 삽입되면 NULL을 반환한다. */
	return hash_insert(&spt->pages, &page->hash_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) 
{
	vm_dealloc_page (page);

	/** TODO: page 해제
	 * 매핑된 프레임을 해제해야하나?
	 * 프레임이 스왑되어있는지 체크할것?
	 * 아마 pml4_clear_page 사용하면 된대요
	 */

	return true;
}

/* 교체될 struct frame을 가져옵니다. */
static struct frame *
vm_get_victim (void) 
{
	struct frame *victim = NULL;
	/* TODO: 교체 정책을 여기서 구현해서 희생자 페이지 찾기 */

	return victim;
}

/* 한 페이지를 교체(evict)하고 해당 프레임을 반환합니다.
 * 에러가 발생하면 NULL을 반환합니다.*/
static struct frame *
vm_evict_frame (void) 
{
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	/* TODO: 여기서 swap_out 매크로를 호출??
	 *	pml4_clear_page를 아마 사용?? (잘 모름)
	 */
	return NULL;
}

/* palloc()을 사용하여 프레임을 할당합니다.
 * 사용 가능한 페이지가 없으면 페이지를 교체(evict)하여 반환합니다.
 * 이 함수는 항상 유효한 주소를 반환합니다. 즉, 사용자 풀 메모리가 가득 차면,
 * 이 함수는 프레임을 교체하여 사용 가능한 메모리 공간을 확보합니다.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	/* 25.05.30 고재웅 작성 */

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

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	/* 25.05.30 정진영 작성
	 * 스택 최하단에 익명 페이지를 추가하여 사용
	 * addr은 PGSIZE로 내림(정렬)하여 사용 */
	vm_alloc_page(VM_ANON, addr, true); // 스택 최하단에 익명 페이지 추가
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) 
{
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) 
{
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
vm_dealloc_page (struct page *page) 
{
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 요구합니다 . */
bool
vm_claim_page (void *va UNUSED) 
{
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page (page);
}

/* PAGE를 요구하고 mmu를 설정합니다*/
static bool
vm_do_claim_page (struct page *page) 
{
	struct frame *frame = vm_get_frame ();
	/* TODO: vm_get_frame이 실패하면 swap_out */
	
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

/* 프로세스가 시작될 때(initd) or 포크될 때(__do_fork) 호출되는 함수 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) 
{
	/* SPT 초기화시 hash_init에 아래 작성한 page_hash, page_less를 포함한다. */
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* SPT 해시 테이블에 넣기 위한 hash_func & less_func 함수 구현 */

/* page_hash 가상 주소를 바탕으로 해시값을 계산한다. */
uint64_t page_hash(const struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

/* 두 page의 va를 기준으로 정렬을 비교한다. */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	/* 이 함수는 해시 테이블 충돌 시 내부 정렬에 사용된다고 한다. */
	struct page *pa = hash_entry(a, struct page, hash_elem);
	struct page *pb = hash_entry(b, struct page, hash_elem);
	return pa->va < pb->va;
}

/* 주로 fork() 혹은 __do_fork() 시, 자식도 부모처럼 똑같은 주소 공간을 갖기 위해 SPT를 복사
 * hash_first(), hash_next()를 써서 src의 모든 페이지를 순회하면서 dst에 복사해야 합니다. */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) 
{
	/* 	🟪 TODO 
	복사 시:
	•	page_get_type()으로 타입 확인
	•	vm_alloc_page_with_initializer()로 새 페이지 생성
	•	claim_page() 후 실제 물리 메모리 복사 

	단순 복사는 struct page만 (init 정보), 내용 복사는 struct page + 메모리의 내용 (frame or swap) 까지
	내용 복사 하려면 frame 할당이랑 memcpy 필요함

	- uninit lazy page: 아직 실제 데이터가 로딩되지 않음 -> 단순 복사 (init 정보만 복사)
	- frame이 있는 페이지: 이미 메모리에 올라감 -> 실제 메모리 내용도 복사해야 함
	- 스왑된 페이지: 디스크에만 있음 -> 복원 후 복사하거나 스왑 슬롯 공유 고려

	page->frame == NULL이면 실제로 메모리에 올라오지 않음. 
	즉, 아직 page fault가 발생하지 않은 lazy-load 페이지
	혹은 swap-out 되어 메모리에는 없는 페이지
	이런 경우는 데이터 복사 불필요 -> vm_alloc_page_with_initializer()로 초기 정보만 복사해서 동일한 lazy loading 조건을 만들면 됨!!!
	*/
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) 
{
	/* TODO: 스레드가 보유한 모든 supplemental_page_table을 제거하고,
	 * TODO: 수정된 내용을 스토리지에 기록(writeback)하세요. */

	/* 	🟪 TODO 
	hash_destroy() 사용
	•	각 페이지에 대해 destroy(page) 호출해서:
	•	swap out 필요 시 writeback
	•	프레임 반환
	•	구조체 메모리 해제 */
}