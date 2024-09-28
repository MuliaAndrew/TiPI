#include "../matrix.hpp"

QMatrix QMatrix::multiply(QMatrix& A, QMatrix& B, int n_threads) {
    if (A.size_ != B.size_) 
        throw invalid_argument("A and B matrices have different size");
    
    size_t size = A.size_;
    QMatrix C(size);

    for (int i = 0; i < size; i++) {
        for (int k = 0; k < size; k++) {
            for (int j = 0; j < size; j++) {
                C.at(i, j) += A.at(i, k) * B.at(k, j);
            }
        }
    }

    return C;
}