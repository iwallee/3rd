;*****************************************************************************
;* mc-a2.asm: h264 encoder library
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

ALIGN 16
mmx_dw_one:
    times 4 dw 16
mmx_dd_one:
    times 2 dd 512
mmx_dw_20:
    times 4 dw 20
mmx_dw_5:
    times 4 dw -5



;=============================================================================
; Macros
;=============================================================================

%macro LOAD_4 9
    movd %1, %5
    movd %2, %6
    movd %3, %7
    movd %4, %8
    punpcklbw %1, %9
    punpcklbw %2, %9
    punpcklbw %3, %9
    punpcklbw %4, %9
%endmacro

%macro FILT_2 2
    psubw %1, %2
    psllw %2, 2
    psubw %1, %2
%endmacro

%macro FILT_4 3
    paddw %2, %3
    psllw %2, 2
    paddw %1, %2
    psllw %2, 2
    paddw %1, %2
%endmacro

%macro FILT_6 4
    psubw %1, %2
    psllw %2, 2
    psubw %1, %2
    paddw %1, %3
    paddw %1, %4
    psraw %1, 5
%endmacro

%macro FILT_ALL 1
    LOAD_4      mm1, mm2, mm3, mm4, [%1], [%1 + ecx], [%1 + 2 * ecx], [%1 + eax], mm0
    FILT_2      mm1, mm2
    movd        mm5, [%1 + 4 * ecx]
    movd        mm6, [%1 + edx]
    FILT_4      mm1, mm3, mm4
    punpcklbw   mm5, mm0
    punpcklbw   mm6, mm0
    psubw       mm1, mm5
    psllw       mm5, 2
    psubw       mm1, mm5
    paddw       mm1, mm6
%endmacro




;=============================================================================
; Code
;=============================================================================

SECTION .text

cglobal x264_vertical_filter_mmxext
cglobal x264_horizontal_filter_mmxext
cglobal x264_center_filter_mmxext

;-----------------------------------------------------------------------------
;
; void x264_center_filter_mmxext( uint8_t *dst1, int i_dst1_stride,
;                                 uint8_t *dst2, int i_dst2_stride,
;                                  uint8_t *src, int i_src_stride,
;                                  int i_width, int i_height );
;
;-----------------------------------------------------------------------------

ALIGN 16
x264_center_filter_mmxext :

    push        edi
    push        esi
    push        ebx
    push        ebp

    mov         ebx,      esp

    mov         esi,      [esp + 36]         ; src

    ;mov         edx,      [esp + 20]         ; dst1
    ;mov         [dst1],   edx

    ;mov         edi,      [esp + 28]         ; dst2
    ;mov         [dst2],   edi

    ;mov         eax,      [esp + 44]         ; width
    ;mov         [width],  eax

    ;mov         eax,      [esp + 48]         ; height
    ;mov         [height], eax

    ;mov         eax,      [esp + 24]         ; dst1_stride
    ;mov         [dstp1],  eax

    ;mov         eax,      [esp + 32]         ; dst2_stride
    ;mov         [dstp2],  eax

    mov         ecx,      [esp + 40]         ; src_stride

    ;sub         esp,      8                ;save for buffer

    sub         esp,      ecx
    sub         esp,      ecx                ; esp is now at the beginning of the buffer
              
    mov         [ebx+52], esp             ;buffer

    ;sub        esi,      2
    sub         esi,      ecx
    sub         esi,      ecx                ; esi - 2 - 2 * stride
    mov         [ebx + 36],    esi       ;src

    ;sub        edi,      2

    mov         eax,      ecx
    shl         eax,      1
    add         eax,      ecx                ; 3 * src_stride
    

    mov         edx,      ecx
    shl         edx,      1
    add         edx,      eax                ; 5 * src_stride
    mov         [ebx+56],  eax

    pxor        mm0,      mm0                ; 0 ---> mm0
    movq        mm7,      [mmx_dd_one]       ; for rounding

    mov         ebp,      [ebx+48]   ;height

loopcy:

    dec         ebp
    mov         eax,    [ebx + 44]  ;width
    mov         edi,    [ebx + 20]  ;dst1
    mov         esp,    [ebx+52]     ;buffer
    mov         esi,    [ebx + 36]  ;src

    push        eax
    mov         eax,    [ebx+56]
    FILT_ALL    esi
    pop         eax

    pshufw      mm2,    mm1, 0
    movq        [esp],  mm2
    add         esp,    8
    movq        [esp],  mm1
    add         esp,    8
    paddw       mm1,    [mmx_dw_one]
    psraw       mm1,    5

    packuswb    mm1,    mm1
    movd        [edi],  mm1

    sub         eax,    8
    add         edi,    4
    add         esi,    4

