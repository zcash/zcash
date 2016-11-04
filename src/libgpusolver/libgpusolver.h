/* MIT License
 *
 * Copyright (c) 2016 Omar Alvarez <omar.alvarez@udc.es>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __GPU_SOLVER_H
#define __GPU_SOLVER_H

#include <cstdio>
#include <csignal>
#include <iostream>

#include "crypto/equihash.h"
#include "libclwrapper.h"


//#include "param.h"
//#include "blake.h"
#include <cassert>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

// The maximum size of the .cl file we read in and compile
#define MAX_SOURCE_SIZE 	(0x200000)

#define EK 9
#define EN 200
#define DIGITBITS_S	(EN/(EK+1))

class GPUSolverCancelledException : public std::exception
{
    virtual const char* what() const throw() {
        return "GPU Equihash solver was cancelled";
    }
};

enum GPUSolverCancelCheck
{
    ListGenerationGPU,
    ListSortingGPU
};

class GPUSolver {

public:
	GPUSolver();
	GPUSolver(unsigned platform, unsigned device);
	~GPUSolver();
        bool run(unsigned int n, unsigned int k, uint8_t *header, size_t header_len, uint64_t nonce,
		            const std::function<bool(std::vector<unsigned char>)> validBlock,
				const std::function<bool(GPUSolverCancelCheck)> cancelled,
			crypto_generichash_blake2b_state base_state);

private:
	cl_gpuminer * miner;
	bool GPU;
	bool initOK;
	static const uint32_t PROOFSIZE = 1 << EK;
	//TODO 20?
	sols_t * indices;
	uint32_t n_sol;
	//Avg
	uint32_t counter = 0;
	float sum = 0.f;
	float avg = 0.f;

	bool GPUSolve200_9(uint8_t *header, size_t header_len, uint64_t nonce,
		         	const std::function<bool(std::vector<unsigned char>)> validBlock,
				const std::function<bool(GPUSolverCancelCheck)> cancelled,
			crypto_generichash_blake2b_state base_state);

};

#endif // __GPU_SOLVER_H
