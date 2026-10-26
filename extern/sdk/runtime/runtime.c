static const unsigned long long __constants[] = {
    0x0000000000000000, // 0.0
    0x41F0000000000000, // 2**32
    0x41E0000000000000, // 2**31
};

/* clang-format off */
asm unsigned long __cvt_fp2unsigned(register double d) {
  nofralloc
  stwu r1, -0x10(r1)
  lis r4, __constants@h
  ori r4, r4, __constants@l
  li r3, 0
  lfd f0, 0(r4)
  lfd f3, 8(r4)
  lfd f4, 0x10(r4)
  fcmpu cr0, f1, f0
  fcmpu cr6, f1, f3
  blt return_unsigned
  addi r3, r3, -1
  bge cr6, return_unsigned
  fcmpu cr7, f1, f4
  fmr f2, f1
  blt cr7, convert_integer
  fsub f2, f1, f4
convert_integer:
  fctiwz f2, f2
  stfd f2, 8(r1)
  lwz r3, 0xc(r1)
  blt cr7, return_unsigned
  addis r3, r3, 0x8000
return_unsigned:
  addi r1, r1, 0x10
  blr
}


asm void __div2u(void) {
  nofralloc
  cmpwi r3, 0
  cntlzw r0, r3
  cntlzw r9, r4
  bne count_divisor_bits
  addi r0, r9, 0x20
count_divisor_bits:
  cmpwi r5, 0
  cntlzw r9, r5
  cntlzw r10, r6
  bne compare_sizes
  addi r9, r10, 0x20
compare_sizes:
  cmpw r0, r9
  subfic r10, r0, 0x40
  bgt zero_quotient
  addi r9, r9, 1
  subfic r9, r9, 0x40
  add r0, r0, r9
  subf r9, r9, r10
  mtctr r9
  cmpwi r9, 0x20
  addi r7, r9, -32
  blt shift_remainder_low
  srw r8, r3, r7
  li r7, 0
  b shift_dividend
shift_remainder_low:
  srw r8, r4, r9
  subfic r7, r9, 0x20
  slw r7, r3, r7
  or r8, r8, r7
  srw r7, r3, r9
shift_dividend:
  cmpwi r0, 0x20
  addic r9, r0, -32
  blt shift_dividend_low
  slw r3, r4, r9
  li r4, 0
  b start_division
shift_dividend_low:
  slw r3, r3, r0
  subfic r9, r0, 0x20
  srw r9, r4, r9
  or r3, r3, r9
  slw r4, r4, r0
start_division:
  li r10, -1
  addic r7, r7, 0
divide_bit:
  adde r4, r4, r4
  adde r3, r3, r3
  adde r8, r8, r8
  adde r7, r7, r7
  subfc r0, r6, r8
  subfe. r9, r5, r7
  blt next_bit
  mr r8, r0
  mr r7, r9
  addic r0, r10, 1
next_bit:
  bdnz divide_bit
  adde r4, r4, r4
  adde r3, r3, r3
  blr
zero_quotient:
  li r4, 0
  li r3, 0
  blr
}

