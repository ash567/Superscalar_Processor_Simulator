#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <iomanip>

using namespace std;

#define NREG 16 					// Number of registers
#define CODE_SIZE 1024 				// Memory size


int pc; 							// program counter
int curr_pc;						// copy of pc for current cycle
string instr_reg; 					// instruction fetched during IF stage
string alu_output_index;			// index of register computed in ALU output
string alu_output;					// alu output register
string mem_access_output_index;		// index of register computed in mem access
string mem_access_output;			// memory access output register
bool is_stage_nop[4] = {1,1,1,1};	// tells if a given stage has a NOP instruction. 

short int reg[NREG]; 				// register file
char mem[CODE_SIZE]; 				// memory

struct instruction
{
	string opcode;
	string val0;	// what is being updated.
	string val1; 	// what is needed.
	string val2; 	// what is needed.
	string val3; 	// what is needed.
};

bool stall_flag = false;
bool curr_stall_flag = false;

bool flush_flag = false;
bool curr_flush_flag = false;

instruction instructions[5];

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

void fetch_instr()
{
	if(curr_stall_flag)
	{
		return;
	}
	string ins = "";
	for(int i = 0; i < 2; i++)
	{
		ins += inttobin(mem[curr_pc + i], 8);
	}
	instr_reg = ins;
	pc = pc + 2;
	is_stage_nop[0] = false;
}

