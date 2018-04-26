#define pr_fmt(fmt) "XX syscall_tracer_pt: " fmt
#include "syscall_tracer_pt.h"
#include "kaper_handlers.h"
#include "pt_mod.h"
#include "guest_syscalls.h"
#include "syscall_tracer.h"
#include "syscall_debugfs.h"

// VM
//#define smp_apic_timer_interrupt_offset 0x107fef0
// AMD
//#define smp_apic_timer_interrupt_offset 0x18f0570
#define nmi 0x094b130

#define int_marker nmi

static gva_t last_fault_address = 0;
static int last_sc_id = NO_RAX;
static u64* entry_sptep;

// time to clear out our collected data
static bool reset_syscall_pt_list;

typedef int (*fp_page_fault)(struct kvm_vcpu *vcpu, gva_t gpa, u32 error_code, bool prefault);
static fp_page_fault original_tdp_page_fault;
static int get_offset_handler(struct kvm_vcpu *vcpu, gva_t gpa, u32 error_code, bool prefault);
static int syscall_page_fault(struct kvm_vcpu *vcpu, gva_t gpa, u32 error_code, bool prefault);
static int syscall_entry_page_fault(struct kvm_vcpu *vcpu, gva_t gpa, u32 error_code, bool prefault);

// marks when we injected an nmi, so that it can be handled accordingly
static bool injected_nmi = false;
// the address we faulted on after injecting the nmi
static u64 intr_addr;

static bool get_syscall_num_entry(u64 gpa);
static u64 get_syscall_num(u64 gpa);
static void set_global_offset(u64 off, struct kvm_vcpu* vcpu);

static void nx_only_sce(struct kvm_vcpu *vcpu);
static void nx_all_but_sce(struct kvm_vcpu *vcpu);
static void nx_sc(struct kvm_vcpu *vcpu);

static atomic_t is_init; // protected by memtrace_lock

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

int init_syscall_pt_trace(struct kvm_vcpu* vcpu) {
   if(!vcpu) {
      pr_warn("Warning: vcpu %p\n", vcpu);
      return -EFAULT;
   }

   if(is_init_and_set()) {
      pr_warn("Already initialized\n");
      return -EFAULT;
   }

   original_tdp_page_fault = vcpu->arch.mmu.page_fault;
   pr_info("Stored original_tdp_page_fault %p\n", original_tdp_page_fault);

   /* Used to get the location of the text section of the kernel (KASLR) */
   vcpu->arch.mmu.page_fault = get_offset_handler;
   set_prot_all(NP_NX, 1, vcpu);

   injected_nmi = false;
   intr_addr = 0;

   pr_info("Syscall Trace initialized\n");
   return 0;
}

int reset_syscall_pt_trace(struct kvm_vcpu* vcpu)
{
   if(!vcpu) {
      pr_warn("Warning: vcpu %p\n", vcpu);
      return -EFAULT;
   }

   if(!atomic_dec_and_test(&is_init)) {
      pr_warn("can not reset because syscall tracer not initialized\n");
      return -EFAULT;
   }

   pr_info("reset_syscall_pt_trace\n");

   // reset all
   //set_prot_all(NP_NX, 0, vcpu);
   pr_info("Restoring original_tdp_page_fault %p\n", vcpu->arch.mmu.page_fault);
   //vcpu->arch.mmu.page_fault = original_tdp_page_fault;
   pr_info("Restored original_tdp_page_fault %p\n", vcpu->arch.mmu.page_fault);

   pr_info("reset_syscall_pt_trace done\n");
   return 0;
}

void set_reset_syscall_pt_list(void)
{
   reset_syscall_pt_list = true;
}


void syscall_total_reset(void)
{
   reset_syscall_pt_list = true;
}

void syscall_trace_reset(void)
{
   reset_syscall_pt_list = true;
}

void syscall_capture_reset(void)
{
   reset_syscall_pt_list = true;
}

