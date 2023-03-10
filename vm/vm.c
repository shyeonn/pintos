/* vm.c: Generic interface for virtual memory objects. */

#include "hash.h"
#include "stddef.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/anon.h"
#include "vm/uninit.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* For Frame Management */
#include "threads/mmu.h"
#include <stdint.h>


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		if(page == NULL)
			return false;

		if(VM_TYPE(type) == VM_ANON){
			uninit_new(page, upage, init, type, aux, anon_initializer);
		}
		else if(VM_TYPE(type) == VM_FILE)
			uninit_new(page, upage, init, type, aux, NULL);

		page->writable = writable;
		page->load_data = aux; 

		/* TODO: Insert the page into the spt. */
		if(!spt_insert_page(spt, page))
			goto err;

		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page p;
	struct hash_elem *e;
	
	p.va = va;
	e = hash_find (&spt->hash_spt, &p.hash_elem);
	if(e == NULL)
		return NULL;

	return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	bool succ = false;

	//check va is not exits in spt
	//if(spt_find_page(spt, page->va) == NULL)
	//	return succ;

	//insert page in spt
	if(hash_insert(&spt->hash_spt, &page->hash_elem) == NULL)
		succ = true;
	//printf("[%llx]Insert page 0x%llx\n", spt, page->va);

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));

	if((frame->kva = palloc_get_page(PAL_USER)) == NULL)
		PANIC("todo");

	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, pg_round_down(addr));
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if(page == NULL)
		return false;

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	if((page = spt_find_page(&thread_current()->spt, va)) == NULL)
		return false;
	
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */


static bool
vm_do_claim_page_T (struct page *page, struct thread *t) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(!pml4_set_page(t->pml4, page->va, frame->kva, page->writable))

		return false;

	return swap_in (page, frame->kva);
}

static bool
vm_do_claim_page (struct page *page) {
	vm_do_claim_page_T(page, thread_current());
}

/* For spt_init function */
/* Returns a hash value for page p. */
static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	  const struct page *p = hash_entry (p_, struct page, hash_elem);
	    return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
static bool
page_less (const struct hash_elem *a_,
		   const struct hash_elem *b_, void *aux UNUSED) {
	  const struct page *a = hash_entry (a_, struct page, hash_elem);
	    const struct page *b = hash_entry (b_, struct page, hash_elem);

		  return a->va < b->va;
}


/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) { 
	hash_init(&spt->hash_spt, page_hash, page_less, NULL);
	spt->t = thread_current();
}

static bool
claim_copy_page (struct page *p, struct thread *t) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = p;
	p->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(!pml4_set_page(t->pml4, p->va, frame->kva, p->writable)) {
		return false;
		printf("vm.c:270\n");
	}

	return true;
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {

	struct hash_iterator i;

	hash_first (&i, &src->hash_spt);
	while (hash_next (&i)) {
			struct page *p = hash_entry (hash_cur (&i), struct page, hash_elem);
			struct page *new_p = (struct page *)malloc(sizeof(struct page));
			if(new_p == NULL)
				return false;

			memcpy(new_p, p, sizeof(struct page));
			//If stack is not Initial stack)
			if(!(new_p->uninit.type & VM_MARKER_0))
				p->load_data->user_cnt++;

			//If Not loaded
			if(pml4_get_page(src->t->pml4, p->va) == NULL){
			}
			//Loaded
			else {
				if(!claim_copy_page(new_p, dst->t)){
					free(new_p);
					return false;
				}
				memcpy(new_p->frame->kva, p->frame->kva, PGSIZE);
			}

			if(!spt_insert_page(dst, new_p)){
				free(new_p);
				return false;
			}
	}
	return true;
}

static void
spt_free_page(struct hash_elem *e, void *aux UNUSED) {
	
	 struct page *p = hash_entry(e, struct page, hash_elem);

	 //printf("Free page 0x%llx\n", p->va);
	 vm_dealloc_page(p);
}


/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator i;

	hash_first (&i, &spt->hash_spt);
	while (hash_next (&i)) {
			struct hash_elem *h = hash_cur (&i);
			spt_free_page(h, NULL);
			hash_delete(&spt->hash_spt, h);
	}

}

