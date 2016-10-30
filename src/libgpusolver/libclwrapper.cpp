
#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstdlib>
//#include <chrono>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <queue>
#include <vector>
#include <random>
//#include <atomic>
#include "libclwrapper.h"
#include "kernels/silentarmy.h" // Created from CMake

// workaround lame platforms
#if !CL_VERSION_1_2
#define CL_MAP_WRITE_INVALIDATE_REGION CL_MAP_WRITE
#define CL_MEM_HOST_READ_ONLY 0
#endif

#undef min
#undef max

//#define DEBUG

using namespace std;

unsigned const cl_gpuminer::c_defaultLocalWorkSize = 32;
unsigned const cl_gpuminer::c_defaultGlobalWorkSizeMultiplier = 4096; // * CL_DEFAULT_LOCAL_WORK_SIZE
unsigned const cl_gpuminer::c_defaultMSPerBatch = 0;
bool cl_gpuminer::s_allowCPU = false;
unsigned cl_gpuminer::s_extraRequiredGPUMem;
unsigned cl_gpuminer::s_msPerBatch = cl_gpuminer::c_defaultMSPerBatch;
unsigned cl_gpuminer::s_workgroupSize = cl_gpuminer::c_defaultLocalWorkSize;
unsigned cl_gpuminer::s_initialGlobalWorkSize = cl_gpuminer::c_defaultGlobalWorkSizeMultiplier * cl_gpuminer::c_defaultLocalWorkSize;

#if defined(_WIN32)
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
static std::atomic_flag s_logSpin = ATOMIC_FLAG_INIT;
#define CL_LOG(_contents) \
	do \
	{ \
		std::stringstream ss; \
		ss << _contents; \
		while (s_logSpin.test_and_set(std::memory_order_acquire)) {} \
		OutputDebugStringA(ss.str().c_str()); \
		cerr << ss.str() << endl << flush; \
		s_logSpin.clear(std::memory_order_release); \
	} while (false)
#else
#define CL_LOG(_contents) cout << "[OPENCL]:" << _contents << endl
#endif

// Types of OpenCL devices we are interested in
#define CL_QUERIED_DEVICE_TYPES (CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR)

// Inject definitions into the kernal source
static void addDefinition(string& _source, char const* _id, unsigned _value)
{
	char buf[256];
	sprintf(buf, "#define %s %uu\n", _id, _value);
	_source.insert(_source.begin(), buf, buf + strlen(buf));
}


cl_gpuminer::cl_gpuminer()
:	m_openclOnePointOne()
{

	dst_solutions = (uint32_t *) malloc(10*NUM_INDICES*sizeof(uint32_t));
	if(dst_solutions == NULL)
		std::cout << "Error allocating dst_solutions array!" << std::endl;

}

cl_gpuminer::~cl_gpuminer()
{
	if(dst_solutions != NULL)
		free(dst_solutions);
	finish();
}

std::vector<cl::Platform> cl_gpuminer::getPlatforms()
{
	vector<cl::Platform> platforms;
	try
	{
		cl::Platform::get(&platforms);
	}
	catch(cl::Error const& err)
	{
#if defined(CL_PLATFORM_NOT_FOUND_KHR)
		if (err.err() == CL_PLATFORM_NOT_FOUND_KHR)
			CL_LOG("No OpenCL platforms found");
		else
#endif
			throw err;
	}
	return platforms;
}

string cl_gpuminer::platform_info(unsigned _platformId, unsigned _deviceId)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return {};
	// get GPU device of the selected platform
	unsigned platform_num = min<unsigned>(_platformId, platforms.size() - 1);
	vector<cl::Device> devices = getDevices(platforms, _platformId);
	if (devices.empty())
	{
		CL_LOG("No OpenCL devices found.");
		return {};
	}

	// use selected default device
	unsigned device_num = min<unsigned>(_deviceId, devices.size() - 1);
	cl::Device& device = devices[device_num];
	string device_version = device.getInfo<CL_DEVICE_VERSION>();

	return "{ \"platform\": \"" + platforms[platform_num].getInfo<CL_PLATFORM_NAME>() + "\", \"device\": \"" + device.getInfo<CL_DEVICE_NAME>() + "\", \"version\": \"" + device_version + "\" }";
}

