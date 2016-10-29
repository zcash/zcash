#pragma once

#define __CL_ENABLE_EXCEPTIONS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

// Just for Hello World Kernel
#define DATA_SIZE 100

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "cl.hpp"
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-protector"
#include "cl.hpp"
#pragma GCC diagnostic pop
#else
#include "cl.hpp"
#endif
#include <time.h>
#include <functional>

#include "sodium.h"

typedef uint8_t		uchar;
typedef uint32_t	uint;
typedef uint64_t	ulong;

#include "param.h"
#include "blake.h"
#include <cassert>

#define EQUIHASH_N 200
#define EQUIHASH_K 9

#define NUM_COLLISION_BITS (EQUIHASH_N / (EQUIHASH_K + 1))
#define NUM_INDICES (1 << EQUIHASH_K)

#define NUM_VALUES (1 << (NUM_COLLISION_BITS+1))
#define NUM_BUCKETS (1 << NUM_COLLISION_BITS)
#define DIGEST_SIZE 25

typedef struct element element_t;
typedef uint64_t digest_t[(DIGEST_SIZE + sizeof(uint64_t) - 1) / sizeof(uint64_t)];

struct element {
    uint32_t digest_index;
    uint32_t parent_bucket_index;
};


typedef struct bucket {
    unsigned size;
    element_t data[18];
} bucket_t;

typedef struct  debug_s
{
    uint32_t    dropped_coll;
    uint32_t    dropped_stor;
}               debug_t;

typedef uint32_t eh_index;

class cl_gpuminer
{

public:

	cl_gpuminer();
	~cl_gpuminer();

	static bool searchForAllDevices(unsigned _platformId, std::function<bool(cl::Device const&)> _callback);
	static bool searchForAllDevices(std::function<bool(cl::Device const&)> _callback);
	static void doForAllDevices(unsigned _platformId, std::function<void(cl::Device const&)> _callback);
	static void doForAllDevices(std::function<void(cl::Device const&)> _callback);
	static unsigned getNumPlatforms();
	static unsigned getNumDevices(unsigned _platformId = 0);
	static std::string platform_info(unsigned _platformId = 0, unsigned _deviceId = 0);
	static void listDevices();

	// Currently just prints memory of the GPU
	static bool configureGPU(
		unsigned _platformId,
		unsigned _localWorkSize,
		unsigned _globalWorkSize
	);

	bool init(
		unsigned _platformId,
		unsigned _deviceId,
		std::vector<std::string> _kernels
	);

	void run(uint8_t *header, size_t header_len, uint64_t nonce, sols_t * indices, uint32_t * n_sol, uint64_t * ptr);

	void finish();

	/* -- default values -- */
	/// Default value of the local work size. Also known as workgroup size.
	static unsigned const c_defaultLocalWorkSize;
	/// Default value of the global work size as a multiplier of the local work size
	static unsigned const c_defaultGlobalWorkSizeMultiplier;
	/// Default value of the milliseconds per global work size (per batch)
	static unsigned const c_defaultMSPerBatch;

private:
  static const unsigned int z_n = 200;
  static const unsigned int z_k = 9;
  static const size_t z_collision_bit_length = z_n / (z_k + 1);
  static const eh_index z_N = 1 << (z_collision_bit_length + 1);

