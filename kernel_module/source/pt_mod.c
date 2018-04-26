#define pr_fmt(fmt) "XX pt_mod: " fmt
#include "pt_mod.h"
#include "kaper_handlers.h"
#include "mem_tracer.h"
#include "kaper.h"
#include <asm/compat.h>
#include <linux/printk.h>
#include <linux/mem_encrypt.h>
#include <asm/processor.h>

// must hold kvm->mmu_lock
static void unset_prot(u64* sptep, u64 pflags) {
   *sptep &= ~pflags;
   if((*sptep & 0x7) == 0x2 ||
         (*sptep & 0x7) == 0x6) {
      *sptep |= pflags;
      printk(KERN_ERR "EPT misconfiguration %llx\n", (unsigned long long)*sptep);
   }

}

// must hold kvm->mmu_lock
static void set_prot(u64* sptep, u64 pflags) {
   *sptep |= pflags;
}

// must hold kvm->mmu_lock
static void __prot(u64* sptep, u64 pflags, char set) {
   if(set)
      set_prot(sptep, pflags);
   else
      unset_prot(sptep, pflags);
}

u64* get_ptptr_nolock(u64 gpa, struct kvm_vcpu* vcpu)
{
   u64* pml4_ptr;
   u64* pdp_ptr;
   u64* pde_ptr;
   u64* ept_ptr;
   // bit 47:39
   u64 pml4_offset;
   // bit 38:30
   u64 pdp_offset;
   // bit 29:21
   u64 pde_offset;
   // bit 20:12
   u64 ept_offset;
   unsigned long long cpu_phys_mask;
   u64* ret = NULL;
   struct kvm* kvm = NULL;

   // bit 47:39
   pml4_offset = ((gpa >> 39) & 0x1ff) << 3;
   // bit 38:30
   pdp_offset = ((gpa >> 30) & 0x1ff) << 3;
   // bit 29:21
   pde_offset = ((gpa >> 21) & 0x1ff) << 3;
   // bit 20:12
   ept_offset = ((gpa >> 12) & 0x1ff) << 3;

   cpu_phys_mask = ((1UL << boot_cpu_data.x86_phys_bits) - 1) << 12;

   if(unlikely(vcpu == NULL)) {
      pr_warn("WARNING: vcpu %p\n", vcpu);
      ret = NULL;
      goto out;
   }

   kvm = vcpu->kvm;

   // 51:12
   pml4_ptr = (u64*)__va((__sme_clr(vcpu->arch.mmu.root_hpa) & cpu_phys_mask)
         | pml4_offset);
   if(!((*pml4_ptr) & 0x1)) {
      ret = NULL;
      goto out;
   }

   pdp_ptr = (u64*)__va((__sme_clr(*pml4_ptr & cpu_phys_mask)) | pdp_offset);
   if(!((*pdp_ptr) & 0x1)) {
      pr_err("ERROR getting spte pdp for %llx\n", (unsigned long long)gpa);
      ret = NULL;
      goto out;
   }

   if(*pdp_ptr & 0x80) {
      // 51:30 from pdp 29:0 from GPA
      //1GB mapping
      pr_err("Returning pdp_ptr\n");
      ret = NULL;
      goto out;
   }

   pde_ptr = (u64*)__va((__sme_clr(*pdp_ptr & cpu_phys_mask)) | pde_offset);
   if(!((*pde_ptr) & 0x1)) {
      pr_err("ERROR getting spte pde for %llx\n", (unsigned long long)gpa);
      ret = NULL;
      goto out;
   }
   if(*pde_ptr & 0x80) {
      // 51:21 from pdp 20:0 from GPA
      //sptep = (u64*) __va((*pdp_ptr & 0xfffffe00000) + (gpa & 0x1fffffff));
      ret = pde_ptr;
      goto out;
   }

   ept_ptr = (u64*)__va((__sme_clr(*pde_ptr & cpu_phys_mask)) | ept_offset);
   ret = ept_ptr;

out:
   return ret;
}