std::vector<cl::Device> cl_gpuminer::getDevices(std::vector<cl::Platform> const& _platforms, unsigned _platformId)
{
	vector<cl::Device> devices;
	unsigned platform_num = min<unsigned>(_platformId, _platforms.size() - 1);
	try
	{
		_platforms[platform_num].getDevices(
			s_allowCPU ? CL_DEVICE_TYPE_ALL : CL_QUERIED_DEVICE_TYPES,
			&devices
		);
	}
	catch (cl::Error const& err)
	{
		// if simply no devices found return empty vector
		if (err.err() != CL_DEVICE_NOT_FOUND)
			throw err;
	}
	return devices;
}

unsigned cl_gpuminer::getNumPlatforms()
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return 0;
	return platforms.size();
}

unsigned cl_gpuminer::getNumDevices(unsigned _platformId)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return 0;

	vector<cl::Device> devices = getDevices(platforms, _platformId);
	if (devices.empty())
	{
		CL_LOG("No OpenCL devices found.");
		return 0;
	}
	return devices.size();
}

// This needs customizing apon completion of the kernel - Checks memory requirements - May not be applicable
bool cl_gpuminer::configureGPU(
	unsigned _platformId,
	unsigned _localWorkSize,
	unsigned _globalWorkSize
)
{
	// Set the local/global work sizes
	s_workgroupSize = _localWorkSize;
	s_initialGlobalWorkSize = _globalWorkSize;

	return searchForAllDevices(_platformId, [](cl::Device const& _device) -> bool
		{
			cl_ulong result;
			_device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &result);

				CL_LOG(
					"Found suitable OpenCL device [" << _device.getInfo<CL_DEVICE_NAME>()
					<< "] with " << result << " bytes of GPU memory"
				);
				return true;
		}
	);
}

