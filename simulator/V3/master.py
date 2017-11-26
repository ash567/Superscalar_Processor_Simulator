import os

os.system('python parser.py')
os.system('g++ -std=c++11 -g simulator.cpp')
os.system('./a.out')