void decode_instr()
{
	if(!curr_stall_flag)
	{
		if(is_stage_nop[0] || curr_flush_flag)
		{
			is_stage_nop[1] = true;
			instructions[1].opcode = "";
			instructions[1].val0 = "";
			instructions[1].val1 = "";
			instructions[1].val2 = "";
			instructions[1].val3 = "";
			return;
		}
		is_stage_nop[1] = false;
		instructions[1].opcode = instr_reg.substr(0, 3);

		// Operations: ADD, SUB, MUL
		if(instructions[1].opcode == "000" || instructions[1].opcode == "001" || instructions[1].opcode == "010")
		{
			bool immediate = instr_reg[3] - '0';	
			instructions[1].val1 = "R" + (instr_reg.substr(8, 4));
			if(immediate)
				instructions[1].val2 = (instr_reg.substr(12));
			else
				instructions[1].val2 = "R" + (instr_reg.substr(12));
			instructions[1].val0 = (instr_reg.substr(4, 4));
			instructions[1].val3 = "";
		}

		// Operation: LD
		else if(instructions[1].opcode == "011")
		{
			instructions[1].val0 = (instr_reg.substr(4, 4));
			bool immediate = instr_reg[3] - '0';
			if(immediate)
				instructions[1].val1 = (instr_reg.substr(8, 4));
			else
				instructions[1].val1 = "R" + (instr_reg.substr(8, 4));
			instructions[1].val2 = "R" + (instr_reg.substr(12));
			instructions[1].val3 = "";
		}

		// Operation: SD
		else if(instructions[1].opcode == "100")
		{
			instructions[1].val0 = "";
			instructions[1].val3 = "R" + (instr_reg.substr(12));
			bool immediate = instr_reg[3] - '0';
			if(immediate)
				instructions[1].val1 = (instr_reg.substr(4, 4));
			else
				instructions[1].val1 = "R" + (instr_reg.substr(4, 4));
			instructions[1].val2 = "R" + (instr_reg.substr(8, 4));
		}

		// Operation: JMP
		else if(instructions[1].opcode == "101")
		{
			instructions[1].val0 = "";
			instructions[1].val1 = (instr_reg.substr(3, 8));
			instructions[1].val2 = "";
			instructions[1].val3 = "";
			int offset = bintoint(instructions[1].val1);
			pc = pc - 4 + offset;
			flush_flag = true;
		}

		// Operation: BEQZ
		else if(instructions[1].opcode == "110")
		{
			instructions[1].val0 = "";
			instructions[1].val1 = "R" + (instr_reg.substr(3, 4));
			instructions[1].val2 = (instr_reg.substr(7, 8));
			instructions[1].val3 = "";
		}

		// Operation: HLT
		else if(instructions[1].opcode == "111")
		{
			// Do nothing.
		}

		// Operation: Unknown
		else
		{
			cout << "Unrecognized opcode: " << instructions[1].opcode << endl;
			exit(-1);
		}
	}

	// Stalling and operand forwarding:
	// Register File value is stale if:
	// 0. There is a LOAD instruction for that register in the EXE stage. (stall)
	// 1. That register is stored in the ALU output and decode instruction is not a branch. (use operand forwarding)
	// 2. That register is stored in the MEM output and decode instruction is not a branch. (use operand forwarding)
	// 3. That register is stored in the ALU output and decode instruction is a branch. (forward and stall)
	// 4. That register is stored in the MEM output and decode instruction is a branch. (forward and stall)		
	
	// val1
	if(instructions[1].val1 != "" && (instructions[1].val1)[0] == 'R')
	{
		bool temp_stall_flag = false;
		if(!is_stage_nop[2] && instructions[2].opcode == "011" && instructions[2].val0 == (instructions[1].val1).substr(1))
			temp_stall_flag = true;
		else if(instructions[1].opcode != "110")
		{
			if(!is_stage_nop[2] && alu_output_index == (instructions[1].val1).substr(1))
			{
				// TODO: Remove
				cout << "found val1 at alu_output" << endl;
				instructions[1].val1 = alu_output;
			}
			else if(!is_stage_nop[3] && mem_access_output_index == (instructions[1].val1).substr(1))
			{
				// TODO: Remove
				cout << "found val1 at mem_access_output" << endl;
				instructions[1].val1 = mem_access_output;
			}
			else
			{
				// TODO: Remove
				cout << "found val1 in reg file" << endl;
				instructions[1].val1 = inttobin(reg[bintouint((instructions[1].val1).substr(1))], 16);
			}
		}
		else if(instructions[1].opcode == "110")
		{
			if(!is_stage_nop[2] && alu_output_index == (instructions[1].val1).substr(1))
			{
				// TODO: Remove
				cout << "found val1 at alu_output" << endl;
				instructions[1].val1 = alu_output;
				temp_stall_flag = true;
			}
			else if(!is_stage_nop[3] && mem_access_output_index == (instructions[1].val1).substr(1))
			{
				// TODO: Remove
				cout << "found val1 at mem_access_output" << endl;
				instructions[1].val1 = mem_access_output;
				temp_stall_flag = true;	
			}
			else
			{
				// TODO: Remove
				cout << "found val1 in reg file" << endl;
				instructions[1].val1 = inttobin(reg[bintouint((instructions[1].val1).substr(1))], 16);
			}
		}
		stall_flag = stall_flag || temp_stall_flag;
	}

	if(instructions[1].val2 != "" && (instructions[1].val2)[0] == 'R')
	{
		bool temp_stall_flag = false;
		if(!is_stage_nop[2] && instructions[2].opcode == "011" && instructions[2].val0 == (instructions[1].val2).substr(1))
			temp_stall_flag = true;
		else if(instructions[1].opcode != "110")
		{
			if(!is_stage_nop[2] && alu_output_index == (instructions[1].val2).substr(1))
				instructions[1].val2 = alu_output;
			else if(!is_stage_nop[3] && mem_access_output_index == (instructions[1].val2).substr(1))
				instructions[1].val2 = mem_access_output;
			else
				instructions[1].val2 = inttobin(reg[bintouint((instructions[1].val2).substr(1))], 16);
		}
		else if(instructions[1].opcode == "110")
		{
			if(!is_stage_nop[2] && alu_output_index == (instructions[1].val2).substr(1))
			{
				instructions[1].val2 = alu_output;
				temp_stall_flag = true;
			}
			else if(!is_stage_nop[3] && mem_access_output_index == (instructions[1].val2).substr(1))
			{
				instructions[1].val2 = mem_access_output;
				temp_stall_flag = true;	
			}
			else
			{
				instructions[1].val2 = inttobin(reg[bintouint((instructions[1].val2).substr(1))], 16);
			}
		}
		stall_flag = stall_flag || temp_stall_flag;
	}

	if(instructions[1].val3 != "" && (instructions[1].val3)[0] == 'R')
	{
		bool temp_stall_flag = false;
		if(!is_stage_nop[2] && instructions[2].opcode == "011" && instructions[2].val0 == (instructions[1].val3).substr(1))
			temp_stall_flag = true;
		else if(instructions[1].opcode != "110")
		{
			if(!is_stage_nop[2] && alu_output_index == (instructions[1].val3).substr(1))
				instructions[1].val3 = alu_output;
			else if(!is_stage_nop[3] && mem_access_output_index == (instructions[1].val3).substr(1))
				instructions[1].val3 = mem_access_output;
			else
				instructions[1].val3 = inttobin(reg[bintouint((instructions[1].val3).substr(1))], 16);
		}
		else if(instructions[1].opcode == "110")
		{
			if(!is_stage_nop[2] && alu_output_index == (instructions[1].val3).substr(1))
			{
				instructions[1].val3 = alu_output;
				temp_stall_flag = true;
			}
			else if(!is_stage_nop[3] && mem_access_output_index == (instructions[1].val3).substr(1))
			{
				instructions[1].val3 = mem_access_output;
				temp_stall_flag = true;	
			}
			else
			{
				instructions[1].val3 = inttobin(reg[bintouint((instructions[1].val3).substr(1))], 16);
			}
		}
		stall_flag = stall_flag || temp_stall_flag;
	}

	if(instructions[1].opcode == "110" && (instructions[1].val1)[0] != 'R' && !stall_flag)
	{
		// Branch taken
		if(bintoint(instructions[1].val1) == 0) 
		{
			// TODO: Remove
			cout << "BEQZ branch taken" << endl;
			int offset = bintoint(instructions[1].val2);
			pc = pc - 4 + offset;
			flush_flag = true;
		}
	}
}

