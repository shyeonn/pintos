#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "list.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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
	struct thread *p_thread;
	struct thread *c_thread;

	c_pid = process_fork(thread_name, f);
	p_thread = thread_current();
	c_thread = find_thread(c_pid);

	//For child
	c_thread->parent = p_thread;

	//For parent
	list_push_back(&p_thread->children, &c_thread->c_elem);
	p_thread->child_head = *list_head(&p_thread->children);
	p_thread->child_tail = *list_tail(&p_thread->children);

	return c_pid;
}

static int
sys_exec (const char *cmd_line, struct intr_frame *f) {
	if(process_fork(cmd_line, f)){
		process_exec((void *)cmd_line);	
	}
	else
		printf(("This is parent"));
	

	return -1;
}

static int
sys_wait (pid_t pid) {
	return process_wait(pid);
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

		case SYS_WRITE :
			sys_write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
			break;

	
	}

}


