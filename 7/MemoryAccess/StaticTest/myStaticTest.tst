// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/7/MemoryAccess/StaticTest/StaticTest.tst

// Tests StaticTest.asm on the CPU emulator.

load myStaticTest.asm,
output-file myStaticTest.out,
compare-to myStaticTest.cmp,

set RAM[0] 256,    // initializes the stack pointer

repeat 200 {       // enough cycles to complete the execution
  ticktock;
}

// Outputs the value at the stack's base 
output-list RAM[256]%D1.6.1;
output;