	static std::vector<cl::Device> getDevices(std::vector<cl::Platform> const& _platforms, unsigned _platformId);
	static std::vector<cl::Platform> getPlatforms();
	int compare_indices32(uint32_t* a, uint32_t* b, size_t n_current_indices) {
		for(size_t i = 0; i < n_current_indices; ++i, ++a, ++b) {
		    if(*a < *b) {
		        return -1;
		    } else if(*a > *b) {
		        return 1;
		    } else {
		        return 0;
		    }
		}
		return 0;
	}
	void normalize_indices(uint32_t* indices) {
		for(size_t step_index = 0; step_index < EQUIHASH_K; ++step_index) {
		    for(size_t i = 0; i < NUM_INDICES; i += (1 << (step_index+1))) {
		        if(compare_indices32(indices+i, indices+i+(1 << step_index), (1 << step_index)) > 0) {
		            uint32_t tmp_indices[(1 << step_index)];
		            memcpy(tmp_indices, indices+i, (1 << step_index)*sizeof(uint32_t));
		            memcpy(indices+i, indices+i+(1 << step_index), (1 << step_index)*sizeof(uint32_t));
		            memcpy(indices+i+(1 << step_index), tmp_indices, (1 << step_index)*sizeof(uint32_t));
		        }
		    }
		}
	}
	char *s_hexdump(const void *_a, uint32_t a_len)
	{
		const uint8_t	*a = (const uint8_t	*) _a;
		static char		buf[1024];
		uint32_t		i;
		for (i = 0; i < a_len && i + 2 < sizeof (buf); i++)
		    sprintf(buf + i * 2, "%02x", a[i]);
		buf[i * 2] = 0;
		return buf;
	}
	size_t select_work_size_blake(void)
	{
		size_t              work_size =
		    64 * /* thread per wavefront */
		    BLAKE_WPS * /* wavefront per simd */
		    4 * /* simd per compute unit */
		    36;
		// Make the work group size a multiple of the nr of wavefronts, while
		// dividing the number of inputs. This results in the worksize being a
		// power of 2.
		while (NR_INPUTS % work_size)
		    work_size += 64;
		//debug("Blake: work size %zd\n", work_size);
		return work_size;
	}
	void sort_pair(uint32_t *a, uint32_t len)
	{
		uint32_t    *b = a + len;
		uint32_t     tmp, need_sorting = 0;
		for (uint32_t i = 0; i < len; i++)
		if (need_sorting || a[i] > b[i])
		  {
			need_sorting = 1;
			tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		  }
		else if (a[i] < b[i])
			return ;
	}

	uint32_t verify_sol(sols_t *sols, unsigned sol_i) {
		uint32_t	*inputs = sols->values[sol_i];
		uint32_t	seen_len = (1 << (PREFIX + 1)) / 8;
		uint8_t	seen[seen_len];
		uint32_t	i;
		uint8_t	tmp;
		// look for duplicate inputs
		memset(seen, 0, seen_len);
		for (i = 0; i < (1 << PARAM_K); i++)
		  {
		tmp = seen[inputs[i] / 8];
		seen[inputs[i] / 8] |= 1 << (inputs[i] & 7);
		if (tmp == seen[inputs[i] / 8])
		  {
			// at least one input value is a duplicate
			sols->valid[sol_i] = 0;
			return 0;
		  }
		  }
		// the valid flag is already set by the GPU, but set it again because
		// I plan to change the GPU code to not set it
		sols->valid[sol_i] = 1;
		// sort the pairs in place
		for (uint32_t level = 0; level < PARAM_K; level++)
		for (i = 0; i < (1 << PARAM_K); i += (2 << level))
			sort_pair(&inputs[i], 1 << level);
		return 1;
	}
	cl::Context m_context;
	cl::CommandQueue m_queue;
	std::vector<cl::Kernel> m_gpuKernels;
	/*cl::Buffer m_digests[2];
	cl::Buffer m_buckets;
	cl::Buffer m_new_digest_index;
	cl::Buffer m_blake2b_digest;
	cl::Buffer m_dst_solutions;
	cl::Buffer m_n_solutions;*/
	cl::Buffer buf_ht[2];
	cl::Buffer buf_sols;
	cl::Buffer buf_dbg;

	uint64_t		nonce;
    uint64_t		total;
	size_t dbg_size = 1 * sizeof (debug_t);	

	const cl_int zero = 0;
	uint32_t solutions;
	uint32_t * dst_solutions;

	unsigned m_globalWorkSize;
	bool m_openclOnePointOne;
	unsigned m_deviceBits;

	/// The step used in the work size adjustment
	unsigned int m_stepWorkSizeAdjust;
	/// The Work Size way of adjustment, > 0 when previously increased, < 0 when previously decreased
	int m_wayWorkSizeAdjust = 0;

