stm32ld
=======

stm32 downloader

Compile
gcc -o stm32ld main.c stm32ld.c serial_posix.c

Execute
stm32ld /dev/tty 115200 main.bin

