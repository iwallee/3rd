;*****************************************************************************
;* predict-a.asm: h264 encoder library
;*****************************************************************************


BITS 32

;=============================================================================
; Macros and other preprocessor constants
;=============================================================================

%macro cglobal 1
    %ifdef PREFIX
        global _%1
        %define %1 _%1
    %else
        global %1
    %endif
%endmacro

;=============================================================================
; Read only data
;=============================================================================

SECTION .rodata data align=16
align 16
    mmx_mask128 db 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80

SECTION .data

;=============================================================================
; Macros
;=============================================================================

%macro SAVE_0_1 1
    movq        [%1]         , mm0
    movq        [%1 + 8]     , mm1
%endmacro

;=============================================================================
; Code
;=============================================================================

SECTION .text

cglobal predict_8x8_v_mmx
cglobal predict_16x16_v_mmx
cglobal	predict_8x8_dc_128_mmx
cglobal	predict_4x4_dc_128_mmx
cglobal predict_16x16_v_sse2

;-----------------------------------------------------------------------------
;
; void predict_8x8_v_mmx( uint8_t *src, int i_stride )
;
;-----------------------------------------------------------------------------

ALIGN 16
predict_8x8_v_mmx :

    ;push       edi
    ;push       esi

    mov         edx             , [esp + 4]
    mov         ecx             , [esp + 8]
    sub         edx             , ecx               ; esi <-- line -1

    movq        mm0             , [edx]
    movq        [edx + ecx]     , mm0               ; 0
    movq        [edx + 2 * ecx] , mm0               ; 1
    movq        [edx + 4 * ecx] , mm0               ; 3
    movq        [edx + 8 * ecx] , mm0               ; 7
    add         edx             , ecx               ; esi <-- line 0
    movq        [edx + 2 * ecx] , mm0               ; 2
    movq        [edx + 4 * ecx] , mm0               ; 4
    lea         edx             , [edx + 4 * ecx]   ; esi <-- line 4
    movq        [edx + ecx]     , mm0               ; 5
    movq        [edx + 2 * ecx] , mm0               ; 6

    ;pop        esi
    ;pop        edi

    ret

;-----------------------------------------------------------------------------
;
; void predict_16x16_v_mmx( uint8_t *src, int i_stride )
;
;-----------------------------------------------------------------------------

ALIGN 16
predict_16x16_v_mmx :

    ;push       edi
    ;push       esi

    mov         edx, [esp + 4]
    mov         ecx, [esp + 8]
    sub         edx, ecx                ; esi <-- line -1

    movq        mm0, [edx]
    movq        mm1, [edx + 8]
    mov         eax, ecx
    shl         eax, 1
    add         eax, ecx                ; eax <-- 3* stride

    SAVE_0_1    (edx + ecx)             ; 0
    SAVE_0_1    (edx + 2 * ecx)         ; 1
    SAVE_0_1    (edx + eax)             ; 2
    SAVE_0_1    (edx + 4 * ecx)         ; 3
    SAVE_0_1    (edx + 2 * eax)         ; 5
    SAVE_0_1    (edx + 8 * ecx)         ; 7
    SAVE_0_1    (edx + 4 * eax)         ; 11
    add         edx, ecx                ; esi <-- line 0
    SAVE_0_1    (edx + 4 * ecx)         ; 4
    SAVE_0_1    (edx + 2 * eax)         ; 6
    SAVE_0_1    (edx + 8 * ecx)         ; 8
    SAVE_0_1    (edx + 4 * eax)         ; 12
    lea         edx, [edx + 8 * ecx]    ; esi <-- line 8
    SAVE_0_1    (edx + ecx)             ; 9
    SAVE_0_1    (edx + 2 * ecx)         ; 10
    lea         edx, [edx + 4 * ecx]    ; esi <-- line 12
    SAVE_0_1    (edx + ecx)             ; 13
    SAVE_0_1    (edx + 2 * ecx)         ; 14
    SAVE_0_1    (edx + eax)             ; 15


    ;pop        esi
    ;pop        edi

    ret