	/// The local work size for the search
	static unsigned s_workgroupSize;
	/// The initial global work size for the searches
	static unsigned s_initialGlobalWorkSize;
	/// The target milliseconds per batch for the search. If 0, then no adjustment will happen
	static unsigned s_msPerBatch;
	/// Allow CPU to appear as an OpenCL device or not. Default is false
	static bool s_allowCPU;
	/// GPU memory required for other things, like window rendering e.t.c.
	/// User can set it via the --cl-extragpu-mem argument.
	static unsigned s_extraRequiredGPUMem;

  const char *get_error_string(cl_int error)
  {
  switch(error){
      // run-time and JIT compiler errors
      case 0: return "CL_SUCCESS";
      case -1: return "CL_DEVICE_NOT_FOUND";
      case -2: return "CL_DEVICE_NOT_AVAILABLE";
      case -3: return "CL_COMPILER_NOT_AVAILABLE";
      case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
      case -5: return "CL_OUT_OF_RESOURCES";
      case -6: return "CL_OUT_OF_HOST_MEMORY";
      case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
      case -8: return "CL_MEM_COPY_OVERLAP";
      case -9: return "CL_IMAGE_FORMAT_MISMATCH";
      case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
      case -11: return "CL_BUILD_PROGRAM_FAILURE";
      case -12: return "CL_MAP_FAILURE";
      case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
      case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
      case -15: return "CL_COMPILE_PROGRAM_FAILURE";
      case -16: return "CL_LINKER_NOT_AVAILABLE";
      case -17: return "CL_LINK_PROGRAM_FAILURE";
      case -18: return "CL_DEVICE_PARTITION_FAILED";
      case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

      // compile-time errors
      case -30: return "CL_INVALID_VALUE";
      case -31: return "CL_INVALID_DEVICE_TYPE";
      case -32: return "CL_INVALID_PLATFORM";
      case -33: return "CL_INVALID_DEVICE";
      case -34: return "CL_INVALID_CONTEXT";
      case -35: return "CL_INVALID_QUEUE_PROPERTIES";
      case -36: return "CL_INVALID_COMMAND_QUEUE";
      case -37: return "CL_INVALID_HOST_PTR";
      case -38: return "CL_INVALID_MEM_OBJECT";
      case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
      case -40: return "CL_INVALID_IMAGE_SIZE";
      case -41: return "CL_INVALID_SAMPLER";
      case -42: return "CL_INVALID_BINARY";
      case -43: return "CL_INVALID_BUILD_OPTIONS";
      case -44: return "CL_INVALID_PROGRAM";
      case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
      case -46: return "CL_INVALID_KERNEL_NAME";
      case -47: return "CL_INVALID_KERNEL_DEFINITION";
      case -48: return "CL_INVALID_KERNEL";
      case -49: return "CL_INVALID_ARG_INDEX";
      case -50: return "CL_INVALID_ARG_VALUE";
      case -51: return "CL_INVALID_ARG_SIZE";
      case -52: return "CL_INVALID_KERNEL_ARGS";
      case -53: return "CL_INVALID_WORK_DIMENSION";
      case -54: return "CL_INVALID_WORK_GROUP_SIZE";
      case -55: return "CL_INVALID_WORK_ITEM_SIZE";
      case -56: return "CL_INVALID_GLOBAL_OFFSET";
      case -57: return "CL_INVALID_EVENT_WAIT_LIST";
      case -58: return "CL_INVALID_EVENT";
      case -59: return "CL_INVALID_OPERATION";
      case -60: return "CL_INVALID_GL_OBJECT";
      case -61: return "CL_INVALID_BUFFER_SIZE";
      case -62: return "CL_INVALID_MIP_LEVEL";
      case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
      case -64: return "CL_INVALID_PROPERTY";
      case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
      case -66: return "CL_INVALID_COMPILER_OPTIONS";
      case -67: return "CL_INVALID_LINKER_OPTIONS";
      case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

      // extension errors
      case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
      case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
      case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
      case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
      case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
      case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
      case -9999: return "NVIDIA: ILLEGAL READ OR WRITE TO A BUFFER";
      default:
          fprintf(stderr, "'%d'\n", error);
          return "Unknown OpenCL error";
      }
  }
};
