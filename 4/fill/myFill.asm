// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/4/Fill.asm

// Runs an infinite loop that listens to the keyboard input. 
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel. When no key is pressed, 
// the screen should be cleared.

// Fill.asm
// Continuously:
//   - If any key is pressed: paint screen black
//   - If no key is pressed: clear screen (white)
//
// Memory map (Hack):
//   SCREEN = 16384 .. 24575 (8,192 words => 256 rows * 32 words per row)
//   KBD    = 24576 (nonzero when any key is pressed)
//
// Approach:
//   1) Poll KBD. If M[KBD] == 0 → color = 0 (white); else color = -1 (black).
//   2) Sweep from SCREEN up to (but not including) KBD, writing 'color'.
//   3) Repeat forever.

            // --- symbols: SCREEN and KBD are predefined in the assembler ---

// Fill.asm
// Project 4: Fills screen black if ANY key pressed, else clears it.

(LOOP)
@KBD
D=M
@WHITE
D;JEQ
@BLACK
0;JMP
(BLACK)
@i
@16384
D=A
@i
M=D
(LOOP1)
@i
D=M
@24576
D=D-A
@ENDBLACK
D;JEQ
@i
D=M
A=D
M=-1
@i
M=M+1
@LOOP1
0;JMP
(ENDBLACK)
@LOOP
0;JMP
(WHITE)
@J
@16384
D=A
@J
M=D
(LOOP2)
@J
D=M
@24576
D=D-A
@ENDWHITE
D;JEQ
@J
D=M
A=D
M=0
@J
M=M+1
@LOOP2
0;JMP
(ENDWHITE)
@LOOP
0;JMP
