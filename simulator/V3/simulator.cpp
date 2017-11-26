#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <queue>
#include <list>
#include <deque>

using namespace std;

#define NREG 16 					// Number of registers
#define CODE_SIZE 1024 				// Memory size
#define Q_SIZE 4

#define ADD_DELAY 2
#define SUB_DELAY 3
#define MUL_DELAY 5
#define LD_DELAY 6

struct instruction
{
	string opcode;
	string val0;	// what is being updated.
	string val1; 	// what is needed.
	string val2; 	// what is needed.
	string val3; 	// what is needed.
};

struct res_entry
{
	int opcode;
	bool busy;
	bool valid[4];
	int val[4];
	int answer;
	bool executing;
};

queue <string> instruction_q;

// 2 reservation entries for each type of instruction.
// LD and ST have only 1 reservation entry, since we want to maintain inorder.
res_entry res_station[7];

queue <int> addq(deque<int>(ADD_DELAY, -1));
queue <int> subq(deque<int>(SUB_DELAY, -1));
queue <int> mulq(deque<int>(MUL_DELAY, -1));
queue <int> ldq(deque<int>(LD_DELAY, -1));

list <int> reorder_buffer;

int pc; 							// program counter
int curr_pc;						// copy of pc for current cycle
bool stall_flag;

short int reg[NREG]; 				// register file
vector <int> reg_ind(NREG, -1);		// index of reservation entry that contains the latest value of the register. -1 if reg file value is not stale.
char mem[CODE_SIZE]; 				// memory


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


// Fetch stage.
// Fetches as many instructions as possible into the instruction queue.
void fetch()
{
	if(stall_flag)
		return;
	for(int i = instruction_q.size(); i < Q_SIZE; i++)
	{
		string ins = "";
		ins += inttobin(mem[pc], 8);
		ins += inttobin(mem[pc+1], 8);
		instruction_q.push(ins);
		pc = pc + 2;
	}
}

// Returns the decoded instruction
// success = 0 only when the instruction is conditional branch and the register value is not available
instruction decode(string ins, int& success)
{
	instruction i;
	i.opcode = ins.substr(0, 3);
	// Operations: ADD, SUB, MUL
	if(i.opcode == "000" || i.opcode == "001" || i.opcode == "010")
	{
		bool immediate = ins[3] - '0';	
		i.val1 = "R" + (ins.substr(8, 4));
		if(immediate)
			i.val2 = (ins.substr(12));
		else
			i.val2 = "R" + (ins.substr(12));
		i.val0 = (ins.substr(4, 4));
		success = 1;
		return i;
	}
	else if(i.opcode == "011")
	{
		i.val0 = (ins.substr(4, 4));
		bool immediate = ins[3] - '0';
		if(immediate)
			i.val1 = (ins.substr(8, 4));
		else
			i.val1 = "R" + (ins.substr(8, 4));
		i.val2 = "R" + (ins.substr(12));
		success = 1;
		return i;
	}
	// Operation: SD
	else if(i.opcode == "100")
	{
		i.val3 = "R" + (ins.substr(12));
		bool immediate = ins[3] - '0';
		if(immediate)
			i.val1 = (ins.substr(4, 4));
		else
			i.val1 = "R" + (ins.substr(4, 4));
		i.val2 = "R" + (ins.substr(8, 4));
		success = 1;
		return i;		
	}
	// Operation: JMP
	else if(i.opcode == "101")
	{
		i.val1 = (ins.substr(3, 8));
		success = 1;
		return i;
	}
	// Operation: BEQZ
	else if(i.opcode == "110")
	{
		i.val1 = "R" + (ins.substr(3, 4));
		i.val2 = (ins.substr(7, 8));
		success = reg_ind[bintouint((i.val1).substr(1))] == -1 ? 1 : 0;
		return i;
	}
	// Operation: HLT
	else if(i.opcode == "111")
	{
		success = 1;
		return i;
	}
	// Operation: Unknown
	else
	{
		cout << "Unrecognized opcode: " << i.opcode << endl;
		exit(-1);
	}
}

void get_operand(res_entry& re, instruction ins, int op_num)
{
	string val;
	if(op_num == 1)
		val = ins.val1;
	else if(op_num == 2)
		val = ins.val2;
	else if(op_num == 3)
		val = ins.val3;
	else
	{
		cout << "Unknown operand number in get_operand: " << op_num << endl;
		exit(-1);
	}
	if(val.length() == 0)
	{
		re.valid[op_num] = false;
		return;
	}
	if(val[0] == 'R')
	{
		int regn = bintouint((val).substr(1));
		if(reg_ind[regn] == -1)
		{
			re.val[op_num] = reg[regn];
			re.valid[op_num] = true;
		}
		else
		{
			if(res_station[reg_ind[regn]].valid[0])
			{
				re.val[op_num] = res_station[reg_ind[regn]].answer;
				re.valid[op_num] = true;
			}
			else
			{
				re.val[op_num] = reg_ind[regn];
				re.valid[op_num] = false; 
			}
		}
	}
	else
	{
		re.val[op_num] = bintoint(val);
		re.valid[op_num] = true;
	}
}

