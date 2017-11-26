#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <iomanip>

using namespace std;

#define NREG 16 // Number of registers
#define CODE_SIZE 1024 // Memeory size


int pc; // program counter
short unsigned int instr_reg; // instruction register
short int reg[NREG]; // register file
char mem[CODE_SIZE]; // memory


// ------- some bit operations on strings ------------ //

// convert bit string to unsigned int.
unsigned bintouint(string str)
{
	unsigned n = 0;
	for(int i = 0; i < str.size(); i++)
		n = n * 2 + (str[i] - '0');
	return n;
}

//flip all the bits of the bit string.
string flip(string s)
{
	string r = "";
	for(int i = 0; i < s.size(); i++)
		r += ((s[i] - '0') + 1) % 2 + '0';
	return r;
}

//add 1 to all the bit string.
string add1(string s)
{
	for(int i = s.size() - 1; i >= 0; i--)
	{
		if(s[i] == '1')
			s[i] = '0';
		else
		{
			s[i] = '1';
			break;
		}
	}
	return s;
}

// unsigned int to bit string
string uinttobin(unsigned n, int l)
{
	string s = "";
	while(n > 0)
	{
		s += ((n % 2) + '0');
		n = n/2;
	}

	if(s.size() > l)
		s = s.substr(0, l);
	else
		s += string(l-s.size(), '0');

	reverse(s.begin(), s.end());
	return s;
}

// convert a bit string to signed integer
int bintoint(string s)
{
	int sign = 1;
	if(s[0] == '1')
	{
		sign = -1;
		s = add1(flip(s));
	}

	return (sign * int(bintouint(s)));
}

// converting a signed integer n to the a bitstring of size l
string inttobin(int n, int l)
{
	int sign = 1;
	if(n < 0)
		sign = -1;
	n = n * sign;
	string bitstr = uinttobin(n, l);
	if(sign == -1)
		bitstr = add1(flip(bitstr));

	return bitstr;
}

// ----------- Implementing operations ---------- //

// alu operations
int calc_op2(string op, int a1, int a2)
{
	if(op == "000")
		return (a1 + a2);
	else if(op == "001")
		return (a1 - a2);
	else if(op == "010")
		return (a1 * a2);
	else
		cout << "ERROR: Unrecognized ALU instruction" << endl;

	return 0;
}

// alu unit
void alu(string op, string ins)
{
	int i1, i2, i3, flag = ins[0] - '0';
	ins = ins.substr(1);

	i1 = bintouint(ins.substr(0, 4));
	ins = ins.substr(4);
	i2 = bintouint(ins.substr(0, 4));
	ins = ins.substr(4);

	if(flag)
	{
		i3 = bintoint(ins);
		reg[i1] = calc_op2(op, reg[i2], i3);
	}

	else
	{
		i3 = bintouint(ins);
		reg[i1] = calc_op2(op, reg[i2], reg[i3]);
	}

	return;
}

// load implementation
void ld(string ins)
{
	int i1, i2, i3, addr, flag = ins[0] - '0';
	string bitval = "";
	ins = ins.substr(1);
	i1 = bintouint(ins.substr(0, 4));
	ins = ins.substr(4);

	if(flag)
		i2 = bintoint(ins.substr(0, 4));
	else
		i2 = bintouint(ins.substr(0, 4));

	ins = ins.substr(4);
	i3 = bintouint(ins);

	if(flag)
		addr = (i2 + reg[i3]);
	else
		addr = (reg[i2] + reg[i3]);

	for(int i= 0; i < sizeof(short int)/sizeof(char); i++)
		bitval += inttobin(mem[addr + i], sizeof(char) * 8);

	reg[i1] = bintoint(bitval);
	return;
}

// storing data implementation
void sd(string ins)
{
	int i1, i2, i3, addr, flag = ins[0] - '0';
	string bitval = "";
	ins = ins.substr(1);

	if(flag)
		i1 = bintoint(ins.substr(0, 4));
	else
		i1 = bintouint(ins.substr(0, 4));

	ins = ins.substr(4);
	i2 = bintouint(ins.substr(0, 4));
	ins = ins.substr(4);
	i3 = bintouint(ins);

	if(flag)
		addr = (i1 + reg[i2]);
	else
		addr = (reg[i1] + reg[i2]);

	bitval = inttobin(reg[i3], sizeof(short int) * 8);

	for(int i = 0; i < sizeof(short int)/sizeof(char); i++)
		mem[addr + i] = bintoint(bitval.substr(i*sizeof(char) * 8, sizeof(char) * 8));

	return;
}

