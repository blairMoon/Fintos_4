#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page
{
	struct file *file; // 매핑된 파일 포인터
	off_t ofs;				 // 파일에서 읽기 시작할 오프셋
	size_t read_bytes; // 파일에서 실제 읽어야 할 바이트 수
	size_t zero_bytes; // 0으로 채울 바이트 수
};
struct lock filesys_lock;

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
							struct file *file, off_t offset);
void do_munmap(void *va);
#endif
