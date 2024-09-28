#include "../matrix.hpp"
#include <thread>

void multForThread(QMatrix* A, QMatrix* B, QMatrix* C, size_t start, size_t end);

QMatrix QMatrix::multiply(QMatrix& A, QMatrix& B, int n_threads) {
    if (A.size_ != B.size_) 
        throw invalid_argument("A and B matrices have different size");
    
    size_t size = A.size_;
    QMatrix C(size);

    size_t dist = size / n_threads;

    vector<thread> workers(n_threads -1);

    for (int i = 0; i < n_threads - 1; i++) {
        workers[i] = thread(&multForThread, &A, &B, &C, i * dist, (i + 1) * dist);
    }
    multForThread(&A, &B, &C, (n_threads - 1) * dist, size);

    for (auto& thr : workers) {
        thr.join();
    }

    return C;
}

void multForThread(QMatrix* A, QMatrix* B, QMatrix* C, size_t start, size_t end) {
    auto size = A->size();
    
    for (int i = start; i < end; i++) {
        for (int k = 0; k < size; k++) {
            for (int j = 0; j < size; j++) {
                C->at(i, j) += A->at(i, k) * B->at(k, j);
            }
        }
    }
}