#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>

/* 
    IST: mooooojjjjjjjjwwwwwwwwrrrrrrrrIRRRRiiii
    o - opcode
    j - jmp
    w - write
    r - read A
    R - read B / Immediate
    i - immediate
	I - activate immediate
	m - RAM instruction mode
    X - unused
*/

#define RAM_IST   (uint64_t)0b100000000000000000000000000000000000000
#define OPCODE    (uint64_t)0b011111000000000000000000000000000000000
#define JMP	      (uint64_t)0b000000111111110000000000000000000000000
#define WRITE     (uint64_t)0b000000000000001111111100000000000000000
#define READ_A    (uint64_t)0b000000000000000000000011111111000000000
#define IMD_ON    (uint64_t)0b000000000000000000000000000000100000000
#define READ_B    (uint64_t)0b000000000000000000000000000000011111111
#define IMMEDIATE (uint64_t)0b000000000000000000000000000000011111111

#define IST_SIZE 6
#define MEM_SIZE 256
#define REG_SIZE 16

const long CYCLE = 1000;

const uint64_t ROM[MEM_SIZE] = { 
	0b000000000000000000000100000000100000011,
	0b000000000000000000001000000000100000100,
	0b000000000000000000001100000000100000100,
	0b001001000011110000000000000000000000000,
	0b010001000000000000000000000000000000001,
	0b001111000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
	0b010000000000000000000100000010000000001,
	0b001110111111110000000000000000000000011,
	0b000000000000000000000000000000000000000,
	0b000000000000000000000000000000000000000,
};

typedef unsigned __int8 byte;

byte RAM[MEM_SIZE];
byte REG[REG_SIZE];

byte path_a, path_b, output;

union IST {
	byte bytes[IST_SIZE];
	uint64_t chunk;
};

union IST ist;

clock_t clock_time = 0;

int state = 0;
int ram_mode = 0;
unsigned int pc = 0;

void log(int level, char* locale, char* message, int num,...) {
	va_list valist;
	char* type;
	if (level == -1) {
		type = "ENTRY";
	} else if (level == 0) {
		type = "INFO";
	} else if (level == 1) {
		type = "WARN";
	}
	else if (level == 2) {
		type = "ERROR";
	}
	else {
		printf("[N/A]");
		return;
	}
	printf("[%s]|%s| %s ", type, locale, message);
	va_start(valist, num);
	for (int i = 0; i < num; i++) {
		printf("%d ", va_arg(valist, int));
	}
	va_end(valist);
	printf("\n");
}

int ist_fetch(byte address, int ram) {
	if (ram) {
		return ram_ist_fetch(address);
	}
	if (address >= MEM_SIZE) {
		return 1;
	}
	ist.chunk = ROM[address];
	log(0, "ROM", "IST fetch", 1, address);
	pc = address;
	return 0;
}

int ram_ist_fetch(byte address) {
	if (address > MEM_SIZE - 9) {
		return 1;
	}
	for (byte i = 0; i < IST_SIZE; i++) {
		ist.bytes[IST_SIZE - i - 1] = RAM[address + i];
	}
	log(0, "RAM", "IST fetch", 1, address);
	pc = address / IST_SIZE;
	return 0;
}

