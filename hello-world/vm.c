/*
 * vm.c from https://www.opensourceforu.com/2011/06/virtual-machines-for-abstraction-dalvik-vm/
 */
#include "stdio.h"
 
#define NUM_REGS 4
#define TRUE    1
#define FALSE   0
#define INVALID -1
  
enum opCodes {
    HALT  = 0x0,
    LOAD  = 0x1,
    ADD   = 0x2,
};
 
/*
 * Register set of the VM
 */
int regs[NUM_REGS];
 
/*
 * VM specific data for an instruction
 */
struct VMData_ {
  int reg0;
  int reg1;
  int reg2;
  int reg3;
  int op;
  int scal;
};
typedef struct VMData_ VMData;
 
int  fetch();
void decode(int instruction, VMData *data);
void execute(VMData *data);
void run();
 
/*
 * Addressing Modes:
 * - Registers used as r0, r1,..rn.
 * - Scalar/ Constant (immediate) values represented as #123
 * - Memory addresses begin with @4556
 */
 
/*
 * Instruction set:
 * - Load an immediate number (a constant) into a register
 * - Perform an arithmetic sum of two registers (in effect,
 *   adding two numbers)
 * - Halt the machine
 *
 * LOAD reg0 #100
 * LOAD reg1 #200
 * ADD reg2 reg1 reg0  // 'reg2' is destination register
 * HALT
 */
 
/*
 * Instruction codes:
 * Since we have very small number of instructions, we can have
 * instructions that have following structure:
 * - 16-bit instructions
 *
 * Operands get 8-bits, so range of number supported by our VM
 * will be 0-255.
 * The operands gets place from LSB bit position
 * |7|6|5|4|3|2|1|0|
 *
 * Register number can we encoded in 4-bits 
 * |11|10|9|8|
 *
 * Rest 4-bits will be used by opcode encoding.
 * |15|14|13|12|
 *
 * So an "LOAD reg0 #20" instruction would assume following encoding:
 * <0001> <0000> <00010100>
 * or 0x1014 is the hex representation of given instruction.
 */
 
 
 /*
  * Sample program with an instruction set
  */
 
unsigned int code[] = {0x1014,
                       0x110A,
                       0x2201,
                       0x0000};
 
/*
 * Instruction cycle: Fetch, Decode, Execute
 */
 
/*
 * Current state of machine: It's a binary true/false
 */
 
int running = TRUE;
 
/*
 * Fetch
 */
int fetch()
{
  /*
   * Program Counter
   */
  static int pc = 0;
 
  if (pc == NUM_REGS)
    return INVALID;
 
  return code[pc++];
}
 
void decode(int instr, VMData *t)
{
  t->op   = (instr & 0xF000) >> 12;
  t->reg1 = (instr & 0x0F00) >> 8;
  t->reg2 = (instr & 0x00F0) >> 4;
  t->reg3 = (instr & 0x000F);
  t->scal = (instr & 0x00FF);
}
 
void execute(VMData *t)
{
  switch(t->op) {
    case 1:
      /* LOAD */
      printf("\nLOAD REG%d %d\n", t->reg1, t->scal);
      regs[t->reg1] = t->scal;
      break;
 
    case 2:
      /* ADD */
      printf("\nADD %d %d\n", regs[t->reg2], regs[t->reg3]);
      regs[t->reg1] = regs[t->reg2] + regs[t->reg3];
      printf("\nResult: %d\n", regs[t->reg1]);
      break;
 
    case 0:
    default:
      /* Halt the machine */
      printf("\nHalt!\n");
      running = FALSE;
      break;
    }
}
 
void run()
{
  int instr;
  VMData t;
 
  while(running)
  {
    instr = fetch();
 
    if (INVALID == instr)
      break;
 
    decode(instr, &t);
    execute(&t);
  }
}
 
int main()
{
  run();
  return 0;
}
