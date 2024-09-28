#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <sstream>
#include <tuple>
#include "matrix.hpp"
#include <chrono>
#include "x86intrin.h"


int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Bad arguments\n";
        return 1;
    }
    string path = string(argv[1]);
    int n_threads = atoi(argv[2]);

    fstream f;
    try {
        f = fstream(path, ios_base::openmode::_S_in);
    }
    catch (const exception& e) {
        cerr << e.what() << '\n';
        return 1;
    }
    
    auto [A, B] = QMatrix::createFromTestFile(f);

    f.close();
    
    QMatrix C;

    auto timer_start = chrono::steady_clock::now();

    try {
        C = QMatrix::multiply(A, B, n_threads);
    }
    catch (const exception& e) {
        cerr << e.what() << '\n';
        return 1;
    }

    auto timer_end = chrono::steady_clock::now();

    cout << chrono::duration_cast<chrono::microseconds>(timer_end - timer_start).count() << "\n";

    return 0;
}