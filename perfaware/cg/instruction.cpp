#include "instruction.h"


// REG | W=0 | W=1
// 000 |  AL |  AX
// 001 |  CL |  CX
// 010 |  DL |  DX
// 011 |  BL |  BX
// 100 |  AH |  SP
// 101 |  CH |  BP
// 110 |  DH |  SI
// 111 |  BH |  DI
register_t get_register(u8 reg, u8 w) {
  static register_t registers[2][8] = {
      {REGISTER_AL, REGISTER_CL, REGISTER_DL, REGISTER_BL, REGISTER_AH, REGISTER_CH, REGISTER_DH, REGISTER_BH},
      {REGISTER_AX, REGISTER_CX, REGISTER_DX, REGISTER_BX, REGISTER_SP, REGISTER_BP, REGISTER_SI, REGISTER_DI},
  };
  register_t result = registers[w][reg];
  return result;
}

string_t get_register_name(u8 reg, u8 w) {
  register_t r = get_register(reg, w);
  return register_names[r];
}

