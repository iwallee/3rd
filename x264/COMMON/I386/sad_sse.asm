;/*****************************************************************************
; *
; *  X264  CODEC
; *
; *
; ****************************************************************************/

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

; from xvid
%macro new_sad16b 0
    movdqu  xmm0, [edx]
    movdqu  xmm1, [edx+ebx]
    lea edx,[edx+2*ebx]
    movdqa  xmm2, [eax]
    movdqa  xmm3, [eax+ecx]
    lea eax,[eax+2*ecx]
    psadbw  xmm0, xmm2
    paddusw xmm6,xmm0
    psadbw  xmm1, xmm3
    paddusw xmm6,xmm1
%endmacro

section .text

;======================================================
;
; uint32_t 
; x264_pixel_sad_16x16_sse(uint8_t* src, int32_t src_stride, uint8_t* data, int32_t dst_stride);
;
;======================================================

align 16

cglobal x264_pixel_sad_16x16_sse
x264_pixel_sad_16x16_sse
   
    push ebx
    
    mov eax, [esp + 4 + 4]      ; src
    mov ecx, [esp + 8 + 4]      ; src_stride
    mov edx, [esp + 12 + 4]     ; data
    mov ebx, [esp + 16 + 4]     ; dst_stride
    
    pxor xmm6, xmm6

    new_sad16b
    new_sad16b
    new_sad16b
    new_sad16b
    new_sad16b
    new_sad16b
    new_sad16b
    new_sad16b
    
    pshufd  xmm5, xmm6, 00000010b
    paddusw xmm6, xmm5
    pextrw  eax, xmm6, 0
    
    pop ebx
    
    ret
    
;======================================================
;
; uint32_t 
; x264_pixel_sad_16x8_sse(uint8_t* src, int32_t src_stride, uint8_t* data, int32_t dst_stride);
;
;======================================================

align 16

cglobal x264_pixel_sad_16x8_sse
x264_pixel_sad_16x8_sse
    
    push ebx
    
    mov eax, [esp + 4 + 4]      ; src
    mov ecx, [esp + 8 + 4]      ; src_stride
    mov edx, [esp + 12 + 4]     ; data
    mov ebx, [esp + 16 + 4]     ; dst_stride
    
    pxor xmm6, xmm6

    new_sad16b
    new_sad16b
    new_sad16b
    new_sad16b
    
    pshufd  xmm5, xmm6, 00000010b
    paddusw xmm6, xmm5
    pextrw  eax, xmm6, 0
    
    pop ebx

    ret
 
