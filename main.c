#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned __int8 byte;
typedef uint16_t word;            // Common data size of system

#define IST_SIZE sizeof(uint16_t) // Instruction size
#define MEM_SIZE UINT16_MAX       // Memory size
#define ROM_SIZE 128              // Program memory size

enum REG { // Register indexes
	R0 = 0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	RPC,   // Program counter
	RCOND, // Condtional flags register
	RCOUNT
};

enum OPCODE {// Instruction Format                   Opcode function
	ADD = 1, // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 + R3 or R1 <- R2 + I
	SUB,     // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 - R3 or R1 <- R2 - I
	MUL,     // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 * R3 or R1 <- R2 * I
	DIV,     // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 / R3 or R1 <- R2 / I
	INC,     // oooooddd00000000 || oooooddd0001iiii R1 <- R1 + 1 or R1 <- R1 + I
	DEC,     // oooooddd00000000 || oooooddd0001iiii R1 <- R2 - 1 or R1 <- R2 - I
	SHR,     // ooooodddsss00000                     R1 <- R2 >> 1
	SHL,     // ooooodddsss00000                     R1 <- R2 << 1
	NOT,     // ooooodddsss00000                     R1 <- ~R2
	OR,      // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 | R3 or R1 <- R2 | I
	AND,     // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 & R3 or R1 <- R2 & I
	XOR,     // ooooodddsss00SSS || ooooodddsss1iiii R1 <- R2 ^ R3 or R1 <- R2 ^ I
	LDR,     // ooooodddbbbfffff                     R1 <- MEM(R2 + F)
	LD,      // ooooodddaaaaaaaa                     R1 <- MEM(A)
	LDI,     // ooooodddaaaaaaaa                     R1 <- MEM(MEM(A))
	ST,      // ooooosssaaaaaaaa                     MEM(A) <- R1
	STI,     // ooooosssaaaaaaaa                     MEM(MEM(A)) <- R1
	STR,     // ooooosssbbbfffff                     MEM(R2 + F) <- R1
	BR,      // ooooonzpaaaaaaaa                     Branch if flag set: PC <- PC + A
	JMP,     // ooooo000sss00000 || ooooo0000001iiii PC <- R1 or PC <- I
	JSR,     // ooooo1aaaaaaaaaa                     R[7] <- PC, PC <- A
	JSRR,    // ooooo000sss00000                     R[7] <- PC, PC <- R1
	RET,     // ooooo00011100000                     PC <- R[7]
	CLR,     // oooooddd00000000                     R1 <- 0
	IN,      // oooooddd00000000                     R1 <- IN
	OUT,     // ooooonffsss00000                     OUT <- R1
	SET,     // ooooodddiiiiiiii                     R1 <- I
	GETS,    // ooooo000sss00000                     R1 <- (char[])IN
};

long CYCLE = 0;     // Time in milliseconds between instruction execution or each clock cycle
byte LOG_LEVEL = 0;   // Level of notice for system activity; 0 = all, 1 = warnings only, 2 = errors only, 3 = output only, 4 = input only

word RAM[MEM_SIZE] = { 0 }; // Memory
word REG[RCOUNT];           // Registers

word inst;                  // Current instruction

clock_t clock_time = 0;     // Clock cycle accumulator

int state = 0;              // System state

void system_log(const int level, const char* locale, const char* message, const int num,...) { // System logging
	if (level >= LOG_LEVEL) {
		va_list valist;
		char* type;
		if (level == 0) {
			type = "INFO";
		}
		else if (level == 1) {
			type = "WARN";
		}
		else if (level == 2) {
			type = "ERROR";
		} else if (level == 3) {
			type = "OUT";
		}
		else if (level == 4) {
			type = "IN";
		}
		else {
			printf("[N/A]");
			return;
		}
		printf("[%s]|%s| %s ", type, locale, message);
		va_start(valist, num);
		for (int i = 0; i < num; i++) {
			printf("%d ", va_arg(valist, __int16));
		}
		va_end(valist);
		printf("\n");
	}
}

word sign_extend(word x, word bit_count) // Extension of 8 bit values to 16 bit for 2's complement
{
	if ((x >> (bit_count - 1)) & 1) {
		x |= (0xFFFF << bit_count);
	}
	return x;
}

int ist_fetch(const word address) { // Fetches the next instruction from memory
	if (address + 1 >= MEM_SIZE) {
		system_log(1, "RAM", "Address out of reach", 1, address);
		return 1;
	}
	inst = RAM[address];
	system_log(0, "RAM", "IST fetch", 2, address, inst);
	REG[RPC] = address;
	return 0;
}

void set_flags(__int16 value) { // Updates the conditional flag register
	REG[RCOND] = ((value < 0) << 2) | ((value == 0) << 1) | (value > 0);
	system_log(0, "REG", "Set Flags", 3, (REG[RCOND] & 0b100) >> 2, (REG[RCOND] & 0b10) >> 1, REG[RCOND] & 0b1);
}

