/**
 * Context switches to new_context from old_context.
 *
 * Saves the current execution context into the `Context` structure pointed to
 * by the first paramter and restores the execution context pointed to by the
 * second paramter. As such, when this function returns, it jumps to the address
 * at the top of the stack (pointed to by %rsp).
 *
 * This function has the following signature in C:
 * context_switch(Context* old_context, Context* new_context)
 */
.text
.globl context_switch
.align 16
context_switch:

  /* FIXME: Phase 2
   * Save and restore execution context.
   * Part of the code is provided.
   */

  movq    %rsp, 0x0(%rdi)
  movq    %r15, 0x8(%rdi)
  movq    %r14, 0x10(%rdi)
  movq    %r13, 0x18(%rdi)
  movq    %r12, 0x20(%rdi)
  movq    %rbx, 0x28(%rdi)
  movq    %rbp, 0x30(%rdi)
  stmxcsr 0x38(%rdi)
  fnstcw  0x3C(%rdi)

  movq    0x0(%rsi), %rsp
  movq    0x8(%rsi), %r15
  movq    0x10(%rsi), %r14
  movq    0x18(%rsi), %r13
  movq    0x20(%rsi), %r12
  movq    0x28(%rsi), %rbx
  movq    0x30(%rsi), %rbp
  ldmxcsr 0x38(%rsi)
  fldcw   0x3C(%rsi)

  ret

/**
 * Initial function implicitly executed by a thread.
 *
 * This function should be jumped into implicitly by the green threads library.
 * It expects a function pointer at the top of the stack, and its argument above
 * that. It sets up the arguments in the right registers accordingly, and call
 * `thread_entry` which will handle subsequent initialization.
 */
.globl start_thread
.align 16
start_thread:
  push    %rbp
  movq    %rsp, %rbp
  movq    0x8(%rbp), %rdi
  movq    0x10(%rbp), %rsi
  callq   thread_entry
  hlt