void syscall_replay_reset(void)
{
   reset_syscall_pt_list = true;
}

static bool get_syscall_num_entry(u64 gpa)
{
   return (gpa & PAGE_MASK) == (ptr_entry_SYSCALL_64 & PAGE_MASK);
}

static u64 get_syscall_num(u64 gpa)
{
   u64 i;
   for(i=0; guest_syscalls[i] != 0; ++i) {
      if((gpa & PAGE_MASK) == (guest_syscalls[i] & PAGE_MASK))
         return i;
   }
   return NO_RAX;
}

static u64 global_offset;
static void set_global_offset(u64 off, struct kvm_vcpu* vcpu)
{
   int i;
   global_offset = off;
   pr_info("Setting offset %llx\n", off);
   for(i=0; guest_syscalls[i] != 0; ++i) {
      guest_syscalls[i] += off;
      guest_syscalls_spte[i] = get_ptptr(guest_syscalls[i], vcpu);
      if(guest_syscalls_spte[i] == NULL) {
         pr_err("ERROR: could not get sptep for gpa %llx\n",
               (unsigned long long)guest_syscalls[i]);
      }
   }
   ptr_entry_SYSCALL_64 += off;
   pr_info("ptr_entry_SYSCALL_64 %llx\n", ptr_entry_SYSCALL_64);
   /* Do we need that?? */
   //    ptr_page_offset_base += off;
}

static u64* common_handler(struct kvm_vcpu* vcpu,gva_t gpa,u32 error_code,
      bool prefault) {
   u64* sptep;
   struct vcpu_svm* svm;
   bool genuine_fault;

   svm = to_svm(vcpu);
   sptep = get_ptptr(gpa, vcpu);
   if(!sptep) {
      pr_err("#CMN: ERROR getting sptep for gpa: 0x%p\n",sptep);
      // FIXME: just to be sure
      //prot(sptep, NP_NX, 0, vcpu->kvm);
      //panic("get_ptptr()");
      return NULL;
   }

   if(gpa == last_fault_address) {
      pr_err("ERROR doulbe fault @ %llx\n", (unsigned long long)gpa);
      // FIXME: just to be sure
      prot(sptep, NP_NX, 0, vcpu);
      // reset all protections on this page
      return NULL;
   }
   last_fault_address = gpa;


   genuine_fault = (!(error_code & PFERR_FETCH_MASK)) || (!(*sptep & (1L << 63)));

   /* We care only about faults caused by our tracer */
   if(genuine_fault) {
      // FIXME: just to be sure
      //prot(sptep, NP_NX, 0, vcpu->kvm);
      //printk("#CMN: Genuine fault at gpa: 0x%lx\n",gpa);
      DBG_PRINTF("#CMN: Genuine fault at gpa: 0x%lx\n",gpa);
      return NULL;
   }

   DBG_PRINTF("#CMNtf: gpa: 0x%lx error_code: 0x%x exitinfo1: 0x%llx"
         " exitinfo2: 0x%llx ip: 0x%llx *sptep: 0x%llxx\n",
         gpa,error_code,svm->vmcb->control.exit_info_1,
         svm->vmcb->control.exit_info_2,svm->vmcb->save.rip,*sptep);

   return sptep;
}

