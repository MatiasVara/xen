/******************************************************************************
 * include/asm-x86/spec_ctrl.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (c) 2017-2018 Citrix Systems Ltd.
 */

#ifndef __X86_SPEC_CTRL_ASM_H__
#define __X86_SPEC_CTRL_ASM_H__

#ifdef __ASSEMBLY__
#include <asm/msr-index.h>
#include <asm/spec_ctrl.h>

/*
 * Saving and restoring MSR_SPEC_CTRL state is a little tricky.
 *
 * We want the guests choice of SPEC_CTRL while in guest context, and Xen's
 * choice (set or clear, depending on the hardware) while running in Xen
 * context.  Therefore, a simplistic algorithm is:
 *
 *  - Set/clear IBRS on entry to Xen
 *  - Set the guests' choice on exit to guest
 *  - Leave SPEC_CTRL unchanged on exit to xen
 *
 * There are two complicating factors:
 *  1) HVM guests can have direct access to the MSR, so it can change
 *     behind Xen's back.
 *  2) An NMI or MCE can interrupt at any point, including early in the entry
 *     path, or late in the exit path after restoring the guest value.  This
 *     will corrupt the guest value.
 *
 * Factor 1 is dealt with:
 *   - On VMX by using MSR load/save lists to have vmentry/exit atomically
 *     load/save the guest value.  Xen's value is loaded in regular code, and
 *     there is no need to use the shadow logic (below).
 *   - On SVM by altering MSR_SPEC_CTRL inside the CLGI/STGI region.  This
 *     makes the changes atomic with respect to NMIs/etc, so no need for
 *     shadowing logic.
 *
 * Factor 2 is harder.  We maintain a shadow_spec_ctrl value, and a use_shadow
 * boolean in the per cpu spec_ctrl_flags.  The synchronous use is:
 *
 *  1) Store guest value in shadow_spec_ctrl
 *  2) Set the use_shadow boolean
 *  3) Load guest value into MSR_SPEC_CTRL
 *  4) Exit to guest
 *  5) Entry from guest
 *  6) Clear the use_shadow boolean
 *  7) Load Xen's value into MSR_SPEC_CTRL
 *
 * The asynchronous use for interrupts/exceptions is:
 *  -  Set/clear IBRS on entry to Xen
 *  -  On exit to Xen, check use_shadow
 *  -  If set, load shadow_spec_ctrl
 *
 * Therefore, an interrupt/exception which hits the synchronous path between
 * steps 2 and 6 will restore the shadow value rather than leaving Xen's value
 * loaded and corrupting the value used in guest context.
 *
 * Additionally, in some cases it is safe to skip writes to MSR_SPEC_CTRL when
 * we don't require any of the side effects of an identical write.  Maintain a
 * per-cpu last_spec_ctrl value for this purpose.
 *
 * The following ASM fragments implement this algorithm.  See their local
 * comments for further details.
 *  - SPEC_CTRL_ENTRY_FROM_PV
 *  - SPEC_CTRL_ENTRY_FROM_INTR
 *  - SPEC_CTRL_ENTRY_FROM_INTR_IST
 *  - SPEC_CTRL_EXIT_TO_XEN_IST
 *  - SPEC_CTRL_EXIT_TO_XEN
 *  - SPEC_CTRL_EXIT_TO_PV
 *
 * Additionally, the following grep-fodder exists to find the HVM logic.
 *  - SPEC_CTRL_ENTRY_FROM_{SVM,VMX}
 *  - SPEC_CTRL_EXIT_TO_{SVM,VMX}
 */

.macro DO_SPEC_CTRL_COND_IBPB maybexen:req
/*
 * Requires %rsp=regs (also cpuinfo if !maybexen)
 * Requires %r14=stack_end (if maybexen), %rdx=0
 * Clobbers %rax, %rcx, %rdx
 *
 * Conditionally issue IBPB if SCF_entry_ibpb is active.  In the maybexen
 * case, we can safely look at UREGS_cs to skip taking the hit when
 * interrupting Xen.
 */
    .if \maybexen
        testb  $SCF_entry_ibpb, STACK_CPUINFO_FIELD(spec_ctrl_flags)(%r14)
        jz     .L\@_skip
        testb  $3, UREGS_cs(%rsp)
    .else
        testb  $SCF_entry_ibpb, CPUINFO_xen_spec_ctrl(%rsp)
    .endif
    jz     .L\@_skip

    mov     $MSR_PRED_CMD, %ecx
    mov     $PRED_CMD_IBPB, %eax
    wrmsr
    jmp     .L\@_done

.L\@_skip:
    lfence
.L\@_done:
.endm