loopcx1:

    sub         eax,    4

    push        eax
    mov         eax,    [ebx+56]
    FILT_ALL    esi
    pop         eax

    movq        [esp],  mm1
    paddw       mm1,    [mmx_dw_one]
    psraw       mm1,    5
    packuswb    mm1,    mm1
    movd        [edi],  mm1

    add         esp,    8
    add         esi,    4
    add         edi,    4
    test        eax,    eax
    jnz         loopcx1

    push        eax
    mov         eax,    [ebx+56]
    FILT_ALL    esi
    pop         eax

    pshufw      mm2,    mm1,  7
    movq        [esp],  mm1
    add         esp,    8
    movq        [esp],  mm2
    paddw       mm1,    [mmx_dw_one]
    psraw       mm1,    5
    packuswb    mm1,    mm1
    movd        [edi],  mm1

    mov         esi,    [ebx + 36]  ;src
    add         esi,    ecx
    mov         [ebx + 36],  esi

    mov         edi,    [ebx + 20] ;dst1
    add         edi,    [ebx + 24] ;dstp1
    mov         [ebx + 20], edi

    mov         eax,    [ebx + 44]  ;width
    mov         edi,    [ebx + 28]  ;dst2
    mov         esp,    [ebx+52]     ;buffer
    add         esp,    4

loopcx2:

    sub         eax,    4

    movq        mm2,    [esp + 2 * eax + 2]
    movq        mm3,    [esp + 2 * eax + 4]
    movq        mm4,    [esp + 2 * eax + 6]
    movq        mm5,    [esp + 2 * eax + 8]
    movq        mm1,    [esp + 2 * eax]
    movq        mm6,    [esp + 2 * eax + 10]
    paddw       mm2,    mm5
    paddw       mm3,    mm4
    paddw       mm1,    mm6

    movq        mm5,    [mmx_dw_20]
    movq        mm4,    [mmx_dw_5]
    movq        mm6,    mm1
    pxor        mm7,    mm7

    punpckhwd   mm5,    mm2
    punpcklwd   mm4,    mm3
    punpcklwd   mm2,    [mmx_dw_20]
    punpckhwd   mm3,    [mmx_dw_5]

    pcmpgtw     mm7,    mm1

    pmaddwd     mm2,    mm4
    pmaddwd     mm3,    mm5

    punpcklwd   mm1,    mm7
    punpckhwd   mm6,    mm7

    paddd       mm2,    mm1
    paddd       mm3,    mm6

    paddd       mm2,    [mmx_dd_one]
    paddd       mm3,    [mmx_dd_one]

    psrad       mm2,    10
    psrad       mm3,    10

    packssdw    mm2,    mm3
    packuswb    mm2,    mm0

    movd        [edi + eax], mm2

    test        eax,    eax
    jnz         loopcx2

    add         edi,    [ebx + 32] ;dstp2
    mov         [ebx + 28], edi    ;dst2

    test        ebp,    ebp
    jnz         loopcy

    mov         esp,    [ebx+52]   ;buffer
    shl         ecx,    1
    add         esp,    ecx

    pop         ebp
    pop         ebx
    pop         esi
    pop         edi

    ret

;-----------------------------------------------------------------------------
;
; void x264_horizontal_filter_mmxext( uint8_t *dst, int i_dst_stride,
;                                     uint8_t *src, int i_src_stride,
;                                     int i_width, int i_height );
;
;-----------------------------------------------------------------------------

ALIGN 16
x264_horizontal_filter_mmxext :
    push edi
    push esi

    mov         edi,    [esp + 12]           ; dst
    mov         esi,    [esp + 20]           ; src

    pxor        mm0,    mm0
    movq        mm7,    [mmx_dw_one]

    mov         ecx,    [esp + 32]           ; height

    sub         esi,    2

loophy:

    dec         ecx
    mov         eax,    [esp + 28]           ; width

loophx:

    sub         eax,    8

    LOAD_4      mm1,    mm2, mm3, mm4, [esi + eax], [esi + eax + 1], [esi + eax + 2], [esi + eax + 3], mm0
    FILT_2      mm1,    mm2
    movd        mm5,    [esi + eax + 4]
    movd        mm6,    [esi + eax + 5]
    FILT_4      mm1,    mm3, mm4
    movd        mm2,    [esi + eax + 4]
    movd        mm3,    [esi + eax + 6]
    punpcklbw   mm5,    mm0
    punpcklbw   mm6,    mm0
    FILT_6      mm1,    mm5, mm6, mm7
    movd        mm4,    [esi + eax + 7]
    movd        mm5,    [esi + eax + 8]
    punpcklbw   mm2,    mm0
    punpcklbw   mm3,    mm0                  ; mm2(1), mm3(20), mm6(-5) ready
    FILT_2      mm2,    mm6
    movd        mm6,    [esi + eax + 9]
    punpcklbw   mm4,    mm0
    punpcklbw   mm5,    mm0                  ; mm2(1-5), mm3(20), mm4(20), mm5(-5) ready
    FILT_4      mm2,    mm3, mm4
    punpcklbw   mm6,    mm0
    FILT_6      mm2,    mm5, mm6, mm7

    packuswb    mm1,    mm2
    movq        [edi + eax],  mm1

    test        eax,    eax
    jnz         loophx

    add         esi,    [esp + 24]           ; src_pitch
    add         edi,    [esp + 16]           ; dst_pitch

    test        ecx,    ecx
    jnz         loophy

    pop         esi
    pop         edi

    ret
