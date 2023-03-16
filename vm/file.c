/* file.c: Implementation of memory backed file object (mmaped object). */

#include "filesys/file.h"
#include "filesys/off_t.h"
#include "stdbool.h"
#include "threads/mmu.h"
#include "threads/thread.h"
#include "vm/file.h"
#include "vm/vm.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};


/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	if(pml4_is_dirty(thread_current()->pml4, page->va)) {
		printf("WRITE\n");
		file_seek(page->file_data->file, page->file_data->new_offset);
		file_write(page->file_data->file, page->va, page->file_data->read_bytes);
	}
	file_close(page->file_data->file);
	free(page->file_data);
//	printf("destroy : 0x%llx\n", page->va); 
	
}

static bool
lazy_load (struct page *page, struct file_backed_data *f_data) {
	
	file_seek(f_data->file, f_data->new_offset);
	/* Load this page. */
	if (file_read (f_data->file, page->va, f_data->read_bytes) 
			!= (int) f_data->read_bytes) {
		return false;
	}

	memset (page->va + f_data->read_bytes, 0, PGSIZE - f_data->read_bytes);
	//printf("Lazy_load 0x%llx\n", page->va);
	//printf("Size %u\n", f_data->read_bytes);

	/* Set Dirty bits to false */
	pml4_set_dirty(thread_current()->pml4, page->va, false);

	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	size_t check_length = 0;
	size_t file_len = file_length(file);
	

	while(file_len > check_length) {
		if(spt_find_page(&thread_current()->spt, addr+check_length)) {
			return NULL;
		}

		struct file_backed_data *f_data = 
		(struct file_backed_data *)malloc(sizeof(struct file_backed_data));

		f_data->file = file_duplicate(file);
		f_data->new_offset = offset+check_length;
		if((file_len - check_length) < PGSIZE)
			f_data->read_bytes = file_len - check_length;
		else
			f_data->read_bytes = PGSIZE;

		vm_alloc_page_with_initializer(VM_FILE, addr+check_length, 
				writable, lazy_load, f_data);

		check_length += PGSIZE;
		}
	/*
	while(length > check_length){
		struct file_backed_data *f_data = 
		(struct file_backed_data *)malloc(sizeof(struct file_backed_data));

		f_data->file = NULL;
		if((length - check_length) < PGSIZE)
			f_data->read_bytes = length - check_length;
		else
			f_data->read_bytes = PGSIZE;
		f_data->read_bytes =  


		check_length += PGSIZE;
	}
	*/
	return addr;

}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct page *p = spt_find_page(&thread_current()->spt, addr);

	spt_remove_page(&thread_current()->spt, p);

}