.macro DO_OVERWRITE_RSB tmp=rax xu
/*
 * Requires nothing
 * Clobbers \tmp (%rax by default), %rcx
 *
 * xu is an optional parameter to add eXtra Uniqueness.  It is intended for
 * passing %= in from an asm() block, in order to work around
 * https://github.com/llvm/llvm-project/issues/60792 where Clang-IAS doesn't
 * expand \@ uniquely.
 *
 * Requires 256 bytes of {,shadow}stack space, but %rsp/SSP has no net
 * change. Based on Google's performance numbers, the loop is unrolled to 16
 * iterations and two calls per iteration.
 *
 * The call filling the RSB needs a nonzero displacement, and int3 halts
 * speculation.
 *
 * %rsp is preserved by using an extra GPR because a) we've got plenty spare,
 * b) the two movs are shorter to encode than `add $32*8, %rsp`, and c) can be
 * optimised with mov-elimination in modern cores.
 */
    mov $16, %ecx                   /* 16 iterations, two calls per loop */
    mov %rsp, %\tmp                 /* Store the current %rsp */

.L\@_fill_rsb_loop\xu:

    .irp n, 1, 2                    /* Unrolled twice. */
    call .L\@_insert_rsb_entry\xu\n /* Create an RSB entry. */
    int3                            /* Halt rogue speculation. */

.L\@_insert_rsb_entry\xu\n:
    .endr

    sub $1, %ecx
    jnz .L\@_fill_rsb_loop\xu
    mov %\tmp, %rsp                 /* Restore old %rsp */

#ifdef CONFIG_XEN_SHSTK
    mov $1, %ecx
    rdsspd %ecx
    cmp $1, %ecx
    je .L\@_shstk_done\xu
    mov $64, %ecx                   /* 64 * 4 bytes, given incsspd */
    incsspd %ecx                    /* Restore old SSP */
.L\@_shstk_done\xu:
#endif
.endm

.macro DO_SPEC_CTRL_COND_VERW
/*
 * Requires %rsp=cpuinfo
 *
 * Issue a VERW for its flushing side effect, if indicated.  This is a Spectre
 * v1 gadget, but the IRET/VMEntry is serialising.
 */
    testb $SCF_verw, CPUINFO_spec_ctrl_flags(%rsp)
    jz .L\@_verw_skip
    verw CPUINFO_verw_sel(%rsp)
.L\@_verw_skip:
.endm

.macro DO_SPEC_CTRL_ENTRY maybexen:req
/*
 * Requires %rsp=regs (also cpuinfo if !maybexen)
 * Requires %r14=stack_end (if maybexen)
 * Clobbers %rax, %rcx, %rdx
 *
 * PV guests can't update MSR_SPEC_CTRL behind Xen's back, so no need to read
 * it back.  Entries from guest context need to clear SPEC_CTRL shadowing,
 * while entries from Xen must leave shadowing in its current state.
 */
    mov $MSR_SPEC_CTRL, %ecx
    xor %edx, %edx

    /*
     * Clear SPEC_CTRL shadowing *before* loading Xen's value.  If entering
     * from a possibly-xen context, %rsp doesn't necessarily alias the cpuinfo
     * block so calculate the position directly.
     */
    .if \maybexen
        xor %eax, %eax
        /* Branchless `if ( !xen ) clear_shadowing` */
        testb $3, UREGS_cs(%rsp)
        setnz %al
        not %eax
        and %al, STACK_CPUINFO_FIELD(spec_ctrl_flags)(%r14)
        movzbl STACK_CPUINFO_FIELD(xen_spec_ctrl)(%r14), %eax
    .else
        andb $~SCF_use_shadow, CPUINFO_spec_ctrl_flags(%rsp)
        movzbl CPUINFO_xen_spec_ctrl(%rsp), %eax
    .endif

    wrmsr
.endm

.macro DO_SPEC_CTRL_EXIT_TO_XEN
/*
 * Requires %rbx=stack_end
 * Clobbers %rax, %rcx, %rdx
 *
 * When returning to Xen context, look to see whether SPEC_CTRL shadowing is
 * in effect, and reload the shadow value.  This covers race conditions which
 * exist with an NMI/MCE/etc hitting late in the return-to-guest path.
 */
    xor %edx, %edx

    testb $SCF_use_shadow, STACK_CPUINFO_FIELD(spec_ctrl_flags)(%rbx)
    jz .L\@_skip

    mov STACK_CPUINFO_FIELD(shadow_spec_ctrl)(%rbx), %eax
    mov $MSR_SPEC_CTRL, %ecx
    wrmsr

.L\@_skip:
.endm

.macro DO_SPEC_CTRL_EXIT_TO_GUEST
/*
 * Requires %eax=spec_ctrl, %rsp=regs/cpuinfo
 * Clobbers %rcx, %rdx
 *
 * When returning to guest context, set up SPEC_CTRL shadowing and load the
 * guest value.
 */
    /* Set up shadow value *before* enabling shadowing. */
    mov %eax, CPUINFO_shadow_spec_ctrl(%rsp)

    /* Set SPEC_CTRL shadowing *before* loading the guest value. */
    orb $SCF_use_shadow, CPUINFO_spec_ctrl_flags(%rsp)

    mov $MSR_SPEC_CTRL, %ecx
    xor %edx, %edx
    wrmsr
