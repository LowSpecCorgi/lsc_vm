#include "stdio.h"

/*
This is a VM for the LC-3, a teaching computer architecture

From: https://www.jmeiners.com/lc3-vm/
*/

#include "stdint.h"
#include "stdio.h"
#include <stdint.h>
#include <stdlib.h>

/*
The LC-3 has 65,536 memory locations, which can be stored in a 16 bit unsigned integer
Each memory location stores a 16 bit value

This means the total N bits is (65,536*16)*(1/(8*1024)) KB
*/
#define LSC_MEMORY_MAX (1 << 16)
uint16_t lsc_memory[LSC_MEMORY_MAX];

/*
The LC-3 has 10 total registers. Each of which stores 1 value.

Why use an enum here?
- Enum values cannot be modified. These can act as constants :)

Why use an anonymous enum?
- This will not use memory to store the enum
*/
enum {
    LSC_R_R0 = 0, // General purpose start
    LSC_R_R1,
    LSC_R_R2,
    LSC_R_R3,
    LSC_R_R4,
    LSC_R_R5,
    LSC_R_R6,
    LSC_R_R7, // General purpose end
    LSC_R_PC, // Program counter (next instruction in memory to compute)
    LSC_R_COND, // Information about previous calculation
    LSC_R_COUNT, // N registers
};

typedef uint16_t LSC_REGISTER[LSC_R_COUNT];

LSC_REGISTER lsc_reg;

/*
This is the instruction set.

Each instruction has an opcode which indicates the type of task to perform and the set of parameters to provide inputs.

LC-3 has 16 opcodes. Each opcode is 16 bits long. Left 4 store opcode. Rest store parameters
*/
enum {
    LSC_OP_BR = 0, // Branch
    LSC_OP_ADD, // Add
    LSC_OP_LD, // Load
    LSC_OP_ST, // Store
    LSC_OP_JSR, // Jump register
    LSC_OP_AND, // Bitwise and
    LSC_OP_LDR, // Load register
    LSC_OP_STR, // Store register
    LSC_OP_RTI, // Unused
    LSC_OP_NOT, // Bitwise not
    LSC_OP_LDI, // Load indirect
    LSC_OP_STI, // Store indirect
    LSC_OP_JMP, // Jump
    LSC_OP_RES, // Reserved/unused
    LSC_OP_LEA, // Load effective address
    LSC_OP_TRAP // Execute trap
};

/*
These are the condition flags, stored by R_COND, which provide information about the most recently executed calculation.
This allows for logical condition checking.

Why use enum over #define here?
- Enums are "type-safe" so the compiler validates whether the types are correct
*/
enum {
    LSC_FL_POS = 1 << 0, // Positive sign (P)
    LSC_FL_ZRO = 1 << 1, // Zero, so no sign (Z)
    LSC_FL_NEG = 1 << 2, // Negative sign (N)
};

/*
Let's look at an example LC-3 assembly program.

HELLO WORLD PROGRAM:
.ORIG x3000 ; This is where the program will originate from in memory
loaded
LEA R0, HELLO_STR ; Load the effective address of HELLO_STR into the general purpose register, R0
PUTs ; Output the string pointed to by R0 into the console
HALT ; Halt the program
HELLO_STR .STRINGZ "Hello World!" ; Store this string here in the program
.END ; EOF

This is not directly compatible with the VM as it is in an assembly forl. AN assembler will transform this into the appropriate binary format.

LOOP PROGRAM:
AND R0, R0, 0 ; Clear R0
LOOP ; This is a label
ADD R0, R0, 1 ; Add 1 to R0 and store in R0
ADD R1, R0, -10 ; Subtract 10 from R0 and store in R1
BRn LOOP ; Go back to LOOP if result is negative
BRn 
*/