static int get_offset_handler(struct kvm_vcpu* vcpu,gva_t gpa,u32 error_code,
      bool prefault) {
   u64* sptep;
   //u64* entry_sptep;
   u64* hva;
   struct vcpu_svm* svm;
   unsigned long long cpu_phys_mask = ((1UL << boot_cpu_data.x86_phys_bits) - 1) << 12;
   svm = to_svm(vcpu);

   DBG_PRINTF("#OFF: 0x%llx\n",gpa);
   sptep = common_handler(vcpu,gpa,error_code,prefault);

   if(!sptep) {
      /* This is not a fault caused by us -> call the original handler */
      //DBG_PRINTF("#OFF: not our problem *sptep: 0x%llx\n",*sptep);
      return original_tdp_page_fault(vcpu,gpa,error_code,prefault);
   }

   hva = (u64*)(__va(__sme_clr(*sptep & cpu_phys_mask) | (gpa & 0xfff)));

   pr_info("#OFF: gpa: 0x%lx error_code: 0x%x exitinfo1: 0x%llx"
         " exitinfo2: 0x%llx ip: 0x%llx sptep: 0x%p hva: 0x%p",
         gpa,error_code,svm->vmcb->control.exit_info_1,
         svm->vmcb->control.exit_info_2,svm->vmcb->save.rip,sptep,hva);

   /* Allow execution of page */
   prot(sptep, NP_NX, 0, vcpu);

   /* ASSUMPTION: this gpa contains the guest apic_local_timer handler */
   intr_addr = gpa;

   set_global_offset((gpa & PAGE_MASK) - (ptr_apic_timer_interrupt & PAGE_MASK), vcpu);
   pr_info("GPA: 0x%lx, ptr: %llx -> offset %llx", gpa, ptr_apic_timer_interrupt,
         (gpa & PAGE_MASK) - (ptr_apic_timer_interrupt & PAGE_MASK));

   DBG_PRINTF("    Calculated offset apic to gpa: 0x%llx\n",
         ((gpa & PAGE_MASK) - (ptr_apic_timer_interrupt & PAGE_MASK)));

   DBG_PRINTF("    Clear NX bit for all pages\n");
   set_prot_all(NP_NX, 0, vcpu);
   __flush_tlb_all();

   DBG_PRINTF("    Protecting syscall entry page: 0x%llx\n",ptr_entry_SYSCALL_64);
   entry_sptep = get_ptptr(ptr_entry_SYSCALL_64, vcpu);
   if(!entry_sptep) {
      pr_err("ERROR could not get spte for syscall entry at %llx\n",
            (unsigned long long)ptr_entry_SYSCALL_64);
      panic("get_offset_handler");
   }
   prot(entry_sptep, NP_NX, 1, vcpu);

   //TODO: Offset calculated correctly but syscall_entry_page_fault handler
   //does not work correctly
   vcpu->arch.mmu.page_fault = syscall_entry_page_fault;

   DBG_PRINTF("EXIT #OFF sptep: 0x%llx\n\n",*sptep);

   return 0;

}

static void nx_sc(struct kvm_vcpu *vcpu)
{
   int i;
   // use guest_syscalls[i], because sptes might be zero due to messups
   for(i=0; guest_syscalls[i] != 0; ++i) {
      if(guest_syscalls_spte[i] == NULL) {
         continue;
      }
      prot(guest_syscalls_spte[i], NP_NX, 1, vcpu);
   }
   __flush_tlb_all();
}

static void nx_only_sce(struct kvm_vcpu *vcpu)
{
   set_prot_all(NP_NX, 0, vcpu);
   prot(entry_sptep, NP_NX, 1, vcpu);
   __flush_tlb_all();

}

static void nx_all_but_sce(struct kvm_vcpu *vcpu)
{
   set_prot_all(NP_NX, 1, vcpu);
   prot(entry_sptep, NP_NX, 0, vcpu);
   __flush_tlb_all();

}

/* handler to hopefully catch the execution of the syscall entry stub.
 * If we get an X fault on the syscall entry stub page we assume
 * that a syscall handler execution is about to start.
 * The syscall handler execution will be handled by syscall_page_fault.
 * REMARK: there is other code colocated on the syscall entry stub page
 * therefore we might falsely assume to be in a syscall execution.
 * We handle this case in syscall_page_fault
 */