.endm

/* Use after an entry from PV context (syscall/sysenter/int80/int82/etc). */
#define SPEC_CTRL_ENTRY_FROM_PV                                         \
    ALTERNATIVE "", __stringify(DO_SPEC_CTRL_COND_IBPB maybexen=0),     \
        X86_FEATURE_IBPB_ENTRY_PV;                                      \
    ALTERNATIVE "", DO_OVERWRITE_RSB, X86_FEATURE_SC_RSB_PV;            \
    ALTERNATIVE "", __stringify(DO_SPEC_CTRL_ENTRY maybexen=0),         \
        X86_FEATURE_SC_MSR_PV

/* Use in interrupt/exception context.  May interrupt Xen or PV context. */
#define SPEC_CTRL_ENTRY_FROM_INTR                                       \
    ALTERNATIVE "", __stringify(DO_SPEC_CTRL_COND_IBPB maybexen=1),     \
        X86_FEATURE_IBPB_ENTRY_PV;                                      \
    ALTERNATIVE "", DO_OVERWRITE_RSB, X86_FEATURE_SC_RSB_PV;            \
    ALTERNATIVE "", __stringify(DO_SPEC_CTRL_ENTRY maybexen=1),         \
        X86_FEATURE_SC_MSR_PV

/* Use when exiting to Xen context. */
#define SPEC_CTRL_EXIT_TO_XEN                                           \
    ALTERNATIVE "",                                                     \
        DO_SPEC_CTRL_EXIT_TO_XEN, X86_FEATURE_SC_MSR_PV

/* Use when exiting to PV guest context. */
#define SPEC_CTRL_EXIT_TO_PV                                            \
    ALTERNATIVE "",                                                     \
        DO_SPEC_CTRL_EXIT_TO_GUEST, X86_FEATURE_SC_MSR_PV;              \
    DO_SPEC_CTRL_COND_VERW

/*
 * Use in IST interrupt/exception context.  May interrupt Xen or PV context.
 */
.macro SPEC_CTRL_ENTRY_FROM_INTR_IST
/*
 * Requires %rsp=regs, %r14=stack_end, %rdx=0
 * Clobbers %rax, %rbx, %rcx, %rdx
 *
 * This is logical merge of:
 *    DO_SPEC_CTRL_COND_IBPB maybexen=0
 *    DO_OVERWRITE_RSB
 *    DO_SPEC_CTRL_ENTRY maybexen=1
 * but with conditionals rather than alternatives.
 */
    movzbl STACK_CPUINFO_FIELD(spec_ctrl_flags)(%r14), %ebx

    test    $SCF_ist_ibpb, %bl
    jz      .L\@_skip_ibpb

    mov     $MSR_PRED_CMD, %ecx
    mov     $PRED_CMD_IBPB, %eax
    wrmsr

.L\@_skip_ibpb:

    test $SCF_ist_rsb, %bl
    jz .L\@_skip_rsb

    DO_OVERWRITE_RSB         /* Clobbers %rax/%rcx */

.L\@_skip_rsb:

    test $SCF_ist_sc_msr, %bl
    jz .L\@_skip_msr_spec_ctrl

    xor %eax, %eax
    testb $3, UREGS_cs(%rsp)
    setnz %al
    not %eax
    and %al, STACK_CPUINFO_FIELD(spec_ctrl_flags)(%r14)

    /* Load Xen's intended value. */
    mov $MSR_SPEC_CTRL, %ecx
    movzbl STACK_CPUINFO_FIELD(xen_spec_ctrl)(%r14), %eax
    wrmsr

    /* Opencoded UNLIKELY_START() with no condition. */
UNLIKELY_DISPATCH_LABEL(\@_serialise):
    .subsection 1
    /*
     * In the case that we might need to set SPEC_CTRL.IBRS for safety, we
     * need to ensure that an attacker can't poison the `jz .L\@_skip_wrmsr`
     * to speculate around the WRMSR.  As a result, we need a dispatch
     * serialising instruction in the else clause.
     */
.L\@_skip_msr_spec_ctrl:
    lfence
    UNLIKELY_END(\@_serialise)
.endm

/* Use when exiting to Xen in IST context. */
.macro SPEC_CTRL_EXIT_TO_XEN_IST
/*
 * Requires %rbx=stack_end
 * Clobbers %rax, %rcx, %rdx
 */
    testb $SCF_ist_sc_msr, STACK_CPUINFO_FIELD(spec_ctrl_flags)(%rbx)
    jz .L\@_skip

    DO_SPEC_CTRL_EXIT_TO_XEN

.L\@_skip:
.endm

#endif /* __ASSEMBLY__ */
#endif /* !__X86_SPEC_CTRL_ASM_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