u64* get_ptptr(u64 gpa, struct kvm_vcpu* vcpu) {
   u64* pml4_ptr;
   u64* pdp_ptr;
   u64* pde_ptr;
   u64* ept_ptr;
   // bit 47:39
   u64 pml4_offset;
   // bit 38:30
   u64 pdp_offset;
   // bit 29:21
   u64 pde_offset;
   // bit 20:12
   u64 ept_offset;
   unsigned long long cpu_phys_mask;
   u64* ret = NULL;
   struct kvm* kvm = NULL;

   // bit 47:39
   pml4_offset = ((gpa >> 39) & 0x1ff) << 3;
   // bit 38:30
   pdp_offset = ((gpa >> 30) & 0x1ff) << 3;
   // bit 29:21
   pde_offset = ((gpa >> 21) & 0x1ff) << 3;
   // bit 20:12
   ept_offset = ((gpa >> 12) & 0x1ff) << 3;

   cpu_phys_mask = ((1UL << boot_cpu_data.x86_phys_bits) - 1) << 12;

   if(unlikely(vcpu == NULL)) {
      pr_warn("WARNING: vcpu %p\n", vcpu);
      ret = NULL;
      goto out;
   }

   kvm = vcpu->kvm;
   spin_lock(&kvm->mmu_lock);

   // 51:12
   pml4_ptr = (u64*)__va((__sme_clr(vcpu->arch.mmu.root_hpa) & cpu_phys_mask)
         | pml4_offset);
   if(!((*pml4_ptr) & 0x1)) {
      ret = NULL;
      goto out;
   }

   pdp_ptr = (u64*)__va((__sme_clr(*pml4_ptr & cpu_phys_mask)) | pdp_offset);
   if(!((*pdp_ptr) & 0x1)) {
      pr_err("ERROR getting spte pdp for %llx\n", (unsigned long long)gpa);
      ret = NULL;
      goto out;
   }

   if(*pdp_ptr & 0x80) {
      // 51:30 from pdp 29:0 from GPA
      //1GB mapping
      pr_err("Returning pdp_ptr\n");
      ret = NULL;
      goto out;
   }

   pde_ptr = (u64*)__va((__sme_clr(*pdp_ptr & cpu_phys_mask)) | pde_offset);
   if(!((*pde_ptr) & 0x1)) {
      pr_err("ERROR getting spte pde for %llx\n", (unsigned long long)gpa);
      ret = NULL;
      goto out;
   }
   if(*pde_ptr & 0x80) {
      // 51:21 from pdp 20:0 from GPA
      //sptep = (u64*) __va((*pdp_ptr & 0xfffffe00000) + (gpa & 0x1fffffff));
      ret = pde_ptr;
      goto out;
   }

   ept_ptr = (u64*)__va((__sme_clr(*pde_ptr & cpu_phys_mask)) | ept_offset);
   ret = ept_ptr;

out:
   spin_unlock(&kvm->mmu_lock);
   return ret;
}

void prot(u64* sptep, u64 pflags, char set, struct kvm_vcpu* vcpu) {
   struct kvm* kvm;
   if(unlikely(vcpu == NULL)) {
      pr_warn("WARNING: vcpu %p\n", vcpu);
      return;
   }

   kvm = vcpu->kvm;
   spin_lock(&kvm->mmu_lock);
   __prot(sptep,pflags, set);
   spin_unlock(&kvm->mmu_lock);
}

void set_prot_all(u64 pflags, char set, struct kvm_vcpu* vcpu) {
   struct kvm* kvm;
   u64* pml4_ptr;
   u64* pdp_ptr;
   u64* pde_ptr;
   u64* ept_ptr;
   u64 pml4_offset;
   u64 pdp_offset;
   u64 pde_offset;
   u64 ept_offset;
   unsigned long long cpu_phys_mask;

   if(unlikely(vcpu == NULL)) {
      pr_warn("WARNING: vcpu %p\n", vcpu);
      return;
   }

   pml4_offset = 0;
   pdp_offset = 0;
   pde_offset = 0;
   ept_offset = 0;

   kvm = vcpu->kvm;

   /* The physical address size is not fixed */
   cpu_phys_mask = ((1UL << boot_cpu_data.x86_phys_bits) - 1) << 12;

   spin_lock(&kvm->mmu_lock);

   for(pml4_offset=0; pml4_offset<512; ++pml4_offset) {
      // 51:12
      pml4_ptr = ((u64*)__va(__sme_clr(vcpu->arch.mmu.root_hpa & cpu_phys_mask)))
         + pml4_offset;

      if(!((*pml4_ptr) & 0x1))
         continue;

      for(pdp_offset=0; pdp_offset<512; ++pdp_offset) {
         pdp_ptr = ((u64*)__va(__sme_clr(*pml4_ptr & cpu_phys_mask)))
            + pdp_offset;

         if(!((*pdp_ptr) & 0x1)) {
            continue;
         }
         if(*pdp_ptr & 0x80) {
            pr_debug("Found 1GB entry: %p\n", pdp_ptr);
            __prot(pdp_ptr, pflags, set);
            continue;
         }

         for(pde_offset=0; pde_offset<512; ++pde_offset) {
            pde_ptr = ((u64*)__va(__sme_clr(*pdp_ptr & cpu_phys_mask)))
               + pde_offset;

            if(!((*pde_ptr) & 0x1)) {
               continue;
            }
            if(*pde_ptr & 0x80) {
               // 51:21 from pdp 20:0 from GPA
               pr_debug("Found 2M entry: %p\n", pde_ptr);
               __prot(pde_ptr, pflags, set);
               continue;
            }

            for(ept_offset=0; ept_offset<512; ++ept_offset) {
               ept_ptr = ((u64*)__va(__sme_clr(*pde_ptr & cpu_phys_mask)))
                  + ept_offset;

               __prot(ept_ptr, pflags, set);
            }
         }
      }
   }

   spin_unlock(&kvm->mmu_lock);
}

