#ifndef __XEN_X86_DEFNS_H__
#define __XEN_X86_DEFNS_H__

/*
 * EFLAGS bits
 */
#define X86_EFLAGS_CF	0x00000001 /* Carry Flag */
#define X86_EFLAGS_MBS	0x00000002 /* Resvd bit */
#define X86_EFLAGS_PF	0x00000004 /* Parity Flag */
#define X86_EFLAGS_AF	0x00000010 /* Auxillary carry Flag */
#define X86_EFLAGS_ZF	0x00000040 /* Zero Flag */
#define X86_EFLAGS_SF	0x00000080 /* Sign Flag */
#define X86_EFLAGS_TF	0x00000100 /* Trap Flag */
#define X86_EFLAGS_IF	0x00000200 /* Interrupt Flag */
#define X86_EFLAGS_DF	0x00000400 /* Direction Flag */
#define X86_EFLAGS_OF	0x00000800 /* Overflow Flag */
#define X86_EFLAGS_IOPL	0x00003000 /* IOPL mask */
#define X86_EFLAGS_NT	0x00004000 /* Nested Task */
#define X86_EFLAGS_RF	0x00010000 /* Resume Flag */
#define X86_EFLAGS_VM	0x00020000 /* Virtual Mode */
#define X86_EFLAGS_AC	0x00040000 /* Alignment Check */
#define X86_EFLAGS_VIF	0x00080000 /* Virtual Interrupt Flag */
#define X86_EFLAGS_VIP	0x00100000 /* Virtual Interrupt Pending */
#define X86_EFLAGS_ID	0x00200000 /* CPUID detection flag */

#define X86_EFLAGS_ARITH_MASK                          \
    (X86_EFLAGS_CF | X86_EFLAGS_PF | X86_EFLAGS_AF |   \
     X86_EFLAGS_ZF | X86_EFLAGS_SF | X86_EFLAGS_OF)

/*
 * Intel CPU flags in CR0
 */
#define X86_CR0_PE              0x00000001 /* Enable Protected Mode    (RW) */
#define X86_CR0_MP              0x00000002 /* Monitor Coprocessor      (RW) */
#define X86_CR0_EM              0x00000004 /* Require FPU Emulation    (RO) */
#define X86_CR0_TS              0x00000008 /* Task Switched            (RW) */
#define X86_CR0_ET              0x00000010 /* Extension type           (RO) */
#define X86_CR0_NE              0x00000020 /* Numeric Error Reporting  (RW) */
#define X86_CR0_WP              0x00010000 /* Supervisor Write Protect (RW) */
#define X86_CR0_AM              0x00040000 /* Alignment Checking       (RW) */
#define X86_CR0_NW              0x20000000 /* Not Write-Through        (RW) */
#define X86_CR0_CD              0x40000000 /* Cache Disable            (RW) */
#define X86_CR0_PG              0x80000000 /* Paging                   (RW) */

/*
 * Intel CPU flags in CR3
 */
#define X86_CR3_NOFLUSH    (_AC(1, ULL) << 63)
#define X86_CR3_ADDR_MASK  (PAGE_MASK & PADDR_MASK)
#define X86_CR3_PCID_MASK  _AC(0x0fff, ULL) /* Mask for PCID */

/*
 * Intel CPU features in CR4
 */
#define X86_CR4_VME        0x00000001 /* enable vm86 extensions */
#define X86_CR4_PVI        0x00000002 /* virtual interrupts flag enable */
#define X86_CR4_TSD        0x00000004 /* disable time stamp at ipl 3 */
#define X86_CR4_DE         0x00000008 /* enable debugging extensions */
#define X86_CR4_PSE        0x00000010 /* enable page size extensions */
#define X86_CR4_PAE        0x00000020 /* enable physical address extensions */
#define X86_CR4_MCE        0x00000040 /* Machine check enable */
#define X86_CR4_PGE        0x00000080 /* enable global pages */
#define X86_CR4_PCE        0x00000100 /* enable performance counters at ipl 3 */
#define X86_CR4_OSFXSR     0x00000200 /* enable fast FPU save and restore */
#define X86_CR4_OSXMMEXCPT 0x00000400 /* enable unmasked SSE exceptions */
#define X86_CR4_UMIP       0x00000800 /* enable UMIP */
#define X86_CR4_LA57       0x00001000 /* enable 5-level paging */
#define X86_CR4_VMXE       0x00002000 /* enable VMX */
#define X86_CR4_SMXE       0x00004000 /* enable SMX */
#define X86_CR4_FSGSBASE   0x00010000 /* enable {rd,wr}{fs,gs}base */
#define X86_CR4_PCIDE      0x00020000 /* enable PCID */
#define X86_CR4_OSXSAVE    0x00040000 /* enable XSAVE/XRSTOR */
#define X86_CR4_SMEP       0x00100000 /* enable SMEP */
#define X86_CR4_SMAP       0x00200000 /* enable SMAP */
#define X86_CR4_PKE        0x00400000 /* enable PKE */
#define X86_CR4_CET        0x00800000 /* Control-flow Enforcement Technology */
#define X86_CR4_PKS        0x01000000 /* Protection Key Supervisor */

