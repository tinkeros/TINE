.intel_syntax noprefix
.global FFI_CALL_TOS_0
.global FFI_CALL_TOS_1
.global FFI_CALL_TOS_2
.global FFI_CALL_TOS_3
.global FFI_CALL_TOS_4
.global FFI_CALL_TOS_5
.global FFI_CALL_TOS_6
.global FFI_CALL_TOS_7
.global FFI_CALL_TOS_8
.global FFI_CALL_TOS_0_ZERO_BP

FFI_CALL_TOS_0:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_1:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_2:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_3:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push r9
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

# https://archive.md/4HDA0#selection-2085.429-2085.1196
# first stack arg: +0x30
# r9 home  +0x28 
# r8 home  +0x20
# rdx home +0x18
# rcx home +0x10
# ret addr +0x8
#          rbp 
FFI_CALL_TOS_4:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x30]
  push r9
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_5:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x38]
  push qword ptr [rbp + 0x30]
  push r9
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_6:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x40]
  push qword ptr [rbp + 0x38]
  push qword ptr [rbp + 0x30]
  push r9
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_7:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x48]
  push qword ptr [rbp + 0x40]
  push qword ptr [rbp + 0x38]
  push qword ptr [rbp + 0x30]
  push r9
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_8:
  push rbp
  mov rbp, rsp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x50]
  push qword ptr [rbp + 0x48]
  push qword ptr [rbp + 0x40]
  push qword ptr [rbp + 0x38]
  push qword ptr [rbp + 0x30]
  push r9
  push r8
  push rdx
  call rcx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  leave
  ret

FFI_CALL_TOS_0_ZERO_BP:
  push rbp
  push rbx
  push rdi
  push rsi
  push r12
  push r13
  push r14
  push r15
  push 0 # fake return address
  push 0 # fake rbp(stacktrace ends here)
  mov rbp, rsp
  call rcx
  add rsp, 0x10
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  pop rbp
  ret

// vim: set expandtab ts=2 sw=2 :
