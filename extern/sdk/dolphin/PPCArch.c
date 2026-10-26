/* clang-format off */
__declspec ( weak ) asm void PPCHalt (void) //spins infinitely
{
    nofralloc

    sync

_spin:
    nop
    li r3, 0
    nop
    b _spin

    // NEVER REACHED
}
/* clang-format on */
