#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

/* 
    IST: mooooojjjjjjjjwwwwwwwwrrrrrrrrIiiiiiiii
    o - opcode
    j - jmp
	J - goto
    w - write
    r - read A
    i - read B / Immediate
	I - activate immediate
	m - RAM instruction mode
*/
                            //JmooooojjjjjjjjwwwwwwwwrrrrrrrrIiiiiiiii
#define GOTO      (uint64_t)0b1000000000000000000000000000000000000000
#define RAM_IST   (uint64_t)0b0100000000000000000000000000000000000000
#define OPCODE    (uint64_t)0b0011111000000000000000000000000000000000
#define JMP	      (uint64_t)0b0000000111111110000000000000000000000000
#define WRITE     (uint64_t)0b0000000000000001111111100000000000000000
#define READ_A    (uint64_t)0b0000000000000000000000011111111000000000
#define IMD_ON    (uint64_t)0b0000000000000000000000000000000100000000
#define READ_B    (uint64_t)0b0000000000000000000000000000000011111111
#define IMMEDIATE (uint64_t)0b0000000000000000000000000000000011111111

#define IST_SIZE sizeof(uint64_t)
#define MEM_SIZE 256
#define REG_SIZE 16

const long CYCLE = 250;
const uint8_t LOG_LEVEL = 0;

/*
	Opcodes:
	00000 - ADD
	00001 - SUB
	00010 - MULT
	00011 - DIV
	00100 - OR
	00101 - AND
	00110 - XOR
	00111 - SHIFT_R
	01000 - SHIFT_L
	01001 - JE
	01010 - JLE
	01011 - JGE
	01100 - JL
	01101 - JG
	01110 - JNE
	01111 - SCAN
	10000 - RAM Write
	10001 - RAM Read [B]
*/

const uint64_t ROM[MEM_SIZE] = { 
	//JmooooojjjjjjjjwwwwwwwwrrrrrrrrIiiiiiiii
	0b0001111000000000000000100000000000000000,
	0b0001111000000000000001000000000000000000,
	0b0000000000000000000001100000000100000001,
	0b0000000000000000000010000000000000000000,
	0b0001010000001000000000000000001000000010,
	0b0000000000000000000010100000000000000001,
	0b0000000000000000000000100000000000000010,
	0b0000000000000000000001000000000000000101,
	0b0000101000000000000010100000010000000011,
	0b0001001000000100000000000000000000000101,
	0b0000000000000000000010000000001000000100,
	0b0001000000000000000000100000000000000001,
	0b0001000000000000000001100000000000000011,
	0b0001010111110110000000000000011000000010,
	0b0010010000000000000000000000000000000100,
	0b1001001000000000000000000000000000000000
	//JmooooojjjjjjjjwwwwwwwwrrrrrrrrIiiiiiiii
};

typedef unsigned __int8 byte;

byte RAM[MEM_SIZE];
uint32_t REG[REG_SIZE];

uint32_t path_a, path_b, output;

union IST {
	byte bytes[IST_SIZE];
	uint64_t chunk;
};

union IST ist;

clock_t clock_time = 0;

int state = 0;
int ram_mode = 0;

uint32_t pc = 0;

void system_log(const int level, const char* locale, const char* message, const int num,...) {
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
			type = "OUTPUT";
		}
		else if (level == 4) {
			type = "INPUT";
		}
		else {
			printf("[N/A]");
			return;
		}
		printf("[%s]|%s| %s ", type, locale, message);
		va_start(valist, num);
		for (int i = 0; i < num; i++) {
			printf("%lu ", va_arg(valist, uint32_t));
		}
		va_end(valist);
		printf("\n");
	}
}

int ist_fetch(const byte address, const uint64_t ram) {
	if (ram) {
		return ram_ist_fetch(address);
	}
	if (address >= MEM_SIZE) {
		system_log(1, "ROM", "Address out of reach", 1, address);
		return 1;
	}
	ist.chunk = ROM[address];
	system_log(0, "ROM", "IST fetch", 1, address);
	pc = address;
	return 0;
}

int ram_ist_fetch(const byte address) {
	if (address > MEM_SIZE - 9) {
		system_log(1, "RAM", "Address out of reach", 1, address);
		return 1;
	}
	for (byte i = 0; i < IST_SIZE; i++) {
		ist.bytes[i] = RAM[address + i];
	}
	system_log(0, "RAM", "IST fetch", 1, address);
	pc = address / IST_SIZE;
	return 0;
}

