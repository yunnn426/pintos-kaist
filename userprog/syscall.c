#include "userprog/syscall.h"
#include <stdio.h>
#include "filesys/file.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include <devices/input.h>
#include "lib/kernel/stdio.h"
#include "threads/synch.h"

// #define STDIN_FILENO 0
// #define STDOUT_FILENO 1

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void is_valid_addr(void *addr);
void halt (void); // 0 
void exit (int status);
bool create (const char *file, unsigned initial_size);
int open (const char *file);
bool remove(const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
int exec(const char *cmd_line);

#define EOF -1

struct lock filesys_lock;


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
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {

	int syscall_n = f->R.rax; /* 시스템 콜 넘버 */
#ifdef VM
	thread_current()->rsp = f->rsp;
#endif
	switch (syscall_n)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	// case SYS_FORK:
	// 	f->R.rax = fork(f->R.rdi, f);
	// 	break;
	// case SYS_EXEC:
	// 	f->R.rax = exec(f->R.rdi);
	// 	break;
	// case SYS_WAIT:
	// 	f->R.rax = wait(f->R.rdi);
	// 	break;
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
	}
}

void is_valid_addr(void *addr) {
	// if (addr == NULL) { exit(-1); }			// addr NULL 체크
	// struct thread *cur = thread_current();
	// if (is_kernel_vaddr(addr)) { exit(-1); } // 유저 가상 메모리인지 확인, 아니면 종료
	// if (pml4e_walk(cur->pml4, addr,false) == NULL ) { exit(-1); } // 해당 가상주소가 사용자 페이지에 매핑됐는지 확인

	struct thread *curr_thread = thread_current();
	if (addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(curr_thread->pml4, addr) == NULL){
		exit(-1);
	}
}

void halt (void) {
	power_off();
}

void exit (int status) {
	struct thread *curr = thread_current();
	// curr->exit_code = status;
	printf ("%s: exit(%d)\n", curr->name,status);
	thread_exit();
}

// int
// exec(const char *cmd_line) {
// 	tid_t tid = process_create_initd(cmd_line);
// 	struct thread *cur = thread_entry(tid);
// }

bool create (const char *file, unsigned initial_size) {
	is_valid_addr(file);
	return filesys_create(file,initial_size);
}

bool remove(const char *file) {
	is_valid_addr(file);
	return filesys_remove(file);
}

int open (const char *file) {
	is_valid_addr(file);
	
	struct file* tmp;
	int fd = -1;

	tmp = filesys_open(file);
	if (tmp == NULL) {
		return -1;
	}

	fd = process_add_file(tmp);
	if (fd == -1) {
		file_close(tmp);  // file.c 
	}

	return fd;
}

void close(int fd) {
	struct file *file = process_get_file(fd);
	if (file == NULL) {
		return;
	}
	process_close_file(fd);
}

void seek(int fd, unsigned position) {
	if (fd < 2) {
		return;
	}
	struct file *tmp = process_add_file(fd);
	if (tmp == NULL) {
		return;
	}
	file_seek(tmp, position);
}


unsigned tell(int fd) {
	struct file *open_file = process_get_file(fd);
	if (fd < 2)
		return;
	if (open_file)
		return file_tell(open_file);
	return NULL;
}


int filesize (int fd) {
	struct file *open_file = process_get_file(fd);
	if (open_file) 
		return file_length(open_file);
	return -1;
}
int
read (int fd, void *buffer, unsigned size) {		

	is_valid_addr(buffer);
	int bytes_read = 0;
	// 표준 입력 처리: fd가 1인 경우 키보드에서 입력을 읽습니다.
    if (fd == 0) {
        unsigned i;
        for (i = 0; i < size; i++) {
			int c = input_getc();
			if (c == EOF) break;
            ((char*)buffer)[i] = (char)c;
			bytes_read++;
        }
        return bytes_read;
	}

	struct file *file = process_get_file(fd);
	lock_acquire(&filesys_lock);
    bytes_read = file_read(file, buffer, size);
	lock_release(&filesys_lock);
    return bytes_read;
}

int write (int fd, const void *buffer, unsigned size) {
	/* lock 사용? */
	is_valid_addr(buffer);
	int writes = 0;
	if (fd == 1) {
		// void putbuf (const char *, size_t);
		putbuf(buffer,size);
        return size;
	}
	else if (fd < 2) {
		return -1;
	}

	struct file *file  = process_get_file(fd);
	if (file == NULL) {
		return -1;
	}
	lock_acquire(&filesys_lock);
	writes = file_write(file, buffer, size);
	lock_release(&filesys_lock);
	return writes;

	// int count = 0;
	// is_valid_addr(buffer);
	// lock_acquire(&filesys_lock);
	// if (fd == 1) {
	// 	putbuf(buffer, size);
	// 	lock_release(&filesys_lock);
	// 	return size;
	// }
	// struct file *open_file = process_get_file(fd);
	// if (open_file == NULL) {
	// 	lock_release(&filesys_lock);
	// 	return -1;
	// }
	// count = file_write(open_file, buffer, size);

	// lock_release(&filesys_lock);
	// return count;

}