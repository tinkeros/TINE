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
  push r12
  push r13
  push r14
  push r15
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


FFI_CALL_TOS_1:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_2:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13 
  push r14
  push r15
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_3:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push rcx
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_4:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push r8
  push rcx
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_5:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push r9
  push r8
  push rcx
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_6:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x10]
  push r9
  push r8
  push rcx
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rsi
  pop rdi
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_7:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x18]
  push qword ptr [rbp + 0x10]
  push r9
  push r8
  push rcx
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_8:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push qword ptr [rbp + 0x20]
  push qword ptr [rbp + 0x18]
  push qword ptr [rbp + 0x10]
  push r9
  push r8
  push rcx
  push rdx
  push rsi
  call rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

FFI_CALL_TOS_0_ZERO_BP:
  push rbp
  push rbx
  push r12
  push r13
  push r14
  push r15
  push 0 # fake return address
  push 0 # fake rbp
  mov rbp, rsp # pass modified stack to TempleOS function
  call rdi
  add rsp, 0x10
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

// vim: set expandtab ts=2 sw=2 :
