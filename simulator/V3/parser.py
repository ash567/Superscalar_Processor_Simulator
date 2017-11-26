import re

mem_dict = {}
labels_dict = {}
instructions = []
curr_pos = 0

continue_var = True

while continue_var:
	# Take string as input, convert to unicode, replace all special characters with space. 
	s = raw_input()
	s = unicode(s, "utf-8")
	s = re.sub(r'[^\x00-\x7F]+',' ', s)
	# s = s.replace(u'\xa0', u' ')
	# stripping comments.
	if s.find('//') != -1:
		s = s[:s.find('//')]
	s = s.strip()
	if s == '':
		continue
	# Ignore commented lines.
	if s.startswith('##'):
		continue
	# If HLT command, do not loop any further.
	if s.find('HLT') != -1:
		continue_var = False
	# Initialization commands.
	if s[0] == '$':
		tokens = s.split('$')
		mem_dict[tokens[1]] = tokens[2]
	# Non-initialization commands. 
	else:
		# Tokenize, store labels in dict, remove any parentheses/brackets, and increment counter by 2.
		tokens = s.split()
		if tokens[0][-1] == ':':
			labels_dict[tokens[0][:-1]] = curr_pos
			tokens = tokens[1:]
		tokens = [tok[1:-1] if tok.startswith('(') and tok.endswith(')') else tok for tok in tokens]
		tokens = [tok.split('[') for tok in tokens]
		tokens = [tok2 for tok1 in tokens for tok2 in tok1]
		tokens = [tok[:-1] if tok.endswith(']') else tok for tok in tokens]
		instructions.append(tokens)
		curr_pos = curr_pos + 2

# Decoding Opcode helper function.
def decode_opcode(tok):
	if tok == 'ADD':
		return '000'
	elif tok == 'SUB':
		return '001'
	elif tok == 'MUL':
		return '010'
	elif tok == 'LD':
		return '011'
	elif tok == 'SD':
		return '100'
	elif tok == 'JMP':
		return '101'
	elif tok == 'BEQZ':
		return '110'
	elif tok == 'HLT':
		return '111'
	else:
		print('Unrecognized opcode:', tok)
		return None

# Adds 1 to a binary string.
def add_one(bin_string):
	if len(bin_string) == 0:
		return ''
	elif bin_string[-1] == '0':
		return bin_string[:-1] + '1'
	else:
		return add_one(bin_string[:-1]) + '0'

# Flips a binary string.
def flip(bin_string):
	return ''.join(['1' if bit == '0' else '0' for bit in bin_string])

# Returns 2's complement of a binary string.
def complement2(num, size):
	ans = bin(num)[2:][-size:]
	ans = '0'*(size-len(ans))+ans
	ans = flip(ans)
	ans = add_one(ans)
	return ans

# Returns binary encoding of a given token.
def to_bin(tok, curr_pos):
	if tok[0] == 'R':
		ans = bin(int(tok[1:]))[2:][-4:]
		ans = '0'*(4-len(ans))+ans
	elif tok[0] == '#':
		tok = tok[1:]
		if tok[0] == '-':
			ans = complement2(int(tok[1:]), 4)
		else:
			ans = bin(int(tok))[2:][-4:]
			ans = '0'*(4-len(ans))+ans
	else:
		jump_to = labels_dict[tok]
		if jump_to >= curr_pos:
			ans = bin(jump_to - curr_pos)[2:]
			ans = '0'*(8-len(ans))+ans
		else:
			ans = complement2(curr_pos - jump_to, 8)
		print tok, jump_to-curr_pos, ans
	return ans

# Convert each instruction into a binary string.
bin_instructions = []
curr_pos = 0
for ins in instructions:
	curr_ins = ''
	# print('Ins0:', ins[0])
	curr_ins = curr_ins + decode_opcode(ins[0])
	bin_toks = [to_bin(tok, curr_pos) for tok in ins[1:]]
	if ins[0] in ['ADD', 'SUB', 'MUL']:
		curr_ins = curr_ins + ('1' if ins[-1][0] == '#' else '0')
	elif ins[0] == 'LD':
		curr_ins = curr_ins + ('1' if ins[-2][0] == '#' else '0')
	elif ins[0] == 'SD':
		curr_ins = curr_ins + ('1' if ins[-3][0] == '#' else '0')
	curr_ins = curr_ins + ''.join(bin_toks)
	curr_ins = curr_ins + '0'*(16-len(curr_ins))
	bin_instructions.append(curr_ins)
	curr_pos = curr_pos + 2

# Write output to instruction and memory files.
instruction_file = open('instructions.txt', 'w')
memory_file = open('memory.txt', 'w')

for bin_instruction in bin_instructions:
	instruction_file.write(bin_instruction+'\n')
	print(bin_instruction)
for i in range(100):
	instruction_file.write('1110000000000000'+'\n')
instruction_file.close()

for key, item in mem_dict.items():
	memory_file.write(key.strip() + '\t' + str(int(item)) + '\n')
	print(key, ':', str(int(item))) 

memory_file.close()

for key, item in labels_dict.items():
	print(key, ':', item)