/*
Two's complement:
- Representation of a negative number
- Think of a car meter
    - Drive for a mile and you could end up with 00001 on the meter. This can be interpreted as +1.
    - Say we could turn this meter back one mile to 99999. This can be intepreted as -1.
- Lets look at an example
    - 0001, this would be represented as +1
    - 1111, This would be represented as -1. The left most bit shows that the value is negative.
        - Think of this like so:
            - MSB represents -8. The rest of the values are positive, adding to it. So -8 + 4 + 2

Sign bit:
- The leftmost bit also called the most significant bit
- When this bit is 1 the number is negative and when 0 the number is positive

Computing the Two's complement:
1. Start with the binary representation of the number. With the leading bit being a sign bit.
2. Invert ALL bits
3. Add 1 to entire number, ignoring overflow

Example:
1. 0111 = +7
2. Flip the bits: 1000 -> Negative number.
3. Add 1 to the flipped number: 1001.
    - Lets check this is correct: -8 + 1 = -7. So this does work :D
*/

uint16_t lsc_sign_extend(uint16_t x, int bit_count) {
    /*
    Lets break this down:
    - Lets assume x = (-7) and the bit_count = 4.
    - bit_count - 1
        - Now we are working with 3 bits.
    - x >> 3
        - The >> operator shifts all bits right. Here that is done 3 times. Let's see that happening:
            - Original number = 0000 0000 0000 1001 (you can see here that -7 has been improperly extended to form 9 rather than -7)
            - Bitshifted number = 0000 0000 0000 0001
    - 0000 0000 0000 0001 & 0000 0000 0000 0001
        - This is the bitwise and operator
            - This compares 0000 0000 0000 0001 with the binary representation of 1 which is 0000 0000 0000 0001
    
    What is the purpose of this?
    - This simply just checks whether the most significant bit is 1. If it is then the number should be negative.
    */
    if ((x >> (bit_count - 1)) & 1) {
        /*
        Lets continue to break this down:
        - 0xFFFF << 4 (bit_count)
            - 0xFFFF is a hexadecimal number
                - Hexadecimal works on powers of 16. 0xF(1)F(16)F(256)F(4096)
                - Multiply by powers of 16:
                    - F is 15
                    - 15*1 + 15*16 + 15*256 + 15*4096 = 65535
                        - Interesting. Where have we seen that number before? That is the maximum number a 16 bit number can store. Would logically be represented like such in binary: 1111 1111 1111 1111
                            - This would represent -1 in binary since we are using the Two's complement
            - The << operator shifts all bits left. This is one 4 times here. Let's see this happening:
                - Original number: 1111 1111 1111 1111
                - Bitshifted number: 1111 1111 1111 0000
                    - We can see the rightmost 4 bits are where our number will be stored
        - x |= 1111 1111 1111 0000
            - |= is the bitwise or operator. 1 will be returned if at least 1 bit is 1. Else 0.
            - x = 0000 0000 0000 1001 
            - 0000 0000 0000 1001 | 1111 1111 1111 0000:
                - 1111 1111 1111 1001
                    - Here we get -7 properly represented in 16 bit :D.
        */
        x |= (0xFFFF << bit_count);
    }

    // If not negative, then the compiler can easily extend the value
    return x;
}

void lsc_update_flags(uint16_t r, LSC_REGISTER *reg) {
    if (reg[r] == 0) {
        *reg[LSC_R_COND] = LSC_FL_ZRO;
    }
    // A 1 in the left most bit indicates negative
    else if (*reg[r] >> 15) {
        *reg[LSC_R_COND] = LSC_FL_NEG;
    }
    else {
        *reg[LSC_R_COND] = LSC_FL_NEG;
    }
}