// Tries to assign ins to a free reservation entry.
// Returns 0 iff ins could not be assigned to a free reservation station entry.
int dispatch(instruction ins)
{
	int opcode = bintouint(ins.opcode);
	int reservation_entry_num;
	if(opcode == 7)
	{
		reorder_buffer.push_back(0);
		return 1;
	}
	else if((opcode == 3 || opcode == 4))
	{
		if(res_station[6].busy)
			return 0;
		reservation_entry_num = 6;
	}
	else if((opcode == 0 || opcode == 1 || opcode == 2))
	{
		if(res_station[opcode*2].busy && res_station[opcode*2+1].busy)
			return 0;
		else if(res_station[opcode*2].busy)
			reservation_entry_num = opcode*2+1;
		else
			reservation_entry_num = opcode*2;
	}
	else
	{
		cout << "Unknown Opcode in Dispatch: " << opcode << endl;
		exit(-1);
	}
	if((ins.val0).length() > 0)
		res_station[reservation_entry_num].val[0] = bintouint(ins.val0);
 	get_operand(res_station[reservation_entry_num], ins, 1);
	get_operand(res_station[reservation_entry_num], ins, 2);
	get_operand(res_station[reservation_entry_num], ins, 3);
	res_station[reservation_entry_num].valid[0] = false;
	res_station[reservation_entry_num].busy = true;
	res_station[reservation_entry_num].opcode = bintouint(ins.opcode);
	if(res_station[reservation_entry_num].opcode != 4)
		reg_ind[res_station[reservation_entry_num].val[0]] = reservation_entry_num;
	reorder_buffer.push_back(-reservation_entry_num-1);
	return 1;
}

// Issue stage.
// Decode instruction, issue instructions from the queue to reservation stations, if available.
// For branch instructions, unless the correct path is known, do not proceed.
// If the wrong set of instructions was fetched, clear the queue, and update the PC.
void issue()
{
	while(instruction_q.size() > 0)
	{
		string top = instruction_q.front();
		int success;
		instruction ins = decode(top, success);
		if(!success)
			return;
		if(ins.opcode == "101")
		{
			// TODO: Check for correctness
			pc = pc - 2*instruction_q.size();
			pc = pc + bintoint(ins.val1);
			while(!instruction_q.empty())
				instruction_q.pop();
			stall_flag = true;
			return;
		}
		else if(ins.opcode == "110")
		{
			int reg_num = bintouint((ins.val1).substr(1));
			int val;
			if(reg_ind[reg_num] == -1)
				val = reg[reg_num];
			else
			{
				if(!res_station[reg_ind[reg_num]].valid[0])
					return;
				val = res_station[reg_ind[reg_num]].answer;
			}
			if(val == 0)
			{
				// TODO: Check for correctness
				pc = pc - 2*instruction_q.size();
				pc = pc + bintoint(ins.val2);
				while(!instruction_q.empty())
					instruction_q.pop();
				stall_flag = true;
				return;
			}
			else
				instruction_q.pop();
		}
		else
		{
			success = dispatch(ins);
			if(success)
				instruction_q.pop();
			else
				return;
		}
	}
}

bool ready(const res_entry& re)
{
	bool ans = re.valid[1] && re.valid[2];
	if(re.opcode == 4)
		ans = ans && re.valid[3];
	return ans && re.busy && !re.executing;
}

void update_operands()
{
	for(int i = 0; i < 7; i++)
	{
		if(res_station[i].busy && !ready(res_station[i]))
		{
			if(!res_station[i].valid[1])
				if(res_station[res_station[i].val[1]].valid[0])
				{
					res_station[i].val[1] = res_station[res_station[i].val[1]].answer;
					res_station[i].valid[1] = true;
				}
			if(!res_station[i].valid[2])
				if(res_station[res_station[i].val[2]].valid[0])
				{
					res_station[i].val[2] = res_station[res_station[i].val[2]].answer;
					res_station[i].valid[2] = true;
				}
		}
	}
	if(res_station[6].busy && !ready(res_station[6]) && res_station[6].opcode == 4)
	{
		if(!res_station[6].valid[3])
			if(res_station[res_station[6].val[3]].valid[0])
			{
				res_station[6].val[3] = res_station[res_station[6].val[3]].answer;
				res_station[6].valid[3] = true;
			}
	}
}

