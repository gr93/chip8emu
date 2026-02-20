A basic CHIP8 emulator for learning purposes.

Deps:

- MSVC Compiler w/ cl.exe in PATH

Built on x86-64 Windows 10 (yes, I'm still on 10, don't ask why):

```cl main.c /I include /link SDL2.lib && main```

Completed:
- Whatever instructions are needed to display logo.ch8 (Set/add VX registers, set I register, jump PC, clear screen, draw pixels from sprites in memory pointed to by the I register)

To go:
- Everything else