static int syscall_entry_page_fault(struct kvm_vcpu *vcpu,gva_t gpa,u32 error_code,
      bool prefault) {
   u64* sptep;
   struct vcpu_svm* svm;


   //TODO: Print caused faults here
   sptep = common_handler(vcpu,gpa,error_code,prefault);

   if(!sptep) {
      /* This is not a fault caused by us -> call the original handler */
      DBG_PRINTF("#SYSen: not our problem *sptep: 0x%llx\n",*sptep);
      return original_tdp_page_fault(vcpu,gpa,error_code,prefault);
   }

   svm = to_svm(vcpu);
   DBG_PRINTF("#SYSen: gpa: 0x%lx error_code: 0x%x exitinfo1: 0x%llx"
         " exitinfo2: 0x%llx ip: 0x%llx sptep: 0x%p rax: 0x%llx\n", gpa,error_code,
         svm->vmcb->control.exit_info_1,svm->vmcb->control.exit_info_2,
         svm->vmcb->save.rip,sptep,svm->vmcb->save.rax);

   if (get_syscall_num_entry(gpa)) {
      // this is indeed the syscall entry page
      // nx all other pages and make this one executable
      // We have to nx *all* other pages because if
      // this is not actually the start of a syscall
      // we don't really know where we might end up
      nx_all_but_sce(vcpu);
      // next fault has to be handled by the syscall handler page fault handler
      vcpu->arch.mmu.page_fault = syscall_page_fault;
   } else {
      pr_warn("WARNING fault on NOT sce in syscall_entry_page_fault (%llx)\n",
            (unsigned long long)gpa);
      // this can happen in the following cases:
      // TODO
      prot(sptep, NP_NX, 0, vcpu);
   }

   // 0 == retry instruction
   return 0;
}

static int add_sc(struct kvm_vcpu *vcpu, unsigned long long rax)
{
   //pr_info("SC %llx\n", rax);
   mem_checkpoint(vcpu, last_sc_id);
   last_sc_id = rax;
   return 0;
}

static int syscall_page_fault(struct kvm_vcpu *vcpu, gva_t gpa,u32 error_code,
      bool prefault) {
   int syscall_nr;
   u64* sptep;
   struct vcpu_svm* svm;
   svm = to_svm(vcpu);

   sptep = common_handler(vcpu, gpa, error_code, prefault);

   if(!sptep) { // this pf is not for us
      return original_tdp_page_fault(vcpu, gpa, error_code, prefault);
   }

   if (get_syscall_num_entry(gpa)) {
      // this can happen in the following case:
      // we got a NO_RAX (see next block)
      // since we hit the syscall entry page again
      // we probably incorrectly hoped to be in a syscall execution
      // maybe this time it is actually a syscall so lets try again

      // we hit the syscall entry page again,
      // set all other pages NX and try again
      nx_all_but_sce(vcpu);

      // we keep this handler
      return 0;
   }

   // this might be a syscall
   syscall_nr = get_syscall_num(gpa);
   if(syscall_nr == NO_RAX) { // or not
      // this happens when we incorrectly assume to have
      // observed a syscall entry, but it was actually something different
      // we have to go back to trying to catch the syscall entry
      //nx_only_sce(vcpu);
      // next fault back to syscall_entry_page_fault
      //vcpu->arch.mmu.page_fault = syscall_entry_page_fault;

      // there might be interrupts and such squeezing in between entry and handler execution
      // so to be safe we just X the current page
      // maybe we end up on a handler page later
      //prot(sptep, NP_NX, 0, vcpu->kvm);
      // but we also have to NX the sc entry page
      // because we might not be inside a syscall execution after all
      //prot(entry_sptep, NP_NX, 1, vcpu->kvm);
      nx_only_sce(vcpu);
      nx_sc(vcpu);

      // we keep this handler
      return 0;
   }

   // this is actually probably a syscall handler
   add_sc(vcpu, syscall_nr);
   nx_only_sce(vcpu);
   // next fault back to syscall_entry_page_fault
   vcpu->arch.mmu.page_fault = syscall_entry_page_fault;
   return 0;
}
