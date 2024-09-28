#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <sstream>
#include <tuple>
#include <cstring>

using namespace std;

class QMatrix {
        size_t size_;
        double* data_;

    public:
        double& at(size_t i, size_t j) {
            return data_[i * size_ + j];
        }

        ~QMatrix() {
            if (data_)
                delete[] data_;
            size_ = 0;
        }

        QMatrix(QMatrix const &other) {
            if (&other == this)
                return;

            if (other.data_ == nullptr) {
                if (data_ != nullptr) {
                    size_ = 0;
                    delete[] data_;
                    data_ = nullptr;
                }
                return;
            }

            size_ = other.size_;
            data_ = new double[size_ * size_];
            memcpy(data_, other.data_, size_ * size_ * sizeof(double));
        }

        QMatrix& operator=(QMatrix const & other) {
            QMatrix tmp(other);
            swap(size_, tmp.size_);
            swap(data_, tmp.data_);
            return *this;
        }

        QMatrix(QMatrix && other) {
            if (&other == this)
                return;

            size_ = other.size_;
            data_ = other.data_;
            other.data_ = nullptr;
        }

        QMatrix& operator=(QMatrix && other) {
            QMatrix tmp(move(other));
            swap(size_, tmp.size_);
            swap(data_, tmp.data_);
            return *this;
        }

        QMatrix() {
            size_ = 0;
            data_ = nullptr;
        }

        QMatrix(size_t size) {
            size_ = size;
            data_ = new double[size_ * size_];
            memset(data_, 0, size_ * sizeof(double));
        }

        size_t size() const {
            return size_;
        }

        static tuple<QMatrix, QMatrix> createFromTestFile(fstream& f) {
            stringstream sstream;

            sstream << f.rdbuf();

            int size = 0;
            sstream >> size;

            QMatrix A(size);
            QMatrix B(size);
            
            for (int i = 0; i < size; i++) {
                vector<double> raw((size_t) size);
                for (int j = 0; j < size; j++) {
                    sstream >> raw[j];
                }
                memcpy(A.data_ + i * size, raw.data(), size * sizeof(double));
            }

            for (int i = 0; i < size; i++) {
                vector<double> raw((size_t) size);
                for (int j = 0; j < size; j++) {
                    sstream >> raw[j];
                }
                memcpy(B.data_ + i * size, raw.data(), size * sizeof(double));
            }

            return {A, B};
        }

        static QMatrix multiply(QMatrix& A, QMatrix& B, int n_threads);
};