void execute()
{
	// ADD
	if(ready(res_station[0]))
	{
		addq.push(0);
		res_station[0].executing = true;
	}
	else if(ready(res_station[1]))
	{
		addq.push(1);
		res_station[1].executing = true;
	}
	else
		addq.push(-1);
	// SUB
	if(ready(res_station[2]))
	{
		subq.push(2);
		res_station[2].executing = true;		
	}
	else if(ready(res_station[3]))
	{
		subq.push(3);
		res_station[3].executing = true;
	}
	else
		subq.push(-1);
	// MUL
	if(ready(res_station[4]))
	{
		mulq.push(4);
		res_station[4].executing = true;
	}
	else if(ready(res_station[5]))
	{
		mulq.push(5);
		res_station[5].executing = true;
	}
	else
		mulq.push(-1);
	// LD/ST
	if(ready(res_station[6]))
	{
		ldq.push(6);
		res_station[6].executing = true;	
	}
	else
		ldq.push(-1);
	int add_index = addq.front();
	int sub_index = subq.front();
	int mul_index = mulq.front();
	int ld_index = ldq.front();
	addq.pop();
	subq.pop();
	mulq.pop();
	ldq.pop();
	if(add_index != -1)
	{
		res_station[add_index].answer = res_station[add_index].val[1]+res_station[add_index].val[2];
		res_station[add_index].valid[0] = true;
		auto i = reorder_buffer.begin();
		while(i != reorder_buffer.end())
		{
			if(*i == -add_index-1)
			{
				*i = add_index+1;
				break;
			}
			++i;
		}
	}
	if(sub_index != -1)
	{
		res_station[sub_index].answer = res_station[sub_index].val[1]-res_station[sub_index].val[2];
		res_station[sub_index].valid[0] = true;
		auto i = reorder_buffer.begin();
		while(i != reorder_buffer.end())
		{
			if(*i == -sub_index-1)
			{
				*i = sub_index+1;
				break;
			}
			++i;
		}
	}
	if(mul_index != -1)
	{
		res_station[mul_index].answer = res_station[mul_index].val[1]*res_station[mul_index].val[2];
		res_station[mul_index].valid[0] = true;
		auto i = reorder_buffer.begin();
		while(i != reorder_buffer.end())
		{
			if(*i == -mul_index-1)
			{
				*i = mul_index+1;
				break;
			}
			++i;
		}
	}
	if(ld_index != -1)
	{
		if(res_station[ld_index].opcode == 3)
		{
			int mem_index = res_station[ld_index].val[1]+res_station[ld_index].val[2];
			res_station[ld_index].answer = bintoint(inttobin(mem[mem_index], 8)+inttobin(mem[mem_index+1], 8));
		}
		else
			res_station[ld_index].answer = res_station[ld_index].val[1]+res_station[ld_index].val[2];
		res_station[ld_index].valid[0] = true;
		auto i = reorder_buffer.begin();
		while(i != reorder_buffer.end())
		{
			if(*i == -ld_index-1)
			{
				*i = ld_index+1;
				break;
			}
			++i;
		}
	}
}

// Returns false when HLT is encountered.
bool reorder()
{
	while(reorder_buffer.size() > 0)
	{
		int res_index = reorder_buffer.front();
		if(res_index == 0)
			return false;
		else if(res_index > 0)
		{
			res_index = res_index - 1;
			if(res_station[res_index].opcode != 4)
			{
				if(reg_ind[res_station[res_index].val[0]] == res_index)
				{
					reg[res_station[res_index].val[0]] = res_station[res_index].answer;
					reg_ind[res_station[res_index].val[0]] = -1;
				}
			}
			else
			{
				int mem_loc = res_station[res_index].answer;
				string ans = inttobin(res_station[res_index].val[3], 16);
				mem[mem_loc] = bintoint(ans.substr(0, 8));
				mem[mem_loc+1] = bintoint(ans.substr(8));
			}
			reorder_buffer.pop_front();
			res_station[res_index].busy = false;
			res_station[res_index].executing = false;
		}
		else
			break;
	}
	return true;
}

// runs all the codes till HLT is encountered
void run()
{
	bool end = false;
	int cycle_num = 1;
	while(!end)
	{
		// cout << "here0" << endl;
		stall_flag = false;
		end = !reorder();
		// cout << "here" << endl;	
		if(end)
			break;
		execute();
		issue();
		fetch();
		update_operands();
		// TODO: Remove.
		cout << "Cycle number: " << cycle_num++ << endl;
		for(int i = 0; i < 7; i++)
		{
			if(!res_station[i].busy)
				continue;
			cout << "Reservation number: " << i << endl;
			cout << "Opcode: " << res_station[i].opcode << endl;
			cout << "Busy: " << res_station[i].busy << endl;
			for(int j = 0; j < 4; j++)
				if(j == 0 || res_station[i].valid[j])
					cout << "Val " << j << ": " << res_station[i].val[j] << endl;
				else
					cout << "Val " << j << ": " << "Res Entry " << res_station[i].val[j] << endl;
			cout << endl;
		}
		cout << "R2 val: " << reg[2] << endl; 
		cout << endl << endl;
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