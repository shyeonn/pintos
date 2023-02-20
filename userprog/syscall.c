#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/off_t.h"
#include "list.h"
#include "stdbool.h"
#include "string.h"
#include "threads/interrupt.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(const uint64_t *addr);
int add_file_to_fdt(struct file *file);


typedef int pid_t;
  

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
}

static void
sys_halt () {
	power_off();
}

static void
sys_exit (int status) {
	struct thread *t = thread_current();
	t->is_exit = true;
	t->exit_status = status;

	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();

}

static pid_t 
sys_fork (const char *thread_name, struct intr_frame *f) {
	pid_t c_pid; 	

	c_pid = process_fork(thread_name, f);
	return c_pid;
}

static int
sys_exec (const char *cmd_line, struct intr_frame *f) {
	check_address((uint64_t *)cmd_line);

	int file_size = strlen(cmd_line) + 1;
	char *fn_copy = palloc_get_page(PAL_ZERO); 
	if (fn_copy == NULL) {
		sys_exit(-1);
	}
	strlcpy(fn_copy, cmd_line, file_size);

	if(process_exec(fn_copy) == -1) {
		sys_exit(-1);
	}

	NOT_REACHED();
	return 0;
}

static int
sys_wait (pid_t pid) {
	int status;
	status = process_wait(pid);

	return status;
}

static bool
sys_create (const char *file, unsigned initial_size) {
	check_address((uint64_t *)file);
	return filesys_create(file, initial_size);

}

static bool
sys_remove (const char *file) {
	return filesys_remove(file);
}

static bool
sys_open (const char *file) {
	check_address((uint64_t *)file); 

	struct file *open_file = filesys_open(file);

	add_file_to_fdt(open_file);


}

static void
sys_write (int fd, const void *buffer, unsigned size) {
	if(fd){
		printf("%s", (char *)buffer);
	}

}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	/* Save argument. !! We should implement check valid pointer!! */
	uint64_t arg[6];
	arg[0] = f->R.rdi;
	arg[1] = f->R.rsi;
	arg[2] = f->R.rdx;
	arg[3] = f->R.r10;
	arg[4] = f->R.r8;
	arg[5] = f->R.r9;
	

	switch(f->R.rax){
		case SYS_HALT :
			sys_halt();
			break;

		case SYS_EXIT :
			sys_exit((int)arg[0]);
			break;

		case SYS_FORK :
			f->R.rax = sys_fork((char *)arg[0], f);
			break;

		case SYS_WAIT :
			f->R.rax = sys_wait((pid_t)arg[0]);
			break;

		case SYS_EXEC :
			f->R.rax = sys_exec((const char *)arg[0], f);
			break;
			
		case SYS_CREATE :
			f->R.rax = sys_create((const char *)arg[0], (unsigned)arg[1]);
			break;


		case SYS_WRITE :
			sys_write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
			break;

	
	}

}



void
check_address(const uint64_t *addr)
{
	struct thread *cur = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) || 
				!pml4_get_page(cur->pml4, addr))
		sys_exit(-1);
}

int 
add_file_to_fdt(struct file *file) {
	struct thread *cur = thread_current();
	struct file **fdt = cur->fd_table;

	while (cur->fd_idx < FDCOUNT_LIMIT && fdt[cur->fd_idx]) {
		cur -> fd_idx ++;
	}

	if (cur->fd_idx >= FDCOUNT_LIMIT)
		return -1;

	fdt[cur->fd_idx] = file;
	return cur->fd_idx;
}