int ist_execute() {
	output = path_a = path_b = 0;
	byte buffer, branch = 0, ram_write = 0, mem_branch = 0;
	if ((OPCODE & ist.chunk) >> 33 == 15) {
		system_log(4, "CPU", "SCAN", 0);
		uint32_t temp;
		scanf_s("%lu", &temp);
		if (temp > UINT64_MAX) {
			system_log(1, "CPU", "Buffer Overflow", 0);
		}
		else {
			path_b = temp;
		}
	} else if ((OPCODE & ist.chunk) >> 33 == 17) {
		if (READ_B & ist.chunk) {
			buffer = (READ_B & ist.chunk);
			path_b = RAM[buffer - 1];
			system_log(0, "RAM", "Read Address", 2, buffer, path_b);
		}
	} else {
		if (IMD_ON & ist.chunk) {
			path_b = IMMEDIATE & ist.chunk;
			system_log(0, "CPU", "Immediate", 1, path_b);
		}
		else if (READ_B & ist.chunk) {
			buffer = (READ_B & ist.chunk);
			if (buffer <= REG_SIZE) {
				path_b = REG[buffer - 1];
				system_log(0, "CPU", "Read B REG", 2, buffer, path_b);
			}
			else {
				system_log(1, "CPU", "Register out of reach", 1, buffer);
			}
		}
	}
	if (READ_A & ist.chunk) {
		buffer = (READ_A & ist.chunk) >> 9;
		path_a = REG[buffer - 1];
		system_log(0, "CPU", "Read A REG", 2, buffer, path_a);
	}
	if (pc >= MEM_SIZE - 1) {
		pc = -1;
	}
	output = path_a + path_b;
	switch ((OPCODE & ist.chunk) >> 33) {
	case 1:
		output = path_a - path_b;
		system_log(0, "ALU", "SUB", 2, path_a, path_b);
		break;
	case 2:
		output = path_a * path_b;
		system_log(0, "ALU", "MULT", 2, path_a, path_b);
		break;
	case 3:
		system_log(0, "ALU", "DIV", 2, path_a, path_b);
		if (path_b == 0) {
			system_log(2, "ALU", "Division by Zero", 2, path_a, path_b);
			output = 256;
		}
		else {
			output = path_a / path_b;
		}
		break;
	case 4:
		output = path_a | path_b;
		system_log(0, "ALU", "OR", 2, path_a, path_b);
		break;
	case 5:
		output = path_a & path_b;
		system_log(0, "ALU", "AND", 2, path_a, path_b);
		break;
	case 6:
		output = path_a ^ path_b;
		system_log(0, "ALU", "XOR", 2, path_a, path_b);
		break;
	case 7:
		output = (path_a + path_b) >> 1;
		system_log(0, "ALU", "SHIFT_R", 2, path_a, path_b);
		break;
	case 8:
		output = (path_a + path_b) << 1;
		system_log(0, "ALU", "SHIFT_L", 2, path_a, path_b);
		break;
	case 9:
		if (path_a == path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		system_log(0, "ALU", "JE", 2, path_a, path_b);
		break;
	case 10:
		if (path_a <= path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		system_log(0, "ALU", "JLE", 2, path_a, path_b);
		break;
	case 11:
		if (path_a >= path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		system_log(0, "ALU", "JGE", 2, path_a, path_b);
		break;
	case 12:
		if (path_a < path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		system_log(0, "ALU", "JL", 2, path_a, path_b);
		break;
	case 13:
		if (path_a > path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		system_log(0, "ALU", "JG", 2, path_a, path_b);
		break;
	case 14:
		if (path_a != path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		system_log(0, "ALU", "JNE", 2, path_a, path_b);
		break;
	case 16:
		ram_write = 1;
		if (WRITE & ist.chunk) {
			buffer = (WRITE & ist.chunk) >> 17;
			RAM[buffer - 1] = output;
			system_log(0, "RAM", "Write Address", 2, buffer, output);
		}
	case 18:
		system_log(3, "CPU", "", 1, output);
		break;
	}
	if (!ram_write && WRITE & ist.chunk) {
		buffer = (WRITE & ist.chunk) >> 17;
		REG[buffer - 1] = output;
		system_log(0, "REG", "Write REG", 2, buffer, output);
	}
	uint64_t ram_ist = (RAM_IST & ist.chunk) >> 38;
	if (ram_ist) {
		system_log(0, "CPU", "RAM Fetch Call", 0);
	}
	if (GOTO & ist.chunk) {
		system_log(0, "CPU", "GOTO", 1, output);
		mem_branch = 1;
		branch = 1;
	}
	if (branch || ram_mode != ram_ist) {
		if (ram_ist || mem_branch) {
			branch = 0;
			pc = output;
		}
		pc += branch;
		if (pc >= MEM_SIZE) {
			pc -= MEM_SIZE;
		}
		if (ram_mode != ram_ist) {
			ram_mode = ram_ist;
			pc = output;
		}
		ist_fetch(pc, ram_ist);
	}
	else {
		ist_fetch(++pc, ram_ist);
	}
	return 0;
}

int start() {
	system_log(0, "CPU", "Starting", 0);
	return ist_fetch(0, 0);
}

int main() {
	start();
	while (!state) {
		if (clock() - clock_time >= CYCLE) {
			clock_time = clock();
			state = ist_execute();
		}
	}
	return 0;
}