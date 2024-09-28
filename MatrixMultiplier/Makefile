CXX  = g++

PROJ = matrix_multiplierV
OBJ	 = src/main.o 
RM	 = rm

CXXFLAGS 	= -march=native -ffast-math -O2 

THREADS   	= nthreads.pdf
MATSIZE	  	= matrix_size.pdf

version 	= 1

mltvers 	= src/MultiplierVersions

PROJ 		:= $(PROJ)$(version)

TESTSET1 	:= $(wildcard tests/testset1/test*.txt)
TESTSET2 	:= $(wildcard tests/testset2/test*.txt)

TEST0 		:= tests/testset1/test0.txt
TEST1		:= tests/testset1/test1.txt
TEST2 		:= tests/testset1/test2.txt
NTHREADS 	:= 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16

RUNTESTSET1 := $(foreach test, $(TESTSET1), ./$(PROJ) $(test) 8 >> tests/results/time$(version).txt;)

NTHREADTESTS1 := $(foreach n, $(NTHREADS), ./$(PROJ) $(TEST0) $(n) >> tests/results/nthreads$(version).txt;)
NTHREADTESTS2 := $(foreach n, $(NTHREADS), ./$(PROJ) $(TEST1) $(n) >> tests/results/nthreads$(version).txt;)
NTHREADTESTS3 := $(foreach n, $(NTHREADS), ./$(PROJ) $(TEST2) $(n) >> tests/results/nthreads$(version).txt;)

MSIZETESTS	:= $(foreach test, $(TESTSET2), echo $(test) >> tests/results/msize$(version).txt; ./$(PROJ) $(test) 16 >> tests/results/msize$(version).txt;)

PYTHONANALYSIS := python3 datahandler.py

$(PROJ): $(OBJ) src/V$(version).o src/matrix.hpp
	$(CXX) $(OBJ) $(CXXFLAGS) src/V$(version).o -o $(PROJ)

$(OBJ): src/main.cpp src/matrix.hpp
	$(CXX) -o $(OBJ) $(CXXFLAGS) src/main.cpp -c

setup:
	mkdir tests
	mkdir tests/results
	mkdir tests/testset1
	mkdir tests/testset2


src/V$(version).o: $(mltvers)/V$(version).cpp src/matrix.hpp
	$(CXX) $(mltvers)/V$(version).cpp -c $(CXXFLAGS) -o src/V$(version).o


test_version_speed: $(PROJ) 
	$(if $(TESTSET1), $(RUNTESTSET1), @echo "The first testset is empty, run 'make gen_tests' to generate testsets")

test_nthreads: $(PROJ) tests/results/nthreads$(version).txt
	$(PYTHONANALYSIS) analyze_nthreads $(version)

tests/results/nthreads$(version).txt: $(PROJ)
	$(if $(TESTSET1), $(NTHREADTESTS1), @echo "The first testset is empty, run 'make gen_tests' to generate testsets")
	$(if $(TESTSET1), $(NTHREADTESTS2), @echo "The first testset is empty, run 'make gen_tests' to generate testsets")
	$(if $(TESTSET1), $(NTHREADTESTS3), @echo "The first testset is empty, run 'make gen_tests' to generate testsets")

test_msize:
	$(PYTHONANALYSIS) analyze_msize $(version)

tests/results/msize$(version).txt: $(PROJ)
	$(if $(TESTSET2), $(MSIZETESTS), @echo "The second testset is empty, run 'make gen_tests' to generate testsets")

gen_tests: 
	$(if $(TESTSET1), , $(PYTHONANALYSIS) gt1 tests/testset1)
	$(if $(TESTSET2), , $(PYTHONANALYSIS) gt2 tests/testset2)

.PHONY: clean
clean:
	rm -r $(wildcard src/*.o)

.PHONY: clean_tests
clean_tests:
	rm -r $(TESTSET1)
	rm -r $(TESTSET2)
	rm -r results/*