void execute()
{
	alu_output_index = "";
	alu_output = "";
	if(is_stage_nop[1] || curr_stall_flag)
	{
		is_stage_nop[2] = true;
		instructions[2].opcode = "";
		instructions[2].val0 = "";
		instructions[2].val1 = "";
		instructions[2].val2 = "";
		instructions[2].val3 = "";
		return;
	}
	instructions[2] = instructions[1];
	if(instructions[2].opcode == "101" || instructions[2].opcode == "110")
		is_stage_nop[2] = true;
	else
	{
		is_stage_nop[2] = false;
		if(instructions[2].opcode == "000")
		{
			int val1 = bintoint(instructions[2].val1);
			int val2 = bintoint(instructions[2].val2);
			int ans = val1 + val2;
			alu_output_index = instructions[2].val0;
			alu_output = inttobin(ans, 16);
		}
		else if(instructions[2].opcode == "001")
		{
			int val1 = bintoint(instructions[2].val1);
			int val2 = bintoint(instructions[2].val2);
			int ans = val1 - val2;
			alu_output_index = instructions[2].val0;
			alu_output = inttobin(ans, 16);
		}
		else if(instructions[2].opcode == "010")
		{
			int val1 = bintoint(instructions[2].val1);
			int val2 = bintoint(instructions[2].val2);
			int ans = val1 * val2;
			alu_output_index = instructions[2].val0;
			alu_output = inttobin(ans, 16);
		}
		else if(instructions[2].opcode == "011")
		{
			int val1 = bintoint(instructions[2].val1);
			int val2 = bintoint(instructions[2].val2);
			int ans = val1 + val2;
			alu_output_index = "";
			alu_output = inttobin(ans, 16);
		}
		else if(instructions[2].opcode == "100")
		{
			int val1 = bintoint(instructions[2].val1);
			int val2 = bintoint(instructions[2].val2);
			int ans = val1 + val2;
			alu_output_index = "";
			alu_output = inttobin(ans, 16);
		}
		else if(instructions[2].opcode == "111")
		{
			alu_output_index = "";
			alu_output = "";
		}
		else
		{
			cout << "Unrecognized opcode in ALU unit." << endl;
			exit(-1);
		}
	}
}