asm void __div2i(void) {
  nofralloc
  stwu r1, -0x10(r1)
  rlwinm. r9, r3, 0, 0, 0
  beq save_dividend_sign
  subfic r4, r4, 0
  subfze r3, r3
save_dividend_sign:
  stw r9, 8(r1)
  rlwinm. r9, r5, 0, 0, 0
  beq save_divisor_sign
  subfic r6, r6, 0
  subfze r5, r5
save_divisor_sign:
  stw r9, 0xc(r1)
  cmpwi r3, 0
  cntlzw r0, r3
  cntlzw r9, r4
  bne count_divisor_bits
  addi r0, r9, 0x20
count_divisor_bits:
  cmpwi r5, 0
  cntlzw r9, r5
  cntlzw r10, r6
  bne compare_sizes
  addi r9, r10, 0x20
compare_sizes:
  cmpw r0, r9
  subfic r10, r0, 0x40
  bgt zero_quotient
  addi r9, r9, 1
  subfic r9, r9, 0x40
  add r0, r0, r9
  subf r9, r9, r10
  mtctr r9
  cmpwi r9, 0x20
  addi r7, r9, -32
  blt shift_remainder_low
  srw r8, r3, r7
  li r7, 0
  b shift_dividend
shift_remainder_low:
  srw r8, r4, r9
  subfic r7, r9, 0x20
  slw r7, r3, r7
  or r8, r8, r7
  srw r7, r3, r9
shift_dividend:
  cmpwi r0, 0x20
  addic r9, r0, -32
  blt shift_dividend_low
  slw r3, r4, r9
  li r4, 0
  b start_division
shift_dividend_low:
  slw r3, r3, r0
  subfic r9, r0, 0x20
  srw r9, r4, r9
  or r3, r3, r9
  slw r4, r4, r0
start_division:
  li r10, -1
  addic r7, r7, 0
divide_bit:
  adde r4, r4, r4
  adde r3, r3, r3
  adde r8, r8, r8
  adde r7, r7, r7
  subfc r0, r6, r8
  subfe. r9, r5, r7
  blt next_bit
  mr r8, r0
  mr r7, r9
  addic r0, r10, 1
next_bit:
  bdnz divide_bit
  adde r4, r4, r4
  adde r3, r3, r3
  lwz r9, 8(r1)
  lwz r10, 0xc(r1)
  xor. r7, r9, r10
  beq apply_sign_done
  cmpwi r9, 0
  subfic r4, r4, 0
  subfze r3, r3
apply_sign_done:
  b finish
zero_quotient:
  li r4, 0
  li r3, 0
finish:
  addi r1, r1, 0x10
  blr
}

asm void __mod2u(void) {
  nofralloc
  cmpwi r3, 0
  cntlzw r0, r3
  cntlzw r9, r4
  bne count_divisor_bits
  addi r0, r9, 0x20
count_divisor_bits:
  cmpwi r5, 0
  cntlzw r9, r5
  cntlzw r10, r6
  bne compare_sizes
  addi r9, r10, 0x20
compare_sizes:
  cmpw r0, r9
  subfic r10, r0, 0x40
  bgt finish
  addi r9, r9, 1
  subfic r9, r9, 0x40
  add r0, r0, r9
  subf r9, r9, r10
  mtctr r9
  cmpwi r9, 0x20
  addi r7, r9, -32
  blt shift_remainder_low
  srw r8, r3, r7
  li r7, 0
  b shift_dividend
shift_remainder_low:
  srw r8, r4, r9
  subfic r7, r9, 0x20
  slw r7, r3, r7
  or r8, r8, r7
  srw r7, r3, r9
shift_dividend:
  cmpwi r0, 0x20
  addic r9, r0, -32
  blt shift_dividend_low
  slw r3, r4, r9
  li r4, 0
  b start_division
shift_dividend_low:
  slw r3, r3, r0
  subfic r9, r0, 0x20
  srw r9, r4, r9
  or r3, r3, r9
  slw r4, r4, r0
start_division:
  li r10, -1
  addic r7, r7, 0
divide_bit:
  adde r4, r4, r4
  adde r3, r3, r3
  adde r8, r8, r8
  adde r7, r7, r7
  subfc r0, r6, r8
  subfe. r9, r5, r7
  blt next_bit
  mr r8, r0
  mr r7, r9
  addic r0, r10, 1
next_bit:
  bdnz divide_bit
  mr r4, r8
  mr r3, r7
  blr
finish:
  blr
}

asm void __shl2i(void) {
  nofralloc
  subfic r8, r5, 0x20
  addic r9, r5, -32
  slw r3, r3, r5
  srw r10, r4, r8
  or r3, r3, r10
  slw r10, r4, r9
  or r3, r3, r10
  slw r4, r4, r5
  blr
}

asm void __shr2u(void) {
  nofralloc
  subfic r8, r5, 0x20
  addic r9, r5, -32
  srw r4, r4, r5
  slw r10, r3, r8
  or r4, r4, r10
  srw r10, r3, r9
  or r4, r4, r10
  srw r3, r3, r5
  blr
}

asm void __shr2i(void) {
  nofralloc
  subfic r8, r5, 0x20
  addic. r9, r5, -32
  srw r4, r4, r5
  slw r10, r3, r8
  or r4, r4, r10
  sraw r10, r3, r9
  ble shift_high_word
  or r4, r4, r10
shift_high_word:
  sraw r3, r3, r5
  blr
}
/* clang-format on */
