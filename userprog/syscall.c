#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include <string.h>
struct lock filesys_lock;

void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
bool create(const char *file, unsigned initial_size);
tid_t fork(const char *thread_name, struct intr_frame *f);
bool remove(const char *file);
int exec(const char *file_name);
int filesize(int fd);
void close(int fd);
int wait(tid_t pid);
void seek(int fd, unsigned position);
int tell(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the syscall instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
/* Predefined file handles. */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1

void check_address(void *addr)
{
    if (addr == NULL || is_kernel_vaddr(addr))
        exit(-1);

    struct thread *t = thread_current();
    struct page *page = spt_find_page(&t->spt, addr);

    if (page == NULL)
    {
#ifdef VM
        // 📌 stack growth 예외 허용
        void *rsp = is_user_vaddr(addr) ? t->rsp : NULL; // syscall이면 rsp 저장돼 있음
        if (rsp == NULL || !is_stack_access(addr, rsp))
        {
            exit(-1);
        }
#else
        exit(-1);
#endif
    }
}
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset)
{
    if (addr == NULL || addr != pg_round_down(addr))
        return NULL;
    if (offset % PGSIZE != 0)
        return NULL;
    if (!is_user_vaddr(addr) || !is_user_vaddr(addr + length))
        return NULL;
    if (spt_find_page(&thread_current()->spt, addr)) // 얘도 문제일 수 있어
        return NULL;
    if (fd <= 2) // 얘도 1로
        return NULL;

    struct file *f = process_get_file(fd);
    if (f == NULL || file_length(f) == 0 || (int)length <= 0)
        return NULL;

    return do_mmap(addr, length, writable, f, offset);
}
void munmap(void *addr)
{
    do_munmap(addr);
}
void syscall_init(void)
{
    write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
                            ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK,
              FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
    lock_init(&filesys_lock);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
    int syscall_n = f->R.rax;
#ifdef VM
    thread_current()->rsp = f->rsp; // 💡 유저 모드 rsp 백업
#endif

    switch (f->R.rax)
    {
    case SYS_HALT:
        halt(); // 핀토스 종료
        break;
    case SYS_EXIT:
        exit(f->R.rdi); // 프로세스 종료
        break;
    case SYS_FORK:
        f->R.rax = fork(f->R.rdi, f);
        break;
    case SYS_EXEC:
        f->R.rax = exec(f->R.rdi);
        break;
    case SYS_WAIT:
        f->R.rax = process_wait(f->R.rdi);
        break;
    case SYS_CREATE:
        f->R.rax = create(f->R.rdi, f->R.rsi);
        break;
    case SYS_REMOVE:
        f->R.rax = remove(f->R.rdi);
        break;
    case SYS_OPEN:
        f->R.rax = open(f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = filesize(f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_SEEK:
        seek(f->R.rdi, f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = tell(f->R.rdi);
        break;
    case SYS_CLOSE:
        close(f->R.rdi);
        break;
    case SYS_MMAP:
        f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
        break;
    case SYS_MUNMAP:
        munmap(f->R.rdi);
        break;
    default:
        exit(-1);
    }
}

void halt(void)
{
    power_off();
}

void exit(int status)
{
    struct thread *curr = thread_current();
    curr->exit_status = status;

    // Print termination message
    printf("%s: exit(%d)\n", thread_name(), status);

    thread_exit();
}

int write(int fd, const void *buffer, unsigned size)
{
    check_address(buffer);

    off_t bytes = -1;

    if (fd <= 0) // stdin에 쓰려고 할 경우 & fd 음수일 경우
        return -1;

    if (fd < 3)
    { // 1(stdout) * 2(stderr) -> console로 출력
        putbuf(buffer, size);
        return size;
    }

    struct file *file = process_get_file(fd);

    if (file == NULL)
        return -1;

    lock_acquire(&filesys_lock);
    bytes = file_write(file, buffer, size);
    lock_release(&filesys_lock);

    return bytes;
}

bool create(const char *file, unsigned initial_size)
{
    lock_acquire(&filesys_lock);
    check_address(file);
    bool success = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return success;
}

bool remove(const char *file)
{
    check_address(file);
    return filesys_remove(file);
}

int open(const char *file)
{
    check_address(file);

    struct file *newfile = filesys_open(file);

    if (newfile == NULL)
        goto err;

    int fd = process_add_file(newfile);

    if (fd == -1)
        file_close(newfile);

    return fd;
err:

    return -1;
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
    check_address(thread_name);
    return process_fork(thread_name, f); // 실제 유저 컨텍스트를 넘긴다
}
int read(int fd, void *buffer, unsigned size)
{
    check_address(buffer);
    lock_acquire(&filesys_lock);
    if (fd == 0)
    {              // 0(stdin) -> keyboard로 직접 입력
        int i = 0; // 쓰레기 값 return 방지
        char c;
        unsigned char *buf = buffer;

        for (; i < size; i++)
        {
            c = input_getc();
            *buf++ = c;
            if (c == '\0')
                break;
        }
        lock_release(&filesys_lock);
        return i;
    }
    // 그 외의 경우
    if (fd < 3) // stdout, stderr를 읽으려고 할 경우 & fd가 음수일 경우
    {
        lock_release(&filesys_lock);
        return -1;
    }

    struct file *file = process_get_file(fd);
    off_t bytes = -1;

    if (file == NULL) // 파일이 비어있을 경우
    {
        lock_release(&filesys_lock);
        return -1;
    }

#ifdef VM
    struct page *page = spt_find_page(&thread_current()->spt, buffer);
    if (page && !page->writable)
    {
        lock_release(&filesys_lock);
        exit(-1);
    }
#endif
    bytes = file_read(file, buffer, size);
    lock_release(&filesys_lock);

    return bytes;
}

// 파일 디스크럽터를 사용하여 파일의 크기를 가져오는 함수
int filesize(int fd)
{
    struct file *file = process_get_file(fd); // 파일 포인터

    if (file == NULL)
    {
        return -1;
    }

    return file_length(file); // 파일의 크기를 반환함
}

int exec(const char *file_name)
{
    check_address(file_name);

    off_t size = strlen(file_name) + 1;
    char *cmd_copy = palloc_get_page(PAL_ZERO);

    if (cmd_copy == NULL)
        return -1;

    memcpy(cmd_copy, file_name, size);

    if (process_exec(cmd_copy) == -1)
        return -1;

    return 0; // process_exec 성공시 리턴 값 없음 (do_iret)
}

// 열려있는 파일 디스크립터 fd의 파일 포인터를 position으로 이동시키는 함수
void seek(int fd, unsigned position)
{
    struct file *file = process_get_file(fd);

    if (fd < 3 || file == NULL)
        return;

    file_seek(file, position);
}

// fd에서 다음에 읽거나 쓸 바이트의 위치를 반환하는 함수
int tell(int fd)
{
    struct file *file = process_get_file(fd);

    if (fd < 3 || file == NULL)
        return -1;

    return file_tell(file);
}

// Close file descriptor fd.
// Use void file_close(struct file *file).
void close(int fd)
{
    struct file *file = process_get_file(fd);

    if (fd < 3 || file == NULL)
        return;

    process_close_file(fd);

    file_close(file);
}

int wait(tid_t pid)
{
    return process_wait(pid);
};