int ist_execute() { // Executes the current instruction
	REG[RPC]++;		// Increment program counter automatically
	const byte opcode = inst >> 11;
	switch (opcode) { // Test cases for each legal opcode - illegal opcodes cause a system error
		case 0:		  // Following are brief descriptions of the opcodes; Exact functions listed at top of file	
			break;
		case ADD: {   // Add two values and save them to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			REG[dest] = val1 + val2;
			system_log(0, "ALU", "ADD", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case SUB: {	  // Subtract two values and save them to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			REG[dest] = val1 - val2;
			system_log(0, "ALU", "SUB", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case MUL: {	  // Multiply two values and save them to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			REG[dest] = val1 * val2;
			system_log(0, "ALU", "MUL", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case DIV: {	  // Divide two values and save to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			if (val2 == 0) {
				system_log(2, "ALU", "DIV ZERO ", 3, dest, val1, val2);
			}
			else {
				REG[dest] = val1 / val2;
				system_log(0, "ALU", "DIV", 4, dest, val1, val2, REG[dest]);
				set_flags(REG[dest]);
			}
		}
			break;
		case INC: {	  // Increment the value of a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[dest];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : 1;
			REG[dest] = val1 + val2;
			system_log(0, "ALU", "INC", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case DEC: {	  // Decrement the value of a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[dest];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : 1;
			REG[dest] = val1 - val2;
			system_log(0, "ALU", "DEC", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case SHR: {	  // Right shift a value and save to a register
			word dest = (inst >> 8) & 0b111;
			word val = REG[(inst >> 5) & 0b111];
			REG[dest] = val >> 1;
			system_log(0, "ALU", "SHR", 4, dest, val, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case SHL: {	  // Left shift a value and save to a register
			word dest = (inst >> 8) & 0b111;
			word val = REG[(inst >> 5) & 0b111];
			REG[dest] = val << 1;
			system_log(0, "ALU", "SHL", 4, dest, val, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case NOT: {	  // Get the bitwise complement of a value and save to a register
			word dest = (inst >> 8) & 0b111;
			word val = REG[(inst >> 5) & 0b111];
			REG[dest] = ~val;
			system_log(0, "ALU", "NOT", 4, dest, val, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case OR: {	  // OR two values and save to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			REG[dest] = val1 | val2;
			system_log(0, "ALU", "OR", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case AND: {	  // AND two values and save to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			REG[dest] = val1 & val2;
			system_log(0, "ALU", "AND", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case XOR: {	  // XOR two values and save to a register
			word dest = (inst >> 8) & 0b111;
			word val1 = REG[(inst >> 5) & 0b111];
			word val2 = (inst >> 4) & 0b1 ? sign_extend(inst & 0b1111, 4) : REG[inst & 0b111];
			REG[dest] = val1 ^ val2;
			system_log(0, "ALU", "XOR", 4, dest, val1, val2, REG[dest]);
			set_flags(REG[dest]);
		}
			break;
		case LDR: {	  // Load a value from memory using an address stored in a register
			word dest = (inst >> 8) & 0b111;
			word base = REG[(inst >> 5) & 0b111];
			word offset = sign_extend(inst & 0b11111, 5);
			word val = RAM[base + offset];
			REG[dest] = val;
			system_log(0, "REG", "LDR", 2, dest, val);
			set_flags(val);
		}
			break;
		case LD: {	  // Load a value from memory into a register
			word dest = (inst >> 8) & 0b111;
			word address = sign_extend(inst & 0xff, 8);
			word val = RAM[address];
			REG[dest] = val;
			system_log(0, "RAM", "LD", 3, dest, address, val);
			set_flags(val);
		}
			break;
		case LDI: {	  // Load the corresponding value of a pointer in memory
			word dest = (inst >> 8) & 0b111;
			word address = RAM[sign_extend(inst & 0xff, 8)];
			word val = RAM[address];
			REG[dest] = val;
			system_log(0, "RAM", "LDI", 3, dest, address, val);
			set_flags(val);
		}
			break;
		case ST: {	 // Store a value from a register in memory
			word val = REG[(inst >> 8) & 0b111];
			word address = sign_extend(inst & 0xff, 8);
			RAM[address] = val;
			system_log(0, "RAM", "ST", 2, address, val);
			set_flags(val);
		}
			break;
		case STI: {	 // Store a value from a register at the value of a pointer in memory
			word val = REG[(inst >> 8) & 0b111];
			word address = RAM[sign_extend(inst & 0xff, 8)];
			RAM[address] = val;
			system_log(0, "RAM", "ST", 2, address, val);
			set_flags(val);
		}
			break;
		case STR: {	 // Store a value from a register in memory using an address stored in a register
			word val = REG[(inst >> 8) & 0b111];
			word base = (inst >> 5) & 0b111;
			word offset = inst & 0b11111;
			RAM[base + offset] = val;
			system_log(0, "RAM", "ST", 2, base + offset, val);
			set_flags(val);
		}
			break;
		case BR: {	 // Branch to a different instruction in memory based on the conditional flags
			word cond = (inst >> 8) & 0b111;
			if (cond & REG[RCOND]) {
				word offset = sign_extend(inst & 0xff, 8);
				word address = REG[RPC] + offset;
				REG[RPC] = address;
				system_log(0, "CPU", "BR", 1, offset);
			}
		}
			break;
		case RET:	 // Return from subroutine
		case JMP: {	 // Uncondtionally jump to a different instruction in memory
			word address = (inst >> 4) & 1 ? inst & 0b1111 : REG[(inst >> 5) & 0b111];
			REG[RPC] = address;
			system_log(0, "CPU", "JMP", 1, address);
		}
			break;
		case JSR: {  // Jump to subroutine address
			word address = REG[RPC] + sign_extend(inst & 0x3ff, 10);
			REG[R7] = REG[RPC];
			REG[RPC] = address;
			system_log(0, "CPU", "JSR", 1, address);
		}
			break;
		case JSRR: { // Jump to subroutine address stored in a register
			word address = REG[(inst >> 5) & 0b111];
			REG[R7] = REG[RPC];
			REG[RPC] = address;
			system_log(0, "CPU", "JSRR", 1, address);
		}
			break;
		case CLR: {  // Clear a register
			word reg = (inst >> 8) & 0b111;
			REG[reg] = 0;
			system_log(0, "REG", "CLR", 1, reg);
		}
			break;
		case IN: {   // Scan for input and store the value in a register
			system_log(4, "CPU", "IN", 0);
			char* end;
			char buf[10];
			if (!fgets(buf, sizeof buf, stdin))
					break;
			buf[strlen(buf) - 1] = 0;
			int buffer = strtol(buf, &end, 10);
			if (buffer > UINT16_MAX) {
				system_log(1, "CPU", "Buffer Overflow", 0);
			}
			else {
				REG[(inst >> 8) & 0b111] = (word)buffer;
			}
			fflush(stdin);
		}
			break;
		case OUT: {	 // Output the value in a register
			system_log(3, "CPU", "OUT", 0);
			word reg = (inst >> 5) & 0b111;
			word format = (inst >> 8) & 0b11;
			if (format == 1) {
				printf("%d", (__int16)REG[reg]);
			}
			else if (format == 2) {
				printf("%c", (char)REG[reg]);
			} else {
				printf("%d", REG[reg]);
			}
			if ((inst >> 10) & 1) {
				printf("\n");
			}
		}
			break;
		case SET: {	 // Set the value of a register
			word dest = (inst >> 8) & 0b111;
			word val = inst & 0xff;
			REG[dest] = val;
			system_log(0, "REG", "SET", 2, dest, val);
		}
			break;
		case GETS: {
			word dest = REG[(inst >> 5) & 0b111];
			system_log(4, "CPU", "GETS", 1, dest);
			char buffer[128];
			scanf_s("%127s", buffer, (unsigned)_countof(buffer));
			int i = 0;
			do {
				RAM[dest + i] = buffer[i];
			} while (i < 32 && buffer[i++] != '\n');
			getchar();
		}
			break;
		default:     // Invalid opcode case
			system_log(2, "CPU", "Invalid Opcode", 1, opcode);
			return 1;
			break;
	}
	return ist_fetch(REG[RPC]); // Fetch the next instruction
}

char* readFileBytes(const char* name, long* size)
{
	#pragma warning(suppress : 4996)
	FILE* fl = fopen(name, "r");
	char* ret = NULL;
	if (fl != 0) {
		fseek(fl, 0, SEEK_END);
		long len = ftell(fl);
		*size = len;
		ret = malloc(len);
		fseek(fl, 0, SEEK_SET);
		fread(ret, 1, len, fl);
		fclose(fl);
	}
	else {
		*size = 0;
	}
	return ret;
}

int start() {                                           // Start the system
	system_log(0, "CPU", "Starting", 0);
	long size;
	char* buffer = readFileBytes("data.inst", &size);
	for (int i = 0; i < MEM_SIZE && i < size / 2 + 1; i++) {// Load the program memory into memory
		RAM[i] = *((uint16_t*)&buffer[i * 2]);
	}
	free(buffer);
	return ist_fetch(0);                                // Fetch the first instruction
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		printf("Clock Cycle and Log level required\n");
	}
	else {
		CYCLE = atoi(argv[1]);
		LOG_LEVEL = atoi(argv[2]);
		if (!start()) {
			while (!state) {                                // Clock cycle
				if (clock() - clock_time >= CYCLE) {
					clock_time = clock();
					state = ist_execute();
				}
			}
		}
	}
	return 0;
}