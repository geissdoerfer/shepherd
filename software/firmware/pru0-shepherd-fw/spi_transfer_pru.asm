.macro NOP
   MOV r23, r23
.endm

    .global adc_readwrite
adc_readwrite:
    MOV r24, r14 ; Save input arg (CS pin)
    LDI r20, 16 ; Load Counter for outloop
    LDI r21, 18; Load Counter for inloop
    CLR r30, r30, r24 ; Set CS low
    LDI r14, 0 ; Clear return reg
adc_outloop:
    SUB r20, r20, 1 ; decrement shiftloop counter
    SET r30, r30, 3 ; Set SCLK high
    QBBC mosi_clear, r15, r20
    SET r30, r30, 2 ; Set MOSI high
    JMP skip_mosi_clear
mosi_clear:
    CLR r30, r30, 2 ; Set MOSI low
    NOP
skip_mosi_clear:
    NOP
    NOP
    CLR r30, r30, 3 ; Set SCLK low
    NOP
    NOP
    NOP
    QBLT adc_outloop, r20, 0
    CLR r30, r30, 2 ; clear MOSI
adc_inloop:
    SET r30, r30, 3 ; Set SCLK high
    SUB r21, r21, 1 ; decrement shiftloop counter
    NOP
    NOP
    NOP
    MOV r25, r31
    CLR r30, r30, 3 ; Set SCLK low
    QBBC adc_miso_clear, r25, 5
    SET r14, r14, r21
    JMP skip_adc_miso_clear
adc_miso_clear:
    NOP
    NOP
skip_adc_miso_clear:
    NOP
    QBLT adc_inloop, r21, 0
    SET r30, r30, r24 ; set CS high
    JMP r3.w2


    .global dac_write
dac_write:
    LDI r20, 24 ; Load Counter for outloop
    CLR r30, r30, r14 ; Set CS low
    NOP

dac_loop:
    SUB r20, r20, 1 ; Decrement counter
    SET r30, r30, 3 ; Set SCLK high
    QBBS set_mosi, r15, r20 ; If bit number [r20] is set in value [r15]
    CLR r30, r30, 2 ; Set MOSI low
    JMP skip_mosi_set
set_mosi:
    SET r30, r30, 2 ; Set MOSI high
    NOP
skip_mosi_set:
    CLR r30, r30, 3 ; Set SCLK low
    NOP
    QBLT dac_loop, r20, 0
    CLR r30, r30, 2 ; clear MOSI
    NOP
    SET r30, r30, r14 ; set CS high
    JMP r3.w2