bool cl_gpuminer::searchForAllDevices(function<bool(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return false;
	for (unsigned i = 0; i < platforms.size(); ++i)
		if (searchForAllDevices(i, _callback))
			return true;

	return false;
}

bool cl_gpuminer::searchForAllDevices(unsigned _platformId, function<bool(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return false;
	if (_platformId >= platforms.size())
		return false;

	vector<cl::Device> devices = getDevices(platforms, _platformId);
	for (cl::Device const& device: devices)
		if (_callback(device))
			return true;

	return false;
}

void cl_gpuminer::doForAllDevices(function<void(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return;
	for (unsigned i = 0; i < platforms.size(); ++i)
		doForAllDevices(i, _callback);
}

void cl_gpuminer::doForAllDevices(unsigned _platformId, function<void(cl::Device const&)> _callback)
{
	vector<cl::Platform> platforms = getPlatforms();
	if (platforms.empty())
		return;
	if (_platformId >= platforms.size())
		return;

	vector<cl::Device> devices = getDevices(platforms, _platformId);
	for (cl::Device const& device: devices)
		_callback(device);
}

void cl_gpuminer::listDevices()
{
	string outString ="\nListing OpenCL devices.\nFORMAT: [deviceID] deviceName\n";
	unsigned int i = 0;
	doForAllDevices([&outString, &i](cl::Device const _device)
		{
			outString += "[" + to_string(i) + "] " + _device.getInfo<CL_DEVICE_NAME>() + "\n";
			outString += "\tCL_DEVICE_TYPE: ";
			switch (_device.getInfo<CL_DEVICE_TYPE>())
			{
			case CL_DEVICE_TYPE_CPU:
				outString += "CPU\n";
				break;
			case CL_DEVICE_TYPE_GPU:
				outString += "GPU\n";
				break;
			case CL_DEVICE_TYPE_ACCELERATOR:
				outString += "ACCELERATOR\n";
				break;
			default:
				outString += "DEFAULT\n";
				break;
			}
			outString += "\tCL_DEVICE_GLOBAL_MEM_SIZE: " + to_string(_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()) + "\n";
			outString += "\tCL_DEVICE_MAX_MEM_ALLOC_SIZE: " + to_string(_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()) + "\n";
			outString += "\tCL_DEVICE_MAX_WORK_GROUP_SIZE: " + to_string(_device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>()) + "\n";
			++i;
		}
	);
	CL_LOG(outString);
}

void cl_gpuminer::finish()
{

	if (m_queue())
		m_queue.finish();
}

// Customise given kernel - This builds the kernel and creates memory buffers
bool cl_gpuminer::init(
	unsigned _platformId,
	unsigned _deviceId,
	const std::vector<std::string> _kernels
)
{
	// get all platforms
	try
	{
		vector<cl::Platform> platforms = getPlatforms();
		if (platforms.empty())
			return false;

		// use selected platform
		_platformId = min<unsigned>(_platformId, platforms.size() - 1);
		CL_LOG("Using platform: " << platforms[_platformId].getInfo<CL_PLATFORM_NAME>().c_str());

		// get GPU device of the default platform
		vector<cl::Device> devices = getDevices(platforms, _platformId);
		if (devices.empty())
		{
			CL_LOG("No OpenCL devices found.");
			return false;
		}

		// use selected device
		cl::Device& device = devices[min<unsigned>(_deviceId, devices.size() - 1)];
		string device_version = device.getInfo<CL_DEVICE_VERSION>();
		CL_LOG("Using device: " << device.getInfo<CL_DEVICE_NAME>().c_str() << "(" << device_version.c_str() << ")");

		if (strncmp("OpenCL 1.0", device_version.c_str(), 10) == 0)
		{
			CL_LOG("OpenCL 1.0 is not supported.");
			return false;
		}
		if (strncmp("OpenCL 1.1", device_version.c_str(), 10) == 0)
			m_openclOnePointOne = true;

		// create context
		m_context = cl::Context(vector<cl::Device>(&device, &device + 1));
		m_queue = cl::CommandQueue(m_context, device);

		// make sure that global work size is evenly divisible by the local workgroup size
		m_globalWorkSize = s_initialGlobalWorkSize;
		if (m_globalWorkSize % s_workgroupSize != 0)
			m_globalWorkSize = ((m_globalWorkSize / s_workgroupSize) + 1) * s_workgroupSize;
		// remember the device's address bits
		m_deviceBits = device.getInfo<CL_DEVICE_ADDRESS_BITS>();
		// make sure first step of global work size adjustment is large enough
		m_stepWorkSizeAdjust = pow(2, m_deviceBits / 2 + 1);

		// patch source code
		// note: CL_MINER_KERNEL is simply cl_gpuminer_kernel.cl compiled
		// into a byte array by bin2h.cmake. There is no need to load the file by hand in runtime

		// Uncomment for loading kernel from compiled cl file.
#ifdef DEBUG
		ifstream kernel_file("./libgpuminer/kernels/silentarmy.cl");
		string code((istreambuf_iterator<char>(kernel_file)), istreambuf_iterator<char>());
		kernel_file.close();
#else
		string code(CL_MINER_KERNEL, CL_MINER_KERNEL + CL_MINER_KERNEL_SIZE);
#endif
		// create miner OpenCL program
		cl::Program::Sources sources;
		sources.push_back({ code.c_str(), code.size() });

		cl::Program program(m_context, sources);
		try
		{
			program.build({ device });
			CL_LOG("Printing program log");
			CL_LOG(program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device).c_str());
		}
		catch (cl::Error const&)
		{
			CL_LOG(program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device).c_str());
			return false;
		}

		try
		{
			for (auto & _kernel : _kernels)
				m_gpuKernels.push_back(cl::Kernel(program, _kernel.c_str()));
		}
		catch (cl::Error const& err)
		{
			CL_LOG("gpuKERNEL Creation failed: " << err.what() << "(" << err.err() << "). Bailing.");
			return false;
		}

		// TODO create buffer kernel inputs (private variables)
	  	buf_dbg = cl::Buffer(m_context, CL_MEM_READ_WRITE, dbg_size, NULL, NULL);
		//TODO Dangger
		m_queue.enqueueFillBuffer(buf_dbg, &zero, 1, 0, dbg_size, 0);
		buf_ht[0] = cl::Buffer(m_context, CL_MEM_READ_WRITE, HT_SIZE, NULL, NULL);
		buf_ht[1] = cl::Buffer(m_context, CL_MEM_READ_WRITE, HT_SIZE, NULL, NULL);
		buf_sols = cl::Buffer(m_context, CL_MEM_READ_WRITE, sizeof (sols_t), NULL, NULL);

		m_queue.finish();

	}
	catch (cl::Error const& err)
	{
		CL_LOG("CL ERROR:" << get_error_string(err.err()));
		return false;
	}
	return true;
}


void cl_gpuminer::run(uint8_t *header, size_t header_len, uint64_t nonce, sols_t * indices, uint32_t * n_sol, uint64_t * ptr)
{
	try
	{

		blake2b_state_t     blake;
    cl::Buffer          buf_blake_st;
		uint32_t		sol_found = 0;
		size_t      local_ws = 64;
		size_t		global_ws;
		uint64_t		*nonce_ptr;
  	assert(header_len == ZCASH_BLOCK_HEADER_LEN ||
    header_len == ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN);
  	nonce_ptr = (uint64_t *)(header + ZCASH_BLOCK_HEADER_LEN - ZCASH_NONCE_LEN);
		//memset(nonce_ptr, 0, ZCASH_NONCE_LEN);
		//*nonce_ptr = nonce;
		*ptr = *nonce_ptr;

		//printf("\nSolving nonce %s\n", s_hexdump(nonce_ptr, ZCASH_NONCE_LEN));

		zcash_blake2b_init(&blake, ZCASH_HASH_LEN, PARAM_N, PARAM_K);
		zcash_blake2b_update(&blake, header, 128, 0);
		buf_blake_st = cl::Buffer(m_context, CL_MEM_READ_ONLY, sizeof (blake.h), NULL, NULL);
		m_queue.enqueueWriteBuffer(buf_blake_st, true, 0, sizeof(blake.h), blake.h);

		m_queue.finish();

		for (unsigned round = 0; round < PARAM_K; round++) {

			size_t      global_ws = NR_ROWS;

			m_gpuKernels[0].setArg(0, buf_ht[round % 2]);
			m_queue.enqueueNDRangeKernel(m_gpuKernels[0], cl::NullRange, cl::NDRange(global_ws), cl::NDRange(local_ws));

			if (!round) {
				m_gpuKernels[1+round].setArg(0, buf_blake_st);
				m_gpuKernels[1+round].setArg(1, buf_ht[round % 2]);
				global_ws = select_work_size_blake();
			} else {
				m_gpuKernels[1+round].setArg(0, buf_ht[(round - 1) % 2]);
				m_gpuKernels[1+round].setArg(1, buf_ht[round % 2]);
				global_ws = NR_ROWS;
			}

			m_gpuKernels[1+round].setArg(2, buf_dbg);

			m_queue.enqueueNDRangeKernel(m_gpuKernels[1+round], cl::NullRange, cl::NDRange(global_ws), cl::NDRange(local_ws));

		}

		m_gpuKernels[10].setArg(0, buf_ht[0]);
		m_gpuKernels[10].setArg(1, buf_ht[1]);
		m_gpuKernels[10].setArg(2, buf_sols);
		global_ws = NR_ROWS;
		m_queue.enqueueNDRangeKernel(m_gpuKernels[10], cl::NullRange, cl::NDRange(global_ws), cl::NDRange(local_ws));

		sols_t	* sols;
		sols = (sols_t *)malloc(sizeof(*sols));

		m_queue.enqueueReadBuffer(buf_sols, true, 0, sizeof (*sols), sols);

		m_queue.finish();

		if (sols->nr > MAX_SOLS) {
			/*fprintf(stderr, "%d (probably invalid) solutions were dropped!\n",
			sols->nr - MAX_SOLS);*/
			sols->nr = MAX_SOLS;
		}

		for (unsigned sol_i = 0; sol_i < sols->nr; sol_i++)
			sol_found += verify_sol(sols, sol_i);

		//print_sols(sols, nonce, nr_valid_sols);

		//printf("\nSolutions: %u\n", sol_found);

		*n_sol = sol_found;
		memcpy(indices, sols, sizeof(sols_t));

		free(sols);

	}
	catch (cl::Error const& err)
	{
		CL_LOG("CL ERROR:" << get_error_string(err.err()));
		//CL_LOG(err.what() << "(" << err.err() << ")");
	}
}
