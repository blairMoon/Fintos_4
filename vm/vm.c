/* vm.c: 
 * 가상 메모리(Virtual Memory) 전반에 대한 인터페이스를 제공
 * 
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

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* 이미 해당 page가 SPT에 존재하는지 확인합니다. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: VM 타입에 따라 페이지를 생성하고, 초기화 함수를 가져온 뒤,
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성하세요.
		 * TODO: uninit_new 호출 후에는 필요한 필드를 수정해야 합니다. */

		// /* 25.05.30 정진영 작성 */ 
		// bool (*page_initializer)(struct page *, enum vm_type, void *kva);
		// struct page *page = malloc(sizeof(struct page));
		// page->writable = writable;

		// switch (VM_TYPE(type))
		// {
		// case VM_ANON:
		// 	page_initializer = anon_initializer;
		// 	break;
		// case VM_MMAP:
		// 	/* 매핑 카운트를 추가해두자
		// 	   mmap_list로 mmap 페이지를 관리할거면 필요 x */
		// case VM_FILE:
		// 	page_initializer = file_backed_initializer;
		// 	break;
		// default:
		// 	free(page);
		// 	goto err;
		// 	break;
		// }

		uninit_new(page, upage, init, type, aux, page_initializer);
		/* TODO: 생성한 페이지를 spt에 삽입하세요. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* 가상 주소를 통해 SPT에서 페이지를 찾아 리턴합니다.
 * 에러가 발생하면 NULL을 리턴하세요. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) 
{
	struct page *page = NULL;
	/* TODO: Fill this function. */

	/* 25.05.30 고재웅 작성 */
	page.va = pg_round_down(va);
	// 보조 테이블에서 va가 포함된 구조체를 찾는다.
  	struct hash_elem *find_elem = hash_find(&spt->pages, page->hash_elem);
	// 찾지 못했다면 NULL 반환
	if (find_elem == NULL){
		return NULL;
	}
	// 구조체를 찾으면 구조체를 반환
	return hash_entry(find_elem, struct page, hash_elem);
}

/* PAGE를 spt에 삽입하며 검증을 수행합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) 
{
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
	/*
	 * 여기서 swap_out을 진행해야 합니다.
	 * pml4_clear_page를 사용해서 물리 주소를 클리어 합니다.
	 */

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

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
/* 25.05.30 고재웅 작성 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) 
{
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) 
{
	/* TODO: 스레드가 보유한 모든 supplemental_page_table을 제거하고,
	 * TODO: 수정된 내용을 스토리지에 기록(writeback)하세요. */
}

/* 25.05.30 고재웅 작성 */
/* SPT 해시 테이블에 넣기 위한 hash_func & less_func 함수 구현 */

/* page_hash 가상 주소를 바탕으로 해시값을 계산한다. */
uint64_t page_hash(onst struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p, sizeof(p->va));
}

/* 두 page의 va를 기준으로 정렬을 비교한다. */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	/* 이 함수는 해시 테이블 충돌 시 내부 정렬에 사용된다고 한다. */
	struct page *pa = hash_entry(a, struct page, hash_elem);
	struct page *pb = hash_entry(b, struct page, hesh_elem);
	return pa->va < pb->va;
}
