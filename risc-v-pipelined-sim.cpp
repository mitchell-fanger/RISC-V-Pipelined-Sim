 /*****************************************************************************/
 /*                              VESP COMPUTER-2.0                             *
 /*                            Author: A. Yavuz Oruc                           *
 /*                            Copyright ¬© 2000,2004, 2008.                   *
 /*                      Sabatech Corporation (www.sabatech.com)               *
 /*                                All rights reserved.                        *
 /*Copying and compiling this program for personal use is permitted. However,  *
 /*no part of this program may be reproduced or distributed in any form or     *
 /*by any means, or stored in a database or  retrieval system, without the     *
 /*prior written permission of the Author and AlgoritiX Corporation.           *
 /*Neither Algoritix nor the Author makes any direct or implied claims         *
 /*that this program will work accurately and/or generate correct results,     *
 /*In addition, they provide no warranties direct or implied that using        *
 /*this program on any computer will be safe in so far as  guarding against    *
 /*destroying and/or deleting any information, data, code, program or file     *
 /*that may be stored in the memory of user's computer.                        *
 /*****************************************************************************/


/*      Please communicate all your questions and comments to                 */
/*                    A. Yavuz Oruc at yavuz@sabatech.com                        */

/*         Programs are entered and displayed in hex code. */
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
using namespace std;

void initialize(void);
int readprogram(void);
void displayregisters(void);
void maincycle(int trace);
void fetch(int trace);
void decode(void);
void execute(void);
void memory(void);
void writeback(void);
void update(void);

void printmodules(void);
int hazard(int reg);
int readinstr(int addr);

//AYO: Define the registers of the architecture.
typedef struct {
  unsigned short  PC; //2 bytes/16 bits because 64K memory
  unsigned int    IR; //4 bytes/32 bits
  unsigned long   clock;
  int             REG[32],
                  reset,
                  LUI,
                  AUIPC,
                  JAL,
                  JALR,
                  BEQ, BNE, BLT, BGE, BLTU, BGEU,
                  LB, LH, LW, LBU, LHU,
                  SB, SH, SW,
                  ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
                  ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
                  FENCE,
                  ECALL, EBREAK;
  unsigned short  INSTR_BUFFER[65536]; //1 byte/8 bits per entry, 64K locations
  unsigned short  MEMORY[65536]; //1 byte/8 bits per entry, 64K memory locations
} architecture;  architecture   vesp;

typedef struct {
  int instr;
  int pc_inc;
} IFID_t; IFID_t IFID; IFID_t IFID_temp;

typedef struct {
  int instr;
  int pc_inc;
  int branch;     //indicates if branch instruction
  int rd;
  int imm;
  int rs1;
  int rs2;
} IDEX_t; IDEX_t IDEX; IDEX_t IDEX_temp;

typedef struct {
  int instr;
  int pc_inc;     //stores new pc if branch is taken
  int branch;     //indicates if branch prediction has failed, should reset pipeline at new pc
  int dest;
  int result;
  int memwrite;   //store instruction flag
  int memread;    //load instruction flag
} EXMEM_t; EXMEM_t EXMEM; EXMEM_t EXMEM_temp;

typedef struct {
  int instr;
  int dest;
  int result;
  int memwrite;   //store instruction flag
  int memread;    //load instruction flag
} MEMWB_t; MEMWB_t MEMWB; MEMWB_t MEMWB_temp;

typedef struct {
  int instr;
  int dest;
  int result;
} WBEND_t; WBEND_t WBEND; WBEND_t WBEND_temp;

int j=1;