;-----------------------------------------------------------------------------
;
; * 8x8 prediction for intra chroma block DC, H, V, P
; void predict_8x8_dc_128_mmx( uint8_t *src, int i_stride )
;
;-----------------------------------------------------------------------------

ALIGN 16
predict_8x8_dc_128_mmx :

	mov         edx             , [esp + 4]
    mov         ecx             , [esp + 8]
    sub         edx             , ecx               ; esi <-- line -1

	movq mm0, [mmx_mask128]
	;movq mm2, [mmx_mask128]
	;movq mm3, [mmx_mask128]
    ;movq        mm0             , [edx]

    movq        [edx + ecx]     , mm0               ; 0
    movq        [edx + 2 * ecx] , mm0               ; 1
    movq        [edx + 4 * ecx] , mm0               ; 3
    movq        [edx + 8 * ecx] , mm0               ; 7
    add         edx             , ecx               ; esi <-- line 0
    movq        [edx + 2 * ecx] , mm0               ; 2
    movq        [edx + 4 * ecx] , mm0               ; 4
    lea         edx             , [edx + 4 * ecx]   ; esi <-- line 4
    movq        [edx + ecx]     , mm0               ; 5
    movq        [edx + 2 * ecx] , mm0               ; 6

	ret

;-----------------------------------------------------------------------------
;
; * 8x8 prediction for intra chroma block DC, H, V, P
; void predict_4x4_dc_128_mmx( uint8_t *src, int i_stride )
;
;-----------------------------------------------------------------------------

ALIGN 16
predict_4x4_dc_128_mmx :

	mov         edx             , [esp + 4]
    mov         ecx             , [esp + 8]
    sub         edx             , ecx               ; esi <-- line -1

	movd mm0, [mmx_mask128]

    movd        [edx + ecx]     , mm0               ; 0
    movd        [edx + 2 * ecx] , mm0               ; 1
    movd        [edx + 4 * ecx] , mm0               ; 3
    add         edx             , ecx               ; esi <-- line 0
    movd        [edx + 2 * ecx] , mm0               ; 2
   
	ret

;-----------------------------------------------------------------------------
;
; void predict_16x16_v_sse( uint8_t *src, int i_stride )
;
;-----------------------------------------------------------------------------

ALIGN 16
predict_16x16_v_sse2 :

    ;push       edi
    ;push       esi

    mov         edx, [esp + 4]
    mov         ecx, [esp + 8]
    sub         edx, ecx                ; esi <-- line -1

    movdqa        xmm0, [edx]
    mov         eax, ecx
    shl         eax, 1
    add         eax, ecx                ; eax <-- 3* stride

    movdqa    [edx + ecx],  xmm0            ; 0
    movdqa    [edx + 2 * ecx],  xmm0         ; 1
    movdqa    [edx + eax],  xmm0             ; 2
    movdqa    [edx + 4 * ecx],  xmm0         ; 3
    movdqa    [edx + 2 * eax],  xmm0         ; 5
    movdqa    [edx + 8 * ecx],  xmm0         ; 7
    movdqa    [edx + 4 * eax],  xmm0         ; 11
    add         edx, ecx                ; esi <-- line 0
    movdqa    [edx + 4 * ecx],  xmm0         ; 4
    movdqa    [edx + 2 * eax],  xmm0         ; 6
    movdqa    [edx + 8 * ecx],  xmm0         ; 8
    movdqa    [edx + 4 * eax],  xmm0         ; 12
    lea         edx, [edx + 8 * ecx]    ; esi <-- line 8
    movdqa    [edx + ecx],  xmm0             ; 9
    movdqa    [edx + 2 * ecx],  xmm0         ; 10
    lea         edx, [edx + 4 * ecx]    ; esi <-- line 12
    movdqa    [edx + ecx],  xmm0             ; 13
    movdqa    [edx + 2 * ecx],  xmm0        ; 14
    movdqa    [edx + eax],  xmm0            ; 15


    ;pop        esi
    ;pop        edi

    ret

