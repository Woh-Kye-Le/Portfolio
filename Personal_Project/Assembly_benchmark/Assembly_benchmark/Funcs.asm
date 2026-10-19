; ============================================================================
; funcs.asm
;   Hand-written x64 assembly, "Can I beat the compiler?".
;
;   Toolchain : Microsoft MASM (ml64.exe), x64 only
;   ABI       : Microsoft x64 calling convention
;                 integer args -> RCX, RDX, R8, R9 (then the stack)
;                 return value -> RAX
;                 volatile     -> RAX, RCX, RDX, R8-R11, XMM0-XMM5
;
;   Every routine below is a leaf function: it calls nothing, uses no
;   non-volatile register and therefore needs no stack frame or shadow space.
; ============================================================================

.data
; BT.601 luma coefficients scaled by 256 (so the final divide is a >> 8).
; Stored as 16-bit words for PMADDWD: one [77,150,29,0] group per pixel lane.
align 16
gs_coeff WORD 77, 150, 29, 0, 77, 150, 29, 0

.code

; ----------------------------------------------------------------------------
; size_t asm_strlen(const char* s)                 RCX = s
; ----------------------------------------------------------------------------
asm_strlen PROC
    xor     eax, eax                    ; eax = length
asm_strlen_test:
    cmp     BYTE PTR [rcx + rax], 0
    je      asm_strlen_done
    inc     rax
    jmp     asm_strlen_test
asm_strlen_done:
    ret
asm_strlen ENDP

; ----------------------------------------------------------------------------
; int64_t asm_sum_array(const int64_t* arr, size_t n)     RCX = arr, RDX = n
; ----------------------------------------------------------------------------
asm_sum_array PROC
    xor     eax, eax
    test    rdx, rdx
    jz      asm_sum_done
asm_sum_loop:
    add     rax, QWORD PTR [rcx]
    add     rcx, 8
    dec     rdx
    jnz     asm_sum_loop
asm_sum_done:
    ret
asm_sum_array ENDP

; ----------------------------------------------------------------------------
; uint64_t asm_popcount_array(const uint64_t* arr, size_t n)
; ----------------------------------------------------------------------------
asm_popcount_array PROC
    xor     eax, eax
    test    rdx, rdx
    jz      asm_pc_done
asm_pc_loop:
    popcnt  r8, QWORD PTR [rcx]
    add     rax, r8
    add     rcx, 8
    dec     rdx
    jnz     asm_pc_loop
asm_pc_done:
    ret
asm_popcount_array ENDP

; ----------------------------------------------------------------------------
; void asm_reverse_array(int64_t* arr, size_t n)
;   Two pointers walking towards each other, swapping 8 bytes at a time.
; ----------------------------------------------------------------------------
asm_reverse_array PROC
    test    rdx, rdx
    jz      asm_rev_done
    lea     r8, [rcx + rdx*8 - 8]       ; r8 -> last element
asm_rev_loop:
    cmp     rcx, r8
    jae     asm_rev_done
    mov     rax, QWORD PTR [rcx]
    mov     r9,  QWORD PTR [r8]
    mov     QWORD PTR [rcx], r9
    mov     QWORD PTR [r8],  rax
    add     rcx, 8
    sub     r8, 8
    jmp     asm_rev_loop
asm_rev_done:
    ret
asm_reverse_array ENDP

; ----------------------------------------------------------------------------
; void asm_grayscale(const uint8_t* src, uint8_t* dst, size_t pixel_count)
;   src = RGBA8, dst = gray8.  RCX = src, RDX = dst, R8 = pixel_count
;
;   SSE2 path: four pixels (16 bytes) per iteration. Bytes are widened to
;   unsigned words, PMADDWD does the 77R+150G+29B dot product in 32-bit lanes
;   (no 16-bit overflow, unlike PMADDUBSW which saturates), PSHUFD/PADDD fold
;   the horizontal pairs, then PSRLD 8 divides by 256 and the two PACK steps
;   squeeze four 32-bit lanes down to four gray bytes.
;   A scalar loop mops up a trailing remainder (pixel_count % 4).
; ----------------------------------------------------------------------------
asm_grayscale PROC
    test    r8, r8
    jz      asm_gs_done
    cmp     r8, 4
    jb      asm_gs_tail
    movdqa  xmm5, XMMWORD PTR gs_coeff
    pxor    xmm4, xmm4                      ; zero, for byte -> word widening
asm_gs_vec:
    movdqu  xmm0, XMMWORD PTR [rcx]         ; 4 pixels = 16 bytes
    movdqa  xmm1, xmm0
    punpcklbw xmm0, xmm4                    ; words: R0 G0 B0 A0 R1 G1 B1 A1
    punpckhbw xmm1, xmm4                    ; words: R2 G2 B2 A2 R3 G3 B3 A3
    pmaddwd xmm0, xmm5                      ; dwords: 77R0+150G0, 29B0, 77R1+150G1, 29B1
    pmaddwd xmm1, xmm5                      ; dwords: 77R2+150G2, 29B2, 77R3+150G3, 29B3
    pshufd  xmm2, xmm0, 0B1h                ; swap adjacent dwords
    paddd   xmm0, xmm2                      ; -> g0, g0, g1, g1
    pshufd  xmm3, xmm1, 0B1h
    paddd   xmm1, xmm3                      ; -> g2, g2, g3, g3
    shufps  xmm0, xmm1, 088h                ; -> g0, g1, g2, g3
    psrld   xmm0, 8                         ; / 256
    packssdw xmm0, xmm0                     ; dwords -> words
    packuswb xmm0, xmm0                     ; words -> bytes
    movd    DWORD PTR [rdx], xmm0           ; store 4 gray pixels
    add     rcx, 16
    add     rdx, 4
    sub     r8, 4
    cmp     r8, 4
    jae     asm_gs_vec
asm_gs_tail:
    test    r8, r8
    jz      asm_gs_done
asm_gs_tail_loop:
    movzx   eax, BYTE PTR [rcx]         ; R
    imul    eax, eax, 77
    movzx   r9d, BYTE PTR [rcx + 1]     ; G
    imul    r9d, r9d, 150
    add     eax, r9d
    movzx   r9d, BYTE PTR [rcx + 2]     ; B
    imul    r9d, r9d, 29
    add     eax, r9d
    shr     eax, 8
    mov     BYTE PTR [rdx], al
    add     rcx, 4
    inc     rdx
    dec     r8
    jnz     asm_gs_tail_loop
asm_gs_done:
    ret
asm_grayscale ENDP

END