int main(void) {
  int action,action2,progend;
  initialize();//AYO: Initialize all the registers.
  cout << "\nWelcome to Vesp 2.0\n\n";
  char * dir = getcwd(NULL, 0); // Platform-dependent, see reference link below
  printf("Current dir: %s", dir);

  while(1) {
    vesp.reset = false; //AYO: vesp keeps running unless it is reset.
    //AYO: Input program, diplay registers or memory
    cin.clear();
    cout << "\nType \n 0 to enter a program\n " << "1 to display registers\n 2 to display memory: ";
    cin >> action2;
    cin.clear();
    cout << "\n";
    //AYO: Read the program, display it, and execute it.
    if(action2 == 0) {
      progend = readprogram();
      //AYO: Step through or execute the program.

      while(vesp.reset == 0) {
        cout << "\nEnter 0 if you wish to execute the program, 1 to step it, ";
        cout <<  "2 to exit to the main menu: ";
        cin.clear(); cin >> action2;
        switch(action2) {
          case 0: cout << "Enter 0 for a brief trace 1 for long trace: "; cin.clear(); cin >> action;
            cout << "\n";

            //initialize pipeline stage latched registers 
            IFID.instr = -1;
            IDEX.instr = -1;
            EXMEM.instr = -1;
            MEMWB.instr = -1;
            WBEND.instr = -1;

            EXMEM.dest = -1;
            IDEX.branch = 0;

            while(vesp.reset == 0)
              maincycle(action);
            displayregisters();
            break;
          case 1:
            maincycle(1);
            break;
          case 2:
            vesp.reset = true;
            break;
        }
      }

      //AYO: Display the number of instructions and clock cycles executed.
      if(action2 != 2) {
        cout << "The number of instructions executed  = " << dec << j-1  << "\n";
        cout << "The number of clock cycles used = " << dec << vesp.clock << "\n";
        j = 1;
        vesp.clock = 0;
      }
    }

    if (action2 == 1)
      displayregisters();
    //if (action2 == 2)
      //displaymemory();
    //if (action == 3)
        //readprogram();
  }
}

void initialize(void) {
  for(int i = 0; i < 32; i++)
    vesp.REG[i] = 0;
  vesp.PC = vesp.IR = 0;
  vesp.reset = 0;
  vesp.clock = 0;
}

int readprogram(void) {
  int address,instruction,progend,action; //,i;
  //char longfilename[100] ="/UMD-Web-Dreamweaver/teaching/courses/enee350/vesp-source-code/vesp2.0X/",
  char filename[25];
  FILE *file;
  do {
    cout << "Enter your program's starting " << "address ( >= 0) as a 4-digit hex number: ";
    cin >> hex >> vesp.PC;
  } while (vesp.PC < 0);

  address =  vesp.PC; action = -1;
  cout << "\n";

  do {
    cout << "Enter 0 to type in your program or 1 to read it from a file: ";
    cin >> action;
  } while (action != 0 && action != 1);

  if(action == 1) {
    cout << "\nEnter the file name: "; cin >> filename;
    file = fopen(filename,"r");
    if( file != NULL) {
      while (fscanf(file,"%x",&instruction) != EOF  &&  address < 16384) {
        vesp.INSTR_BUFFER[address] = (0x000000FF & instruction);
        vesp.INSTR_BUFFER[address+1] = (0x0000FF00 & instruction) >> 8;
        vesp.INSTR_BUFFER[address+2] = (0x00FF0000 & instruction) >> 16;
        vesp.INSTR_BUFFER[address+3] = (0xFF000000 & instruction) >> 24;
        address = address + 4;
      }
      fclose(file);
    }
    else {
      cout << "The file is not found. Check if file to be opened is in the program directory... \n"; exit(1);
    }
  }
  else {
    bool done = false;
    do {
      cin.clear();
      cout << "Enter instruction  "
           << (address/4 -vesp.PC/4)
           << " using a 8-digit hex number" << "\n";
      cout << "Or type -1 to end your program: ";

      cin >> hex >> instruction;   //AYO: read it in hex.
      if (instruction != -1) {
        vesp.INSTR_BUFFER[address] = (0x000000FF & instruction);
        vesp.INSTR_BUFFER[address+1] = (0x0000FF00 & instruction) >> 8;
        vesp.INSTR_BUFFER[address+2] = (0x00FF0000 & instruction) >> 16;
        vesp.INSTR_BUFFER[address+3] = (0xFF000000 & instruction) >> 24;
        address = address + 4;
      }
      else
        done = true;
    } while ( !done && (address < 16384)); //AYO: -1 marks the end.

    if (address >= 65536) {
      cout << "Memory overflow,"
           << "Please quit from the file menu and restart me!";
      return address;
    }
    progend = address;
  }

    //this doesnt work
  //save the program into a file
    cout << "Enter 0 to continue, 1 to save your program into a file: ";
    cin >> action;

    if(action == 1) {
      cout << "\nEnter the file name: ";
      cin >> filename;

      cout << hex;
      file = fopen(filename,"w");
      if( file != NULL) {
        address = vesp.PC;
        while (address < progend ) {
          fprintf(file,"%04X\n",vesp.MEMORY[address]);
          address = address + 1;
        }
        fclose(file);
      }
      else {
        cout << "The file is not found. Check if file to be opened is in the program directory... \n"; exit(1);
      }

   }

  return progend;

}

