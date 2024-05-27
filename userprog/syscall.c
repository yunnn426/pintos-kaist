#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

// #include <process.h>
// #include <filesys.h>

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

/* my macro */
#define FAIL -1 

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	/* init global lock
		for file descriptor */
	// lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	
	/* syscall number */
	uint64_t syscall_number = f->R.rax;
	// printf("syscall number: %d\n", syscall_number);

	/* argument가 레지스터에 저장되는 순서: 
		%rdi, %rsi, %rdx, %r10, %r8, and %r9 */

	switch (syscall_number)
	{
	case SYS_HALT:
		/* code */
		halt();
		break;

	case SYS_EXIT:
	{
		int status = f->R.rdi;
		exit(status);
		break;
	}
	
	case SYS_EXEC:
	{
		char *file = f->R.rdi;
		f->R.rax = exec(file);
		break;
	}

	case SYS_CREATE:
	{
		const char *file = (const char *)f->R.rdi;
		unsigned initial_size = (unsigned)f->R.rsi;
		f->R.rax = create(file, initial_size);
		break;
	}
	
	case SYS_REMOVE:
	{
		char *file = f->R.rdi;
		f->R.rax = remove(file);
		break;
	}

	case SYS_OPEN:
	{
		char *file = f->R.rdi;
		f->R.rax = open(file);
		break;
	}

	case SYS_WRITE:
	{
		int fd = f->R.rdi;
		void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;
		f->R.rax = write(fd, buffer, size);
		break;
	}

	default:
		// thread_exit ();
		break;
	}

}

void
halt() {
	power_off();
}

/* 프로세스명과 상태를 출력하고 종료한다. */
void 
exit(int status) {

	struct thread *t = thread_current();
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}

/* 프로세스를 실행시킨다.
	cmd_line으로 받아온 프로그램명 혹은 인자들을 전달한다. */
int 
exec (const char *cmd_line) {
	check_address(cmd_line);
	int result = process_exec(cmd_line);

	return result;
}

/* "이름: file, 사이즈: initial_size"
	의 파일을 생성한다.
	성공 여부를 반환한다. */
bool
create (const char *file, unsigned initial_size) {
	check_address(file);
	if (initial_size < 0) 
		return false;
		
	// printf("Creating file: %s, initial_size: %u\n", file, initial_size);
	bool result = filesys_create(file, initial_size);
	// printf("Create success: %d\n", result);

	return result;
}

/* "이름: file" 
	의 파일을 삭제한다. 
	성공 여부를 반환한다. */
bool 
remove (const char *file) {
	check_address(file);
	bool result = filesys_remove(file);

	return false;
}

/* 유저 영역을 벗어난 경우 프로세스를 종료한다. */
// #define USERPROG
void 
check_address(void *addr) {
	struct thread *t = thread_current();

	/* 주소가 NULL이면 exit */
	if (addr == NULL)
		exit(FAIL);
	/* user virtual memory가 매핑이 되었는지 확인 */
	if (pml4_get_page(t->pml4, addr) == NULL)
		exit(FAIL);
	/* 커널 영역을 접근하려 하면 exit */
	if (is_kernel_vaddr(addr))
		exit(FAIL);
}

/* "이름: file" 
	의 파일을 연다. */
int 
open (const char *file) {
	check_address(file);
	struct file *f = filesys_open(file);
	int fd = process_add_file(f);

	return fd;
}

/* fd 식별자를 갖는 파일의 크기를 바이트 단위로 반환한다.*/
int 
filesize (int fd) {

}

/* 열려있는 fd 식별자의 파일에 대해
	size 바이트만큼 buffer의 내용을 쓴다.
	 */
int 
write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);

	struct thread *curr = thread_current();
	struct file *f = curr->fdt[fd];

	/* stdout */
	if (fd == 1) {
		putbuf(buffer, size);

		return size;
	}
	/* write in file */
	else {
		int bytes_written = file_write(f, buffer, size);

		return bytes_written;
	}
}