int main(int argc, char **argv) {
    /*
    Program execution

    1. Load one instruction from address stored at PC
    2. Increment PC
    3. Look at opcode to determine the appropriate instruction to perform
    4. Perform instruction
    5. Repeat
    */

    /*
    Here we handle command line input

    argc: ARGument Count
    argv: ARGument Variables
    */
    if (argc < 2) {
        printf("lsc_vm [image-file1] ...\n");
        exit(2);
    }

    for (int j = 1; j < argc; ++j) {
        // STUB
    }

    // Exactly one condition flag must be set at any time.
    lsc_reg[LSC_R_COND] = LSC_FL_ZRO;

    /*
    Set the PC to the starting position at 0x3000

    An enum is used as a constant here
    */
    enum { PC_START = 0x3000 };
    lsc_reg[LSC_R_PC] = PC_START;

    int running = 1;
    while (running) {
        // Fetch instr / STUB
        uint16_t instr = 0;
        uint16_t op = instr >> 12;

        switch (op) {
            case LSC_OP_ADD: {
                /*
                ADD has two encodings:
                - Register mode: 0001 (15-12) DR (11-9) SR1 (8-6) 0 (5) 00 (4-3 unused) SR2 (2-0)
                - Immediate mode: 0001 (15-12) DR (11-9) SR1 (8-6) 1 (5) imm5 (4-0)
                */

                // Get the destination register stored at (11-9)
                uint16_t dr = (instr >> 9) & 0x7;

                // Get the source register stored at (8-6)
                uint16_t sr = (instr >> 6) & 0x7;

                // Check whether in immediate or register mode
                uint16_t mode = (instr >> 5) & 0x1;

                uint16_t n2;

                if (mode == 1) {
                    // Get the immediate value and extend to 16 bit preserving sign (this value is 5 bits wide inclusive of 0)
                    n2 = lsc_sign_extend(instr & 0x1F, 5);
                    
                } else {
                    // Get the source register 2 value stored at (2-0)
                    uint16_t sr2 = instr & 0x7;
                    n2 = lsc_reg[sr2];
                }

                // Add the immediate to the value in n2 then store in dr
                lsc_reg[dr] = lsc_reg[sr] + n2;

                // Update flags so the next cycle has sign information
                lsc_update_flags(dr, &lsc_reg);
                break;
            }
            case LSC_OP_AND: {
                /*
                AND has two encodings:
                - Register mode: 0101 (15-12), DR (11-9), SR1 (8-6), 0 (5), 00 (4-3 unused), SR2 (2-0)
                - Immediate mode: 0101 (15-12), DR (11-9), SR1 (8-6), 1 (5), imm5 (4-0)

                SR1 and SR2/imm5 are ANDed. Result stored in DR. COND set based on sign.
                */

                // Get the destination register located at (11-9)
                uint16_t dr = (instr >> 9) & 0x7;

                // Get the source register located at (8-6)
                uint16_t sr1 = (instr >> 6) & 0x7;

                // Check whether in register or immediate mode located at (5)
                uint16_t mode = (instr >> 5) & 0x1;

                uint16_t n2;

                if (mode == 1) {
                    // Get imm5 located at (4-0)
                    n2 = instr & 0x1F;


                } else {
                    // Get the value located at SR2 stored at (2-0)
                    uint16_t sr2 = instr & 0x7;
                    n2 = lsc_reg[sr2];
                }

                // Bitwise AND n2 and SR1 and store in dr
                lsc_reg[dr] = lsc_reg[sr1] & n2;

                // Set COND flag
                lsc_update_flags(dr, &lsc_reg);
                break;
            }
            case LSC_OP_NOT: break;
            case LSC_OP_BR: break;
            case LSC_OP_JMP: break;
            case LSC_OP_JSR: break;
            case LSC_OP_LD: break;
            case LSC_OP_LDI: {
                /*
                LoaD Indirect has one encoding:
                - 1010 (15-12), DR (11-9), PCoffset9 (8-0)

                This function loads a value from a location in memory into a register.

                PCoffset9 is an immediate value.

                The adress referred to by PCoffset9 is computed by sign-extending bits (8-0) to 16 bits then adding this to the PC. 
                What is stored at this address is the address of the data to be loaded into DR.

                LDI is particularly useful for loading values that are far away from current PC as there are only 9 bits to store the address.
                */

                // Retrieve PCoffset9 stored at (8-0)
                uint16_t pcoffset9 = lsc_sign_extend(instr & 0x9, 9);

                // Retrieve DR
                uint16_t dr = (instr >> 9) & 0x3;

                // Get PC address
                uint16_t pc_address = lsc_reg[LSC_R_PC] + pcoffset9;

                // Get what is stored at PC and add to DR / STUB

                // Update flags
                lsc_update_flags(dr, &lsc_reg);

                break;
            }
            case LSC_OP_LDR: break;
            case LSC_OP_LEA: break;
            case LSC_OP_ST: break;
            case LSC_OP_STR: break;
            case LSC_OP_TRAP: break;
            case LSC_OP_RES:
            case LSC_OP_RTI:
            default: break;
        }
    }

    return 0;
}