void displayregisters(void) {

  for(int i = 0; i < 32; i++) {
    cout << dec << "x" << (i) << " = ";  cout.fill('0');
    cout.width(8);
    cout.setf(ios::uppercase);

    cout << hex << (vesp.REG[i]) << ", ";
  }

  cout << "PC = ";
  cout.fill('0');
  cout.width(4);
  cout.setf(ios::uppercase);

  cout << hex << vesp.PC << ", ";

  cout << "IR = ";
  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);

  cout << hex << vesp.IR << ", ";

  cout << "reset = " << vesp.reset << "\n";
}

void printmodules(void) {

  cout << "\nIFID: " << "\n\tInstruction: ";

  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);


  cout << hex << IFID.instr
       << "\n\tPC " << IFID.pc_inc;


  cout << "\nIDEX: " << "\n\tInstruction: ";

  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);

  cout << hex << IDEX.instr
       << "\n\tPC: " << IDEX.pc_inc
       << "\n\tBranch: " << IDEX.branch
       << "\n\tRD: " << IDEX.rd
       << "\n\tIMM: " << IDEX.imm
       << "\n\tRS1: " << IDEX.rs1
       << "\n\tRS2: " << IDEX.rs2;

  cout << "\nEXMEM: " << "\n\tInstruction: ";

  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);

  cout << hex << EXMEM.instr
       << "\n\tBranch: " << EXMEM.branch
       << "\n\tPC: " << EXMEM.pc_inc
       << "\n\tDest: " << EXMEM.dest
       << "\n\tResult: " << EXMEM.result
       << "\n\tMemread: " << EXMEM.memread
       << "\n\tMemwrite: " << EXMEM.memwrite;


  cout << "\nMEMWB: " << "\n\tInstruction: ";

  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);

  cout << hex << MEMWB.instr
       << "\n\tDest: " << MEMWB.dest
       << "\n\tResult: " << MEMWB.result
       << "\n\tMemread: " << MEMWB.memread
       << "\n\tMemwrite: " << MEMWB.memwrite;

  cout << "\nWBEND: " << "\n\tInstruction: ";
  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);

  cout << hex << WBEND.instr << "\n";
}

void maincycle(int trace) {
  cout << "Machine Cycle ";
  cout.fill('0');
  cout.width(8);
  cout.setf(ios::uppercase);

  cout << hex << j << ":  ";
  j = j+1;

  //AYO: Fetch Step
  cout << "PC = ";
  cout.fill('0');
  cout.width(4);
  cout.setf(ios::uppercase);

  cout << hex << (vesp.PC & 0xFFFF) << ", ";

  if(trace == 1)
    cout << "\nFE\n";

  fetch(1);

  //AYO: Decode Step
  if(trace == 1)
    cout << "\nDE\n";

  decode();

  //AYO: Execute Step
  if(trace == 1)
    cout << "\nEX\n";

  execute();

  if(trace == 1)
    cout << "\nMEM\n";

  memory();

  if(trace == 1)
    cout << "\nWB\n";

  writeback();

  //update structs and increment clock
  update();

  if(trace == 1)
    printmodules();

  //AYO: Display the registers
  if(trace == 1) 
    displayregisters();
  cout << "\n"; 

}
//updates structs and increments clock
void update(void) {

  //update IFID latched registers
  IFID.instr = IFID_temp.instr;
  IFID.pc_inc = IFID_temp.pc_inc;
 
  //update IDEX latched registers 
  IDEX.instr = IDEX_temp.instr;
  IDEX.pc_inc = IDEX_temp.pc_inc;
  IDEX.branch = IDEX_temp.branch;
  IDEX.rd = IDEX_temp.rd;
  IDEX.imm = IDEX_temp.imm;
  IDEX.rs1 = IDEX_temp.rs1;
  IDEX.rs2 = IDEX_temp.rs2;

  //update EXMEM latched registers 
  EXMEM.instr = EXMEM_temp.instr;
  EXMEM.branch = EXMEM_temp.branch;
  EXMEM.pc_inc = EXMEM_temp.pc_inc;
  EXMEM.dest = EXMEM_temp.dest;
  EXMEM.result = EXMEM_temp.result;
  EXMEM.memread = EXMEM_temp.memread;
  EXMEM.memwrite = EXMEM_temp.memwrite;

  //update MEMWB latched registers 
  MEMWB.instr = MEMWB_temp.instr;
  MEMWB.dest = MEMWB_temp.dest;
  MEMWB.result = MEMWB_temp.result;
  MEMWB.memread = MEMWB_temp.memread;
  MEMWB.memwrite = MEMWB_temp.memwrite;

  //update WBEND latched registers 
  WBEND.instr = WBEND_temp.instr;
  WBEND.dest = WBEND_temp.dest;
  WBEND.result = WBEND_temp.result;

  vesp.clock += 1;

  //if branch prediction fails
  if (EXMEM.branch == 1) {
    IFID.instr = -1; //flush fetch
    IDEX.instr = -1; //flush decode 
    vesp.PC = EXMEM.pc_inc; 
  }
  else
    vesp.PC = IFID.pc_inc;

  if (IFID.instr == 0 && IDEX.instr == 0 && EXMEM.instr == 0 && MEMWB.instr == 0 && WBEND.instr == 0) {
    cout << "\nAll modules empty, ending process\n";
    vesp.reset = 1;
  } 

}

//read 1 byte from memory
int readinstr(int addr) {
  return (0x00FF & vesp.INSTR_BUFFER[addr]);
}

void fetch(int trace) {

  IFID_temp.instr = 0;
  IFID_temp.instr += readinstr(vesp.PC);
  IFID_temp.instr += readinstr(vesp.PC+1) << 8;
  IFID_temp.instr += readinstr(vesp.PC+2) << 16;
  IFID_temp.instr += readinstr(vesp.PC+3) << 24;

  IFID_temp.pc_inc = vesp.PC + 4; //Increment PC by 4

  if(trace == 1) {
    cout << "IR = "; cout.fill('0'); cout.width(8); cout.setf(ios::uppercase);
    cout << hex << IFID_temp.instr << ", ";
  }
}

void decode(void) {

  int imm = 0;
  int rd = 0;
  int rs1 = 0;
  int rs2 = 0;

  //if no instruction to be decoded

  if (IFID.instr == -1) {
    IDEX_temp.instr = -1;
    IDEX_temp.pc_inc = -1; 
    IDEX_temp.branch = 0;
    IDEX_temp.rd = -1;
    IDEX_temp.imm = -1;
    IDEX_temp.rs1 = -1;
    IDEX_temp.rs2 = -1;
    return;
  }

  IDEX_temp.instr = IFID.instr;

  cout << hex << "instruction: " << IFID.instr << " decodes to: " << (IFID.instr & 0x7F);

  switch(IFID.instr & 0x0000007F) {
    case  0x63: //B-Type, has imm, rs1, and rs2
      cout << "B-Type";
      rs1 = (IFID.instr >> 15) & 0x1F;
      rs2 = (IFID.instr >> 20) & 0x1F;
      imm = 0;
      imm += ((IFID.instr >> 7) & 0x1) << 11;  //imm[11]
      imm += ((IFID.instr >> 8) & 0xF) << 1;   //imm[4:1]
      imm += ((IFID.instr >> 25) & 0x3F) << 5; //imm[10:5]

      //perform sign extension for imm[31:12]
      for(int i = 12; i < 32; i++)
        imm += ((IFID.instr >> 31) & 1) << i;

      //update temp struct 
      IDEX_temp.instr = IFID.instr;
      IDEX_temp.pc_inc = IFID.pc_inc; //NEVER TAKE BRANCH
      IDEX_temp.branch = 1;
      IDEX_temp.rd = -1;
      IDEX_temp.imm = imm;
      IDEX_temp.rs1 = rs1;
      IDEX_temp.rs2 = rs2;

      cout << "Immediate: " << imm << " IDEX_temp.imm: " << IDEX_temp.imm 
           << " IFID.instr[31] = " << ((IFID.instr >>31) & 1)
           << "\n";

      switch((IFID.instr >> 12) & 0x00007) {
        case  0x0:
          cout << "BEQ\n";
          break;
        case  0x1:
          cout << "BNE\n";
          break;
        case  0x4:
          cout << "BLT\n";
          break;
        case  0x5:
          cout << "BGE\n";
          break;
      }
      break;
    case  0x03: //I-Type, has imm, rs1, rd
      cout << "I-Type";
      imm = 0;
      imm += (IFID.instr >> 20) & 0xFFF; 
      
      //perform sign extension for imm[31:12]
      for(int i = 12; i < 31; i++)
        imm += ((IFID.instr >> 31) & 1) << i;

      rs1 = (IFID.instr >> 15) & 0x1F;
      rd = (IFID.instr >> 7) & 0x1F;

      //update temp struct
      IDEX_temp.instr = IFID.instr;
      IDEX_temp.pc_inc = IFID.pc_inc;
      IDEX_temp.branch = 0;
      IDEX_temp.rd = rd;
      IDEX_temp.imm = imm;
      IDEX_temp.rs1 = rs1;
      IDEX_temp.rs2 = -1;

      switch((IFID.instr >> 12) & 0x00007) {
        case  0x2:
          cout << "LW\n";
          break;
        }
      break;
    case  0x23: //S-Type, has imm, rs1, rs2

      cout << "S-Type";
      imm = 0;
      imm += (IFID.instr >> 7) & 0x1F;
      imm += (IFID.instr >> 25) << 5;
      
      //perform sign extension for imm[31:12]
      for(int i = 12; i < 32; i++)
        imm += ((IFID.instr >> 31) & 1) << i;

      rs1 = (IFID.instr >> 15) & 0x1F;
      rs2 = (IFID.instr >> 20) & 0x1F;

      //update temp struct
      IDEX_temp.instr = IFID.instr;
      IDEX_temp.pc_inc = IFID.pc_inc;
      IDEX_temp.branch = 0;
      IDEX_temp.rd = -1;
      IDEX_temp.imm = imm;
      IDEX_temp.rs1 = rs1;
      IDEX_temp.rs2 = rs2;

      switch((IFID.instr >> 12) & 0x00007) {
        case  0x2:
          cout << "SW\n";
          break;
      }
      break;
    case  0x13: //I-Type, has imm, rs1, rd
      cout << "I-Type";
      imm = 0;
      imm += (IFID.instr >> 20) & 0xFFF; 
      
      //perform sign extension for imm[31:12]
      for(int i = 12; i < 32; i++)
        imm += ((IFID.instr >> 31) & 1) << i;

      rs1 = (IFID.instr >> 15) & 0x1F;
      rd = (IFID.instr >> 7) & 0x1F;

      //update temp struct
      IDEX_temp.instr = IFID.instr;
      IDEX_temp.pc_inc = -1;
      IDEX_temp.branch = 0;
      IDEX_temp.rd = rd;
      IDEX_temp.imm = imm;
      IDEX_temp.rs1 = rs1;
      IDEX_temp.rs2 = -1;

      switch((IFID.instr >> 12) & 0x00007) {
        case  0x0:
          cout << "ADDI\n";
          break;
        case  0x6:
          cout << "ORI\n";
          break;
        case  0x7:
          cout << "ANDI\n";
          break;
      }
      break;
    case  0x33: //R-Type, has rd, rs1, rs2
      cout << "R-Type";
      rs1 = (IFID.instr >> 15) & 0x1F;
      rs2 = (IFID.instr >> 20) & 0x1F;
      rd = (IFID.instr >> 7) & 0x1F;
 
      //update temp struct
      IDEX_temp.instr = IFID.instr;
      IDEX_temp.pc_inc = -1;
      IDEX_temp.branch = 0;
      IDEX_temp.rd = rd;
      IDEX_temp.imm = -1;
      IDEX_temp.rs1 = rs1;
      IDEX_temp.rs2 = rs2;

      switch((IFID.instr >> 12) & 0x00007) {
        case  0x0:
          switch(IFID.instr >> 25) {
            case 0x0:
              cout << "ADD\n";
              break;
            case 0x20:
              cout << "SUB\n";
              break;
          }
          break;
        case  0x4:
          cout << "XOR\n";
          break;
        case  0x5:
          switch(IFID.instr >> 25) {
            case 0x0:
              cout << "SRL\n";
              break;
            case 0x20:
              cout << "SRA\n";
              break;
          }
          break;
        case  0x6:
          cout << "OR\n";
          break;
        case  0x7:
          cout << "AND\n";
          break;
      }
      break;
  }
}


