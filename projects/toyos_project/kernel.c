void kernel_main(void)
{
    volatile unsigned short* video =
        (volatile unsigned short*)0xB8000;

    video[0] = (0x07 << 8) | 'K';

    while (1)
    {
    }
}