import os

os.system('python parser.py')
os.system('g++ -std=c++11 simulator.cpp')
os.system('./a.out')