//check for data hazard and perform data forwarding
int hazard(int reg) {
  if (reg == EXMEM.dest) {         
    if (EXMEM.memread == 1) {  //check if load instruction
      cout << "DATA HAZARD: in previous instruction, load error, reading from x" << reg << "\n"
           << "New data value is " << vesp.MEMORY[EXMEM.result] << "\n";
      return vesp.MEMORY[EXMEM.result];
    }
    else if (EXMEM.memwrite == 0) {//make sure not store instruction
      cout << "DATA HAZARD: in previous instruction, alu error, reading from x" << reg << "\n"
           << "New data value is " << EXMEM.result << "\n";
      return EXMEM.result;
    }
    cout << "No data hazard, returning x" << reg << " as data value " << vesp.REG[reg] << "\n";
    return vesp.REG[reg];
  } 
  else if(reg == MEMWB.dest && MEMWB.memwrite == 0) {
      cout << "DATA HAZARD: in second most previous instruction, alu error, reading from x" << reg << "\n"
           << "New data value is " << MEMWB.result << "\n";
      return MEMWB.result;
  }
  else {
    cout << "No data hazard, returning x" << reg << " as data value " << vesp.REG[reg] << "\n";
    return vesp.REG[reg];         //no data hazards, read from register
  }  
}

void execute(void) {

  //if no instruction
  if (IDEX.instr == -1) {
    EXMEM_temp.instr = -1;
    EXMEM_temp.pc_inc = -1;
    EXMEM_temp.branch = 0;
    EXMEM_temp.dest = -1;
    EXMEM_temp.result = -1;
    EXMEM_temp.memread = 0;
    EXMEM_temp.memwrite = 0;
    return;
  }

  EXMEM_temp.instr = IDEX.instr;

  switch(IDEX.instr & 0x0000007F) {
    case  0x63: //B-Type, has imm, rs1, and rs2
      EXMEM_temp.instr = IDEX.instr;
      EXMEM_temp.pc_inc = IDEX.pc_inc + IDEX.imm;
      EXMEM_temp.dest = -1;
      EXMEM_temp.result = -1;
      EXMEM_temp.memread = 0;
      EXMEM_temp.memwrite = 0;
 
      switch((IDEX.instr >> 12) & 0x00007) {
        case  0x0: //BEQ
          if (hazard(IDEX.rs1) == hazard(IDEX.rs2)) 
            EXMEM_temp.branch = 1;
          else
            EXMEM_temp.branch = 0;
          break;
        case  0x1: //BNE
          if (hazard(IDEX.rs1) != hazard(IDEX.rs2)) 
            EXMEM_temp.branch = 1;
          else
            EXMEM_temp.branch = 0;
          break;
        case  0x4: //BLT
          if (hazard(IDEX.rs1) < hazard(IDEX.rs2)) 
            EXMEM_temp.branch = 1;
          else
            EXMEM_temp.branch = 0;
          break;
        case  0x5: //BGE
          if (hazard(IDEX.rs1) >= hazard(IDEX.rs2)) 
            EXMEM_temp.branch = 1;
          else
            EXMEM_temp.branch = 0;
         break;
      }
      break;
    case  0x03: //I-Type, has imm, rs1, rd
      EXMEM_temp.instr = IDEX.instr;
      EXMEM_temp.pc_inc = -1;
      EXMEM_temp.branch = 0;
      EXMEM_temp.memwrite = 0;
 
      switch((IDEX.instr >> 12) & 0x00007) {
        case  0x2: //LW
          EXMEM_temp.dest = IDEX.rd;
          EXMEM_temp.memread = 1;
          EXMEM_temp.result = hazard(IDEX.rs1) + IDEX.imm;
          cout << "HELLO WORLD " << IDEX.rs1 << " " << IDEX.imm << "\n";
          break;
        }
      break;
    case  0x23: //S-Type, has imm, rs1, rs2
      EXMEM_temp.instr = IDEX.instr;
      EXMEM_temp.pc_inc = -1;
      EXMEM_temp.branch = 0;
      EXMEM_temp.memread = 0;

      switch((IDEX.instr >> 12) & 0x00007) {
        case  0x2: //SW
          EXMEM_temp.memwrite = 1;
          EXMEM_temp.dest = hazard(IDEX.rs1) + IDEX.imm;
          EXMEM_temp.result = hazard(IDEX.rs2);
          break;
      }
      break;
    case  0x13: //I-Type, has imm, rs1, rd
      EXMEM_temp.instr = IDEX.instr;
      EXMEM_temp.pc_inc = -1;
      EXMEM_temp.branch = 0;
      EXMEM_temp.memwrite = 0;
      EXMEM_temp.memread = 0;
      EXMEM_temp.dest = IDEX.rd;

      switch((IDEX.instr >> 12) & 0x00007) {
        case  0x0: //ADDI
          EXMEM_temp.result = hazard(IDEX.rs1) + IDEX.imm; 
          break;
        case  0x6: //ORI
          EXMEM_temp.result = hazard(IDEX.rs1) | IDEX.imm; 
          break;
        case  0x7: //ANDI
          EXMEM_temp.result = hazard(IDEX.rs1) & IDEX.imm; 
          break;
      }
      break;
    case  0x33: //R-Type, has rd, rs1, rs2
      EXMEM_temp.instr = IDEX.instr;
      EXMEM_temp.pc_inc = -1;
      EXMEM_temp.branch = 0;
      EXMEM_temp.memwrite = 0;
      EXMEM_temp.memread = 0;
      EXMEM_temp.dest = IDEX.rd;

      switch((IDEX.instr >> 12) & 0x00007) {
        case  0x0:
          switch(IDEX.instr >> 25) {
            case 0x0: //ADD
              EXMEM_temp.result = hazard(IDEX.rs1) + hazard(IDEX.rs2); 
              break;
            case 0x20: //SUB
              EXMEM_temp.result = hazard(IDEX.rs1) - hazard(IDEX.rs2); 
              break;
          }
          break;
        case  0x4: //XOR
          EXMEM_temp.result = hazard(IDEX.rs1) ^ hazard(IDEX.rs2); 
          break;
        case  0x5:
          switch(IDEX.instr >> 25) {
            case 0x0: //SRL
              EXMEM_temp.result = hazard(IDEX.rs1) << hazard(IDEX.rs2); 
              break;
            case 0x20: //SRA
              EXMEM_temp.result = hazard(IDEX.rs1) << hazard(IDEX.rs2); 
              break;
          }
          break;
        case  0x6: //OR
          EXMEM_temp.result = hazard(IDEX.rs1) | hazard(IDEX.rs2); 
          break;
        case  0x7: //AND
          EXMEM_temp.result = hazard(IDEX.rs1) & hazard(IDEX.rs2); 
          break;
      }
      break;
  }
}
void memory(void) {

  //if no instruction
  if (EXMEM.instr == -1) {
    MEMWB_temp.instr = -1;
    MEMWB_temp.dest = -1;
    MEMWB_temp.result = -1;
    MEMWB_temp.memread = 0;
    MEMWB_temp.memwrite = 0;
    return;
  }

  MEMWB_temp.instr = EXMEM.instr; 

  switch(EXMEM.instr & 0x0000007F) {
    case  0x03: //I-Type, has imm, rs1, rd
      switch((EXMEM.instr >> 12) & 0x00007) {
        case  0x2: //LW
          MEMWB_temp.memread = 1;
          MEMWB_temp.dest = EXMEM.dest;
          MEMWB_temp.result = vesp.MEMORY[EXMEM.result];
          break;
        }
      break;
    case  0x23: //S-Type, has imm, rs1, rs2
      switch((EXMEM.instr >> 12) & 0x00007) {
        case  0x2: //SW
          MEMWB_temp.memwrite = 1;
          vesp.MEMORY[EXMEM.dest] = EXMEM.result;
          MEMWB_temp.dest = EXMEM.dest;
          break;
      }
      break;
    default: //non memory instructions
      MEMWB_temp.memread = 0;
      MEMWB_temp.memwrite = 0;
      MEMWB_temp.dest = EXMEM.dest;
      MEMWB_temp.result = EXMEM.result;
      
  }
}

void writeback(void) {

  //if no instruction
  if (MEMWB.instr == -1) {
    WBEND_temp.instr = -1;
    return;
  }
 
  WBEND_temp.instr = MEMWB.instr; 

  switch(MEMWB.instr & 0x0000007F) {
    case  0x03: //I-Type, has imm, rs1, rd
     switch((MEMWB.instr >> 12) & 0x00007) {
        case  0x2: //LW
          vesp.REG[MEMWB.dest] = MEMWB.result; 
          break;
        }
      break;
    case  0x13: //I-Type, has imm, rs1, rd
      vesp.REG[MEMWB.dest] = MEMWB.result;
      break;
    case  0x33: //R-Type, has rd, rs1, rs2
      vesp.REG[MEMWB.dest] = MEMWB.result;
      break;
  }
}

