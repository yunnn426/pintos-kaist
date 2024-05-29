#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "userprog/process.h"

#include <lib/kernel/console.h>
#include "threads/palloc.h"
#include "lib/string.h"

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
#define STDIN 0
#define STDOUT 1

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

	case SYS_FORK:
	{
		const char *thread_name = f->R.rdi;
		/* 지훈쿤 방식 */
		thread_current()->parent_if = *f;
		f->R.rax = fork(thread_name);

		/* 만도 방식 */
		// f->R.rax = sysfork(thread_name, f);
		break;
	}
	
	case SYS_EXEC:
	{
		char *file = f->R.rdi;
		f->R.rax = exec(file);
		break;
	}

	case SYS_WAIT:
	{
		pid_t pid = f->R.rdi;
		f->R.rax = wait(pid);
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

	case SYS_FILESIZE:
	{
		int fd = f->R.rdi;
		f->R.rax = filesize(fd);
		break;
	}

	case SYS_READ:
	{
		int fd = f->R.rdi;
		const void *buffer = f->R.rsi;
		unsigned size = f->R.rdx;
		f->R.rax = read(fd, buffer, size);
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

	case SYS_SEEK:
	{
		int fd = f->R.rdi;
		unsigned position = f->R.rsi;
		seek(fd, position);
		break;
	}

	case SYS_TELL:
	{
		int fd = f->R.rdi;
		f->R.rax = tell(fd);
		break;
	}

	case SYS_CLOSE:
	{
		int fd = f->R.rdi;
		close(fd);
		break;
	}

	default:
		// thread_exit ();
		break;
	}

}

void
halt(void) {
	power_off();
}

/* 프로세스명과 상태를 출력하고 종료한다. */
void 
exit(int status) {
	struct thread *t = thread_current();
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}

/* 만도 방식 */
/* struct intr_frame을 인자로 받아야 하는 이유? */
/* : 레지스터로부터 인터럽트 프레임을 받지 못하면 이전 문맥의 인터럽트 프레임을 받을 방법이 없다.
	커널쪽 syscall.c(@lib/user)에는 fork(인자 1개)로 시스템 콜이 호출되지만
	중간에 레지스터를 거쳐서 넘어오기 때문에 커널쪽 fork()에서는 인터럽트 프레임을 인자로 받지 않아도 된다. */
// pid_t 
// sysfork (c)

/* 지훈쿤 방식 */
/* syscall_handler에서 받아오는 f가 있으므로 
	case: SYS_FORK에서 fork 호출 전에 부모if 안에 받아온 f를 넣어주었다. */
pid_t 
fork (const char *thread_name) {
	return process_fork(thread_name, &thread_current()->parent_if);
}

/* 프로세스를 실행시킨다.
	cmd_line으로 받아온 프로그램명 혹은 인자들을 전달한다. */
int 
exec (const char *cmd_line) {
	/* process_execute() 함수를 호출하여 자식 프로세스 생성 */
	/* 생성된 자식 프로세스의 프로세스 디스크립터를 검색 */
	/* 자식 프로세스의 프로그램이 적재될 때까지 대기*/
	/* 프로그램 적재 실패 시 -1 리턴*/
	/* 프로그램 적재 성공 시 자식 프로세스의 pid 리턴*/

	check_address(cmd_line);

	/* const char는 수정할 수 없으므로 복사해서 사용한다. */
	char *cmd_line_copy = palloc_get_page(PAL_ZERO);
	if (cmd_line_copy == NULL)
		exit(-1);

	strlcpy(cmd_line_copy, cmd_line, PGSIZE);

	int result = process_exec(cmd_line_copy);
	if (result == -1)
		exit(-1);
		
	return result;
}

int 
wait (pid_t pid) {
	int status = process_wait(pid);

	return status;
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

	return result;
}

/* 유저 영역을 벗어난 경우 프로세스를 종료한다. */
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
	if (f == NULL) 
		return -1;
	int fd = process_add_file(f);
	if (fd == -1)
		exit(-1);

	return fd;
}

/* fd 식별자를 갖는 파일의 크기를 바이트 단위로 반환한다.*/
int 
filesize (int fd) {
	struct thread *curr = thread_current();
	struct file *f = process_get_file(fd);
	if (f)
		return file_length(f);
	return -1;
}

/* fd로 열려있는 파일에 대해
	fd가 STDIN(0)인 경우 키보드에서 읽고, 
	그 외의 경우 파일의 내용을 읽어서 buffer에 쓴다. */
int 
read (int fd, void *buffer, unsigned size) {
	if (fd < 0 || fd > MAX_FD || fd == NULL)
		exit(-1);
	else if (fd == 1)
		return;

	check_address(buffer);

	/* reads from keyboard. */
	if (fd == 0) {
		return input_getc();
	} 

	/* reads fd to buffer. */
	else {
		struct thread *curr = thread_current();
		struct file *f = process_get_file(fd);
	
		return file_read(f, buffer, size);
	}
}

/* 열려있는 fd 식별자의 파일에 대해
	size 바이트만큼 buffer의 내용을 쓴다. */
int 
write (int fd, const void *buffer, unsigned size) {
	if (fd < 0 || fd > MAX_FD || fd == NULL)
		exit(-1);
	else if (fd == 0)
		return;

	check_address(buffer);

	struct thread *curr = thread_current();
	struct file *f = process_get_file(fd);

	/* stdout */
	if (fd == 1) {
		putbuf(buffer, size);		// in console.c

		return size;
	}
	/* write in file */
	else {
		int bytes_written = file_write(f, buffer, size);

		return bytes_written;
	}
}

/* fd로 열린 파일의 
	pos를 position으로 변경한다. */
void 
seek (int fd, unsigned position) {
	if (fd < 0 || fd > MAX_FD || fd == NULL)
		exit(-1);
	if (position < 0)
		return;

	struct thread *curr = thread_current();
	struct file *f = process_get_file(fd);
	file_seek(f, position);
}

/* fd로 열린 파일에서
	다음으로 읽히거나 써지는 곳의 pos를 리턴한다. */
unsigned 
tell (int fd) {
	if (fd < 0 || fd > MAX_FD || fd == NULL)
		exit(-1);

	struct thread *curr = thread_current();
	struct file *f = process_get_file(fd);
	
	return file_tell(f);
}

/* fd로 열린 파일을 닫고
	File Descriptor table의 해당 엔트리를 초기화한다. */
void 
close (int fd) {
	if (fd < 2 || fd >= MAX_FD || fd == NULL)
		exit(-1);

	struct file *f = process_get_file(fd);
	if (!f) 
		exit(-1);
	process_close_file(fd);		
}
