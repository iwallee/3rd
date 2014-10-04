bits 32

; ideal from xvid
; for GCC
%macro cglobal 1
	%ifdef NOPREFIX
		global %1
	%else
		global _%1
		%define %1 _%1
	%endif
%endmacro

%macro cextern 1
	%ifdef NOPREFIX
		extern %1
	%else
		extern _%1
		%define %1 _%1
	%endif
%endmacro


section .rodata data align=16

align 16
    sse2_neg1 dw -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    sse2_1 dw 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1

cextern quant_mf
cextern dequant_mf

align 16

section .text



;======================================================
;
; void
; quant4x4_sse2(int16_t* data, const int32_t Qp, int32_t is_intra)
;
;======================================================

align 16

cglobal quant4x4_sse2
quant4x4_sse2

    push ebx
    push esi
    push edi
    push ebp
    
    mov  edi, [esp + 4 + 16]  ; data
    mov  eax, [esp + 8 + 16]  ; qp
    cdq
    mov  ebp, [esp + 12 + 16] ; is_intra
    mov  ebx, 6

    idiv ebx
    add  eax, 15         ; qbits(eax) = 15 + qp / 6, mf_index(edx) = qp % 6    
    mov  esi, edx
    shl  esi, 5
    add  esi, quant_mf     ; esi = quant[mf_index]
    mov  ecx, eax        ; ecx = qbits
    
    neg  ebp
    sbb  ebp, ebp
    and  ebp, 0xfffffffd
    add  ebp, 6          ; is_intra(ecx) ? 3 : 6
    
    mov  eax, 1
    shl  eax, cl         ; 1 << qbits
    cdq
    idiv ebp             ; 1 << qbits / is_intra(ecx) ? 3 : 6
    
    ; eax = f, ecx = qbits, esi = quant[mf_index], edi = data
    
    movd      mm0, eax
    movd      mm1, ecx
    pshufw    mm0, mm0, 0x44
    movq2dq   xmm6, mm0
    movq2dq   xmm7, mm1
    pshufd    xmm6, xmm6, 0x44        ; f
    pxor      mm3, mm3
    
    movdqa    xmm0, [edi + 0]         ; data
    movdqa    xmm1, [esi + 0]         ; quant
    
    ; > 0
    pxor      xmm4, xmm4
    movdqa    xmm2, xmm0
    pcmpgtw   xmm0, xmm4
    movdqa    xmm4, xmm0
    pand      xmm0, xmm2
    movdqa    xmm3, xmm0
    pmullw    xmm0, xmm1              ; low part
    pmulhw    xmm3, xmm1              ; high part
    movdqa    xmm5, xmm0
    punpcklwd xmm0, xmm3              ; low 4 - 32 bits
    punpckhwd xmm5, xmm3              ; high 4 - 32 bits
    movdqa    xmm3, xmm4
    punpcklwd xmm4, xmm4
    pand      xmm4, xmm6
    paddd     xmm0, xmm4              ; data * quant + f
    psrad     xmm0, xmm7              ; data * quant + f >> qbits
    punpckhwd xmm3, xmm3
    pand      xmm3, xmm6
    paddd     xmm5, xmm3              ; data * quant + f
    psrad     xmm5, xmm7              ; data * quant + f >> qbits
    packssdw  xmm0, xmm5
    
    ; < 0
    pxor      xmm4, xmm4
    movdqa    xmm5, xmm2
    pcmpgtw   xmm4, xmm2
    pand      xmm5, xmm4
    pmullw    xmm5, [sse2_neg1]
    movdqa    xmm3, xmm5
    pmullw    xmm5, xmm1
    pmulhw    xmm3, xmm1
    movdqa    xmm1, xmm5
    punpcklwd xmm5, xmm3
    punpckhwd xmm1, xmm3
    movdqa    xmm3, xmm4
    punpcklwd xmm4, xmm4
    pand      xmm4, xmm6
    paddd     xmm5, xmm4            ; data * quant - f
    psrad     xmm5, xmm7
    punpckhwd xmm3, xmm3
    pand      xmm3, xmm6
    paddd     xmm1, xmm3
    psrad     xmm1, xmm7
    packssdw  xmm5, xmm1
    pmullw    xmm5, [sse2_neg1]
    
    por       xmm5, xmm0
    movdqa    [edi + 0], xmm5

    movdqa    xmm0, [edi + 16]         ; data
    movdqa    xmm1, [esi + 16]         ; quant
    
    ; > 0
    pxor      xmm4, xmm4
    movdqa    xmm2, xmm0
    pcmpgtw   xmm0, xmm4
    movdqa    xmm4, xmm0
    pand      xmm0, xmm2
    movdqa    xmm3, xmm0
    pmullw    xmm0, xmm1              ; low part
    pmulhw    xmm3, xmm1              ; high part
    movdqa    xmm5, xmm0
    punpcklwd xmm0, xmm3              ; low 4 - 32 bits
    punpckhwd xmm5, xmm3              ; high 4 - 32 bits
    movdqa    xmm3, xmm4
    punpcklwd xmm4, xmm4
    pand      xmm4, xmm6
    paddd     xmm0, xmm4              ; data * quant + f
    psrad     xmm0, xmm7              ; data * quant + f >> qbits
    punpckhwd xmm3, xmm3
    pand      xmm3, xmm6
    paddd     xmm5, xmm3              ; data * quant + f
    psrad     xmm5, xmm7              ; data * quant + f >> qbits
    packssdw  xmm0, xmm5
    
    ; < 0
    pxor      xmm4, xmm4
    movdqa    xmm5, xmm2
    pcmpgtw   xmm4, xmm2
    pand      xmm5, xmm4
    pmullw    xmm5, [sse2_neg1]
    movdqa    xmm3, xmm5
    pmullw    xmm5, xmm1
    pmulhw    xmm3, xmm1
    movdqa    xmm1, xmm5
    punpcklwd xmm5, xmm3
    punpckhwd xmm1, xmm3
    movdqa    xmm3, xmm4
    punpcklwd xmm4, xmm4
    pand      xmm4, xmm6
    paddd     xmm5, xmm4            ; data * quant - f
    psrad     xmm5, xmm7
    punpckhwd xmm3, xmm3
    pand      xmm3, xmm6
    paddd     xmm1, xmm3
    psrad     xmm1, xmm7
    packssdw  xmm5, xmm1
    pmullw    xmm5, [sse2_neg1]
    
    por       xmm5, xmm0
    movdqa    [edi + 16], xmm5

    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

;======================================================
;
; void
; iquant4x4_sse2(int16_t* data, const int32_t Qp)
;
;======================================================

align 16

cglobal iquant4x4_sse2
iquant4x4_sse2

    mov  eax, [esp + 8]  ; qp
    cdq
    mov  ecx, 6

    idiv ecx             ; qbits(eax) = qp / 6, mf_index(edx) = qp % 6    
    mov  ecx, edx
    shl  ecx, 5
    add  ecx, dequant_mf   ; ecx = quant[mf_index]
    mov  edx, [esp + 4]  ; data
    
    ; eax = qbits, ecx = quant[mf_index], edx = data
    
    movdqa xmm6, [sse2_1]
    movdqa xmm0, [edx + 0]
    movdqa xmm2, [edx + 16]
    movdqa xmm1, [ecx + 0]
    movdqa xmm3, [ecx + 16]

    pmullw  xmm0, xmm1
    pmullw  xmm2, xmm3

    movd    xmm7, eax
    psllw   xmm6, xmm7      ; << qbits

    pmullw xmm0, xmm6
    pmullw xmm2, xmm6

    movdqa [edx + 0], xmm0
    movdqa [edx + 16], xmm2

    ret    