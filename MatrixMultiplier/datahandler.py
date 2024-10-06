import numpy as np
import matplotlib.pyplot as plt
import random
import sys


def createTestSetIndex(dir_path: str):
    f_index = open("./%s/index.txt" % dir_path, "+xt")
    f_index.write("0")
    f_index.close()


def testGenerator(N: int, dir_path: str):
    try: 
        f_index = open("./%s/index.txt" % dir_path, "+rt")
    except:
        createTestSetIndex(dir_path)
    finally:
        f_index = open("./%s/index.txt" % dir_path, "+rt")

    test_index = int(f_index.readline())
    f_index.close()
    res = str("%d\n" % N)

    for _ in range(2):
        for i in range(N):
            matrix = ""
            for k in range(N):
                matrix += "%.5f " % np.float64(random.gauss(0, 5))

            res += "\n" + matrix
        res += "\n"    
    
    
    
    test_file = open("./%s/test%d.txt" % (dir_path,test_index), "+xt")
    test_file.write(res)
    test_file.close()

    f_index = open("./%s/index.txt" % dir_path, "+wt")
    f_index.write(str(test_index + 1))
    f_index.close()

def createTestSet1(dir_to_test_set = "tests/testset1"): 
    for i in range(20):
        testGenerator(2048, dir_to_test_set)

def createTestSet2(dir_to_test_set = "tests/testset2"):
    for i in range(8):
        for _ in range(5):
            testGenerator(512 * (i + 1), dir_to_test_set)

def analyzeVersionSpeed(version: int):
    try:
        time_stat = open("./tests/results/time%d.txt" % version, "rt")
    except:
        print("no file for speed analysis given")
    
    tests_time = []
    
    for test_time in time_stat.read().split():
        test_time = float(test_time)
        tests_time.append(test_time)
    
    print(sum(tests_time, .0)/len(tests_time))

def analyzeNumOfThreadsSpeed(version: int):
    try:
        nthreads_stat = open("./tests/results/nthreads%d.txt" % version, "rt")
    except:
        print("no file for speed dependency on number of threads analysis given")
        return
    
    nthreads_time = []
    
    for nthread_time in nthreads_stat.read().split():
        nthreads_time.append(float(nthread_time))

    y = []
    for i in range(16):
        y.append((nthreads_time[i] + nthreads_time[i + 16] + nthreads_time[i + 32])/3.0)
    
    x = range(1, 17)

    plt.plot(x, y, "sk")
    plt.xlabel("number of threads")
    plt.ylabel("msecё")

    plt.savefig(fname = "tests/results/Version%d_time_nthreads_dependency.png" % version)

def analyzeSizeOfMatrixSpeed(version: int):
    try:
        msize_stat = open("./tests/results/msize%d.txt" % version, "rt")
    except:
        print("no file for speed dependency on matrix size analysis given")
        return
    
    msize_times = [0 for _ in range(40)]
    
    l = msize_stat.readline()
    while l != "":
        n = int(l.removeprefix("tests/testset2/test").removesuffix(".txt\n"))
        l = msize_stat.readline()
        msize_times[n] = int(l)
        l = msize_stat.readline()
    
    msize_avg_times = [0 for _ in range(8)]
    for m in range(8):
        msize = 0
        for i in range(5):
            msize += msize_times[m * 5 + i]
        msize_avg_times[m] = msize / 5.0
    
    y = msize_avg_times
    x = range(512, 4097, 512)

    plt.plot(x, y, "sk")
    plt.xlabel("size of matrix in bytes")
    plt.ylabel("msec")

    plt.savefig(fname = "tests/results/Version%d_time_msize_dependency.png" % version)

if __name__ == "__main__":
    match (sys.argv[1]):
        case "gt1":
            createTestSet1(sys.argv[2])
            
        case "gt2":
            createTestSet2(sys.argv[2])
        
        case "analyze_speed":
            analyzeVersionSpeed(sys.argv[2])

        case "analyze_nthreads":
            analyzeNumOfThreadsSpeed(int(sys.argv[2]))

        case "analyze_msize":
            analyzeSizeOfMatrixSpeed(int(sys.argv[2]))