// jmp instruction impelmentation
void jmp(string ins)
{
	ins = ins.substr(0, 8); // 8 is the max label length
	pc = (pc - sizeof(short int)/sizeof(char)) + bintoint(ins);
	return;
}

// conditional jump instruction implementation
void beqz(string ins)
{
	int index = bintouint(ins.substr(0, 4));
	ins = ins.substr(4);
	ins = ins.substr(0, ins.size()-1);
	if(reg[index] == 0)
		pc = (pc - sizeof(short int)/sizeof(char)) + bintoint(ins);
	return;
}

// runs a single instruction
// return 0 if the program has to halt and 1 if it has to continue.
int run_inst()
{
	string ins = "";
	for(int i = 0; i < sizeof(short int)/sizeof(char); i++)
		ins += inttobin(mem[pc + i], sizeof(char) * 8);

	instr_reg = bintoint(ins);
	// in all jump instructions decrease the pc by two and then jump
	pc = pc + sizeof(short int)/sizeof(char);

	// cout << "here" << endl;

	string op = ins.substr(0, 3);
	ins = ins.substr(3);
	int opcode = bintouint(op);
	// cout << opcode << endl;

	switch(opcode)
	{
		case(0): alu("000", ins); break;
		case(1): alu("001", ins); break;
		case(2): alu("010", ins); break;
		case(3): ld(ins); break;
		case(4): sd(ins); break;
		case(5): jmp(ins); break;
		case(6): beqz(ins); break;
		case(7): return 0; break;
	}

	return 1;
}

// runs all the codes till HLT is encountered
void run()
{
	while(run_inst() != 0);
		// cout << "reg3:" << reg[3] << endl;
	return;
}

// initialization of registers and memory
void initialize()
{	
	// initializing the memory and registers
	ifstream mem_file("memory.txt");
	if(mem_file.is_open())
	{
		string ini;
		while(getline(mem_file, ini))
		{	
			istringstream iss(ini);
			string op1, op2;
			int flag = 0, index = 0;
			iss >> op1 >> op2;

			if(op1[0] == 'R')
			{
				flag = 1;
				op1 = op1.substr(1);
			}

			index = atoi(op1.c_str());

			if(flag)
				reg[index] = atoi(op2.c_str());
			else
			{
				string s = inttobin(atoi(op2.c_str()), sizeof(short int) * 8);
				for(int i = 0; i < sizeof(short int) / sizeof(char); i++)
					mem[index + i] = bintoint(s.substr(i * sizeof(char) * 8, sizeof(char) * 8));
				// mem[index] = atoi(op2.c_str());
			}
		}
	}

	// reading the instructions
	ifstream ins_file("instructions.txt");
	if(ins_file.is_open())
	{
		string ins;
		int ptr = 0;

		while(getline(ins_file, ins))
		{
			// TR: debugging
			// cout << ins << endl;
			// cout << "here" << endl;
			for(int i =0 ; i < sizeof(short int)/sizeof(char); i++)
				mem[ptr + i] = bintoint(ins.substr((i*sizeof(char) * 8), sizeof(char) * 8));

			ptr = ptr + sizeof(short int)/sizeof(char);
		}
	}
	
	pc = 0;
	instr_reg = 0;

	return;
}

// dumping register values
void dump_reg()
{
	ofstream  reg_dump_file("reg_dump.txt");
	for(int i = 0; i < NREG; i ++)
		reg_dump_file << "R" << i << " " << reg[i] << endl;
	return;
}

// dumping the memory
void dump_mem()
{
	ofstream  mem_dump_file("cs13b061.txt");
	for(int i = 0; i < CODE_SIZE; i ++)
	{
		string s = inttobin(int(mem[i]), 8);
		mem_dump_file << setfill('0') << setw(4) << i << " : " << s.substr(0, 4) << " " << s.substr(4, 4) << endl;
	}
	return;
}

int main()
{	
	initialize();
	run();
	dump_reg();
	dump_mem();
	return 0;
}