int ist_execute() {
	output = path_a = path_b = 0;
	log(0, "PC", "Line", 1, pc);
	byte buffer, branch = 0, ram_write = 0, mem_branch = 0;
	if ((JMP & ist.chunk) >> 25 == 255) {
		mem_branch = 1;
	}
	if ((OPCODE & ist.chunk) >> 33 == 15) {
		log(-1, "SYSTEM", "Opcode SCAN", 0);
		int temp;
		scanf_s("%d", &temp);
		path_b = (byte)temp;
	} else if ((OPCODE & ist.chunk) >> 33 == 17) {
		if (READ_B & ist.chunk) {
			buffer = (READ_B & ist.chunk);
			path_b = RAM[buffer - 1];
			log(0, "RAM", "Read Address", 2, buffer, path_b);
		}
	} else {
		if (IMD_ON & ist.chunk) {
			path_b = IMMEDIATE & ist.chunk;
			log(0, "SYSTEM", "Immediate", 1, path_b);
		}
		else if (READ_B & ist.chunk) {
			buffer = (READ_B & ist.chunk);
			if (buffer <= REG_SIZE) {
				path_b = REG[buffer - 1];
				log(0, "SYSTEM", "Read B REG", 2, buffer, path_b);
			}
			else {
				log(1, "SYSTEM", "Register index out of bound", 1, buffer);
			}
		}
	}
	if (READ_A & ist.chunk) {
		buffer = (READ_A & ist.chunk) >> 9;
		path_a = REG[buffer - 1];
		log(0, "SYSTEM", "Read A REG", 2, buffer, path_a);
	}
	if (pc >= MEM_SIZE - 1) {
		pc = -1;
	}
	output = path_a + path_b;
	switch ((OPCODE & ist.chunk) >> 33) {
	case 1:
		output = path_a - path_b;
		log(0, "SYSTEM", "Opcode SUB", 2, path_a, path_b);
		break;
	case 2:
		output = path_a * path_b;
		log(0, "SYSTEM", "Opcode MULT", 2, path_a, path_b);
		break;
	case 3:
		log(0, "SYSTEM", "Opcode DIV", 2, path_a, path_b);
		if (path_b == 0) {
			log(2, "SYSTEM", "Division by Zero", 2, path_a, path_b);
			output = 256;
		}
		else {
			output = path_a / path_b;
		}
		break;
	case 4:
		output = path_a | path_b;
		log(0, "SYSTEM", "Opcode OR", 2, path_a, path_b);
		break;
	case 5:
		output = path_a & path_b;
		log(0, "SYSTEM", "Opcode AND", 2, path_a, path_b);
		break;
	case 6:
		output = path_a ^ path_b;
		log(0, "SYSTEM", "Opcode XOR", 2, path_a, path_b);
		break;
	case 7:
		output = (path_a + path_b) >> 1;
		log(0, "SYSTEM", "Opcode SHIFT_R", 2, path_a, path_b);
		break;
	case 8:
		output = (path_a + path_b) << 1;
		log(0, "SYSTEM", "Opcode SHIFT_L", 2, path_a, path_b);
		break;
	case 9:
		if (path_a == path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		log(0, "SYSTEM", "Opcode JE", 2, path_a, path_b);
		break;
	case 10:
		if (path_a <= path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		log(0, "SYSTEM", "Opcode JLE", 2, path_a, path_b);
		break;
	case 11:
		if (path_a >= path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		log(0, "SYSTEM", "Opcode JGE", 2, path_a, path_b);
		break;
	case 12:
		if (path_a < path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		log(0, "SYSTEM", "Opcode JL", 2, path_a, path_b);
		break;
	case 13:
		if (path_a > path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		log(0, "SYSTEM", "Opcode JG", 2, path_a, path_b);
		break;
	case 14:
		if (path_a != path_b) {
			branch = (JMP & ist.chunk) >> 25;
		}
		log(0, "SYSTEM", "Opcode JNE", 2, path_a, path_b);
		break;
	case 16:
		ram_write = 1;
		if (WRITE & ist.chunk) {
			buffer = (WRITE & ist.chunk) >> 17;
			RAM[buffer - 1] = output;
			log(0, "RAM", "Write Address", 2, buffer, output);
		}
		break;
	}
	if (!ram_write && WRITE & ist.chunk) {
		buffer = (WRITE & ist.chunk) >> 17;
		REG[buffer - 1] = output;
		log(0, "REG", "Write REG", 2, buffer, output);
	}
	int ram_ist = RAM_IST & ist.chunk;
	if (branch || ram_mode != ram_ist) {
		if (mem_branch && branch) {
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
	log(0, "SYSTEM", "Starting", 0);
	return ist_fetch(0, 0);
}

int main() {
	start();
	while (!state) {
		if (clock() - clock_time >= CYCLE) {
			clock_time = clock();
			ist_execute();
		}
	}
	return 0;
}