void mem_access()
{
	mem_access_output = "";
	mem_access_output_index = "";
	if(is_stage_nop[2])
	{
		is_stage_nop[3] = true;
		instructions[3].opcode = "";
		instructions[3].val0 = "";
		instructions[3].val1 = "";
		instructions[3].val2 = "";
		instructions[3].val3 = "";
		return;
	}
	is_stage_nop[3] = false;
	instructions[3] = instructions[2];
	mem_access_output_index = alu_output_index;
	mem_access_output = alu_output;
	if(instructions[3].opcode == "011")
	{
		mem_access_output_index = instructions[3].val0;
		mem_access_output = inttobin(mem[bintoint(alu_output)], 8) + inttobin(mem[bintoint(alu_output)+1], 8);
	}
	else if(instructions[3].opcode == "100")
	{
		int loc = bintoint(alu_output);
		mem[loc] = bintoint((instructions[3].val3).substr(0,8));
		mem[loc+1] = bintoint((instructions[3].val3).substr(8));
	}
}

// Returns true when HLT is encountered.
bool write_back()
{
	if(is_stage_nop[3])
	{
		is_stage_nop[4] = true;
		return false;
	}
	is_stage_nop[4] = false;
	instructions[4] = instructions[3];
	if(instructions[4].opcode == "000" || instructions[4].opcode == "001" || instructions[4].opcode == "010")
	{
		reg[bintouint((instructions[4].val0).substr(1))] = bintoint(mem_access_output);
	}
	else if(instructions[4].opcode == "011")
	{
		reg[bintouint((instructions[4].val0).substr(1))] = bintoint(mem_access_output);
	}
	else if(instructions[4].opcode == "111")
	{
		return true;
	}
	return false;
}

// runs all the codes till HLT is encountered
void run()
{
	bool exit_flag = false;
	int cycle_num = 0;
	while(!exit_flag)
	{
		curr_pc = pc;
		curr_stall_flag = stall_flag;
		stall_flag = false;
		curr_flush_flag = flush_flag;
		flush_flag = false;
		if(curr_flush_flag)
		{
			instr_reg = "";
			is_stage_nop[0] = true;
		}
		exit_flag = write_back();
		mem_access();
		execute();
		decode_instr();
		fetch_instr();
		// TODO: Remove
		cout << "cycle num: " << ++cycle_num << endl;
		cout << "is_stage_nop: " << is_stage_nop[0] << ' ' << is_stage_nop[1] << ' ' << is_stage_nop[2] << ' ' << is_stage_nop[3] << ' ' << is_stage_nop[4] << endl; 
		cout << "fetched instruction: " << instr_reg << endl;
		cout << "decode instruction opcode: " << instructions[1].opcode << endl;
		cout << "decode instruction val0: " << instructions[1].val0 << endl;
		cout << "decode instruction val1: " << instructions[1].val1 << endl;
		cout << "decode instruction val2: " << instructions[1].val2 << endl;
		cout << "decode instruction val3: " << instructions[1].val3 << endl;
		cout << "execute instruction opcode: " << instructions[2].opcode << endl;
		cout << "execute instruction val0: " << instructions[2].val0 << endl;
		cout << "execute instruction val1: " << instructions[2].val1 << endl;
		cout << "execute instruction val2: " << instructions[2].val2 << endl;
		cout << "execute instruction val3: " << instructions[2].val3 << endl;
		cout << "execute output index: " << alu_output_index << endl;
		cout << "execute output: " << alu_output << endl; 
		cout << "mem access instruction opcode: " << instructions[3].opcode << endl;
		cout << "mem access instruction val0: " << instructions[3].val0 << endl;
		cout << "mem access instruction val1: " << instructions[3].val1 << endl;
		cout << "mem access instruction val2: " << instructions[3].val2 << endl;
		cout << "mem access instruction val3: " << instructions[3].val3 << endl;
		cout << "mem output index: " << mem_access_output_index << endl;
		cout << "mem output: " << mem_access_output << endl;
		cout << endl;
	}
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
				string s = inttobin(atoi(op2.c_str()), 16);
				for(int i = 0; i < 2; i++)
					mem[index + i] = bintoint(s.substr(i * 8, 8));
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
			for(int i = 0 ; i < 2; i++)
				mem[ptr + i] = bintoint(ins.substr((i * 8), 8));

			ptr = ptr + 2;
		}
	}
	
	pc = 0;

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