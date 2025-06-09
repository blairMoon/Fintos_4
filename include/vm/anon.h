#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page
{
     /* 25.05.30 정진영 작성
     음수이면 스왑 아웃 상태가 아님 */
     int swap_idx;
     size_t page_no;
};

void vm_anon_init(void);
bool anon_initializer(struct page *page, enum vm_type type, void *kva);

#endif
