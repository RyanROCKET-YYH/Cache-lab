/**
 * @file trans.c
 * @brief Contains various implementations of matrix transpose
 *
 * Each transpose function must have a prototype of the form:
 *   void trans(size_t M, size_t N, double A[N][M], double B[M][N],
 *              double tmp[TMPCOUNT]);
 *
 * All transpose functions take the following arguments:
 *
 *   @param[in]     M    Width of A, height of B
 *   @param[in]     N    Height of A, width of B
 *   @param[in]     A    Source matrix
 *   @param[out]    B    Destination matrix
 *   @param[in,out] tmp  Array that can store temporary double values
 *
 * A transpose function is evaluated by counting the number of hits and misses,
 * using the cache parameters and score computations described in the writeup.
 *
 * Programming restrictions:
 *   - No out-of-bounds references are allowed
 *   - No alterations may be made to the source array A
 *   - Data in tmp can be read or written
 *   - This file cannot contain any local or global doubles or arrays of doubles
 *   - You may not use unions, casting, global variables, or
 *     other tricks to hide array data in other forms of local or global memory.
 *
 * TODO: fill in your name and Andrew ID below.
 * @author Yuhong YAO <yuhongy>
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "cachelab.h"

/**
 * @brief Checks if B is the transpose of A.
 *
 * You can call this function inside of an assertion, if you'd like to verify
 * the correctness of a transpose function.
 *
 * @param[in]     M    Width of A, height of B
 * @param[in]     N    Height of A, width of B
 * @param[in]     A    Source matrix
 * @param[out]    B    Destination matrix
 *
 * @return True if B is the transpose of A, and false otherwise.
 */
#ifndef NDEBUG
static bool is_transpose(size_t M, size_t N, double A[N][M], double B[M][N]) {
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                fprintf(stderr,
                        "Transpose incorrect.  Fails for B[%zd][%zd] = %.3f, "
                        "A[%zd][%zd] = %.3f\n",
                        j, i, B[j][i], i, j, A[i][j]);
                return false;
            }
        }
    }
    return true;
}
#endif

/*
 * You can define additional transpose functions here. We've defined
 * some simple ones below to help you get started, which you should
 * feel free to modify or delete.
 */

/**
 * @brief A simple baseline transpose function, not optimized for the cache.
 *
 * Note the use of asserts (defined in assert.h) that add checking code.
 * These asserts are disabled when measuring cycle counts (i.e. when running
 * the ./test-trans) to avoid affecting performance.
 */
static void trans_basic(size_t M, size_t N, double A[N][M], double B[M][N],
                        double tmp[TMPCOUNT]) {
    assert(M > 0);
    assert(N > 0);

    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; j++) {
            B[j][i] = A[i][j];
        }
    }

    assert(is_transpose(M, N, A, B));
}

/**
 * @brief A simple transpose function, optimized for the cache using blocking.
 *
 * The transpose function is optimized for cache using a blocking strategy.
 * The basic idea is to decompose the matrix into blocks of size block_size x
 * block_size and then transpose each block.
 *
 * The function handles the elements in two cases:
 * 1. Non-diagonal elements: These are directly transposed from matrix A to B.
 * 2. Diagonal elements: These are first stored in the tmp array and then write
 * to matrix B.
 *
 *
 * The choice of the offset (`i - ii + 64`) for tmp is strategic, designed to
 * reduce potential cache conflicts between tmp, A, and B.
 *
 * The blocking startegy is cited from hint section.
 * citation:  http://csapp.cs.cmu.edu/public/waside/waside-blocking.pdf
 */
static void trans_student(size_t M, size_t N, double A[N][M], double B[M][N],
                          double tmp[TMPCOUNT]) {
    assert(M > 0);
    assert(N > 0);

    const size_t block_size = 8;

    for (size_t ii = 0; ii < N; ii += block_size) {
        for (size_t jj = 0; jj < M; jj += block_size) {
            for (size_t i = ii; (i < ii + block_size && i < N); i++) {
                for (size_t j = jj; (j < jj + block_size && j < M); j++) {
                    if (i != j) {
                        B[j][i] = A[i][j];
                    } else if (ii == jj) {
                        tmp[i - ii + 64] = A[i][j];
                    }
                }
                if (ii == jj && i < M) {
                    B[i][i] = tmp[i - ii + 64];
                }
            }
        }
    }
    assert(is_transpose(M, N, A, B));
}

/**
 * @brief This is only dealing with 1024x1024 matrix in order to get better
 * performance.
 *
 * This is a midway checkpoint when I am optimizing the transpose and I found it
 * has the min cycles for 1024 matrix.
 *
 * The blocking startegy is cited from hint section.
 * citation:  http://csapp.cs.cmu.edu/public/waside/waside-blocking.pdf
 */
static void trans_1024(size_t M, size_t N, double A[N][M], double B[M][N],
                       double tmp[TMPCOUNT]) {
    assert(M == 1024);
    assert(N == 1024);

    const size_t block_size = 8;

    for (size_t ii = 0; ii < N; ii += block_size) {
        for (size_t jj = 0; jj < M; jj += block_size) {
            if (ii == jj) {
                for (size_t i = ii; i < ii + block_size; i++) {
                    for (size_t j = jj; j < jj + block_size; j++) {
                        tmp[(i - ii) * block_size + (j - jj)] = A[i][j];
                    }
                }
                for (size_t j = jj; j < jj + block_size; j++) {
                    for (size_t i = ii; i < ii + block_size; i++) {
                        B[j][i] = tmp[(i - ii) * block_size + (j - jj)];
                    }
                }
            } else {
                for (size_t i = ii; i < ii + block_size; i++) {
                    for (size_t j = jj; j < jj + block_size; j++) {
                        B[j][i] = A[i][j];
                    }
                }
            }
        }
    }
    assert(is_transpose(M, N, A, B));
}

/**
 * @brief A contrived example to illustrate the use of the temporary array.
 *
 * This function uses the first four elements of tmp as a 2x2 array with
 * row-major ordering.
 */
static void trans_tmp(size_t M, size_t N, double A[N][M], double B[M][N],
                      double tmp[TMPCOUNT]) {
    assert(M > 0);
    assert(N > 0);

    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < M; j++) {
            size_t di = i % 2;
            size_t dj = j % 2;
            tmp[2 * di + dj] = A[i][j];
            B[j][i] = tmp[2 * di + dj];
        }
    }

    assert(is_transpose(M, N, A, B));
}

/**
 * @brief The solution transpose function that will be graded.
 *
 * You can call other transpose functions from here as you please.
 * It's OK to choose different functions based on array size, but
 * this function must be correct for all values of M and N.
 */
static void transpose_submit(size_t M, size_t N, double A[N][M], double B[M][N],
                             double tmp[TMPCOUNT]) {
    if (M == N && M == 1024) {
        trans_1024(M, N, A, B, tmp);
    } else {
        trans_student(M, N, A, B, tmp);
    }
}

/**
 * @brief Registers all transpose functions with the driver.
 *
 * At runtime, the driver will evaluate each function registered here, and
 * and summarize the performance of each. This is a handy way to experiment
 * with different transpose strategies.
 */
void registerFunctions(void) {
    // Register the solution function. Do not modify this line!
    registerTransFunction(transpose_submit, SUBMIT_DESCRIPTION);

    // Register any additional transpose functions
    registerTransFunction(trans_basic, "Basic transpose");
    registerTransFunction(trans_tmp, "Transpose using the temporary array");
    // registerTransFunction(trans_student, "Student transpose");
}