/*
 * XSTATE component flags in XCR0
 */
#define X86_XCR0_FP_POS           0
#define X86_XCR0_FP               (1ULL << X86_XCR0_FP_POS)
#define X86_XCR0_SSE_POS          1
#define X86_XCR0_SSE              (1ULL << X86_XCR0_SSE_POS)
#define X86_XCR0_YMM_POS          2
#define X86_XCR0_YMM              (1ULL << X86_XCR0_YMM_POS)
#define X86_XCR0_BNDREGS_POS      3
#define X86_XCR0_BNDREGS          (1ULL << X86_XCR0_BNDREGS_POS)
#define X86_XCR0_BNDCSR_POS       4
#define X86_XCR0_BNDCSR           (1ULL << X86_XCR0_BNDCSR_POS)
#define X86_XCR0_OPMASK_POS       5
#define X86_XCR0_OPMASK           (1ULL << X86_XCR0_OPMASK_POS)
#define X86_XCR0_ZMM_POS          6
#define X86_XCR0_ZMM              (1ULL << X86_XCR0_ZMM_POS)
#define X86_XCR0_HI_ZMM_POS       7
#define X86_XCR0_HI_ZMM           (1ULL << X86_XCR0_HI_ZMM_POS)
#define X86_XCR0_PKRU_POS         9
#define X86_XCR0_PKRU             (1ULL << X86_XCR0_PKRU_POS)
#define X86_XCR0_LWP_POS          62
#define X86_XCR0_LWP              (1ULL << X86_XCR0_LWP_POS)

/*
 * Debug status flags in DR6.
 */
#define X86_DR6_DEFAULT         0xffff0ff0  /* Default %dr6 value. */

/*
 * Debug control flags in DR7.
 */
#define X86_DR7_DEFAULT         0x00000400  /* Default %dr7 value. */

/*
 * Invalidation types for the INVPCID instruction.
 */
#define X86_INVPCID_INDIV_ADDR      0
#define X86_INVPCID_SINGLE_CTXT     1
#define X86_INVPCID_ALL_INCL_GLOBAL 2
#define X86_INVPCID_ALL_NON_GLOBAL  3

#define X86_NR_VECTORS 256

/* Exception Vectors */
#define X86_EXC_DE             0 /* Divide Error */
#define X86_EXC_DB             1 /* Debug Exception */
#define X86_EXC_NMI            2 /* NMI */
#define X86_EXC_BP             3 /* Breakpoint */
#define X86_EXC_OF             4 /* Overflow */
#define X86_EXC_BR             5 /* BOUND Range */
#define X86_EXC_UD             6 /* Invalid Opcode */
#define X86_EXC_NM             7 /* Device Not Available */
#define X86_EXC_DF             8 /* Double Fault */
#define X86_EXC_CSO            9 /* Coprocessor Segment Overrun */
#define X86_EXC_TS            10 /* Invalid TSS */
#define X86_EXC_NP            11 /* Segment Not Present */
#define X86_EXC_SS            12 /* Stack-Segment Fault */
#define X86_EXC_GP            13 /* General Porection Fault */
#define X86_EXC_PF            14 /* Page Fault */
#define X86_EXC_SPV           15 /* PIC Spurious Interrupt Vector */
#define X86_EXC_MF            16 /* Maths fault (x87 FPU) */
#define X86_EXC_AC            17 /* Alignment Check */
#define X86_EXC_MC            18 /* Machine Check */
#define X86_EXC_XM            19 /* SIMD Exception */
#define X86_EXC_VE            20 /* Virtualisation Exception */
#define X86_EXC_CP            21 /* Control-flow Protection */
#define X86_EXC_HV            28 /* Hypervisor Injection */
#define X86_EXC_VC            29 /* VMM Communication */
#define X86_EXC_SX            30 /* Security Exception */

/* Bitmap of exceptions which have error codes. */
#define X86_EXC_HAVE_EC                                             \
    ((1u << X86_EXC_DF) | (1u << X86_EXC_TS) | (1u << X86_EXC_NP) | \
     (1u << X86_EXC_SS) | (1u << X86_EXC_GP) | (1u << X86_EXC_PF) | \
     (1u << X86_EXC_AC) | (1u << X86_EXC_CP) |                      \
     (1u << X86_EXC_VC) | (1u << X86_EXC_SX))

/* Memory types */
#define X86_MT_UC     0x00 /* uncachable */
#define X86_MT_WC     0x01 /* write-combined */
#define X86_MT_RSVD_2 0x02 /* reserved */
#define X86_MT_RSVD_3 0x03 /* reserved */
#define X86_MT_WT     0x04 /* write-through */
#define X86_MT_WP     0x05 /* write-protect */
#define X86_MT_WB     0x06 /* write-back */
#define X86_MT_UCM    0x07 /* UC- */
#define X86_NUM_MT    0x08

#endif	/* __XEN_X86_DEFNS_H__ */
