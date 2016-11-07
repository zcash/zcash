// Copyright (c) 2016 Jack Grigg <jack@z.cash>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ZcashStratum.h"

#include "chainparams.h"
#include "clientversion.h"
#include "crypto/equihash.h"
#include "metrics.h"
#include "pow/tromp/equi_miner.h"
#include "streams.h"
#include "version.h"

#include <atomic>


void static ZcashMinerThread(ZcashMiner* miner, int size, int pos)
{
    LogPrintf("ZcashMinerThread started\n");
    RenameThread("zcash-miner");

    unsigned int n = Params().EquihashN();
    unsigned int k = Params().EquihashK();

    std::string solver = GetArg("-equihashsolver", "default");
    assert(solver == "tromp" || solver == "default");
    LogPrint("pow", "Using Equihash solver \"%s\" with n = %u, k = %u\n", solver, n, k);

    std::shared_ptr<std::mutex> m_zmt(new std::mutex);
    CBlockHeader header;
    arith_uint256 space;
    size_t offset;
    arith_uint256 inc;
    arith_uint256 target;
    std::atomic_bool workReady {false};
    std::atomic_bool cancelSolver {false};

    miner->NewJob.connect(NewJob_t::slot_type(
        [&m_zmt, &header, &space, &offset, &inc, &target, &workReady, &cancelSolver]
        (const ZcashJob* job) mutable {
            std::lock_guard<std::mutex> lock{*m_zmt.get()};
            if (job) {
                header = job->header;
                space = job->nonce2Space;
                offset = job->nonce1Size * 4; // Hex length to bit length
                inc = job->nonce2Inc;
                target = job->serverTarget;
                workReady.store(true);
                if (job->clean) {
                    cancelSolver.store(true);
                }
            } else {
                workReady.store(false);
                cancelSolver.store(true);
            }
        }
    ).track_foreign(m_zmt)); // So the signal disconnects when the mining thread exits

    try {
        while (true) {
            // Wait for work
            bool expected;
            do {
                expected = true;
                boost::this_thread::interruption_point();
                MilliSleep(1000);
            } while (!workReady.compare_exchange_weak(expected, false));
            // TODO change atomically with workReady
            cancelSolver.store(false);

            // Calculate nonce limits
            arith_uint256 nonce;
            arith_uint256 nonceEnd;
            {
                std::lock_guard<std::mutex> lock{*m_zmt.get()};
                arith_uint256 baseNonce = UintToArith256(header.nNonce);
                nonce = baseNonce + ((space/size)*pos << offset);
                nonceEnd = baseNonce + ((space/size)*(pos+1) << offset);
            }

            // Hash state
            crypto_generichash_blake2b_state state;
            EhInitialiseState(n, k, state);

            // I = the block header minus nonce and solution.
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            {
                std::lock_guard<std::mutex> lock{*m_zmt.get()};
                CEquihashInput I{header};
                ss << I;
            }

            // H(I||...
            crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

            // Start working
            while (true) {
                // H(I||V||...
                crypto_generichash_blake2b_state curr_state;
                curr_state = state;
                auto bNonce = ArithToUint256(nonce);
                crypto_generichash_blake2b_update(&curr_state,
                                                  bNonce.begin(),
                                                  bNonce.size());

                // (x_1, x_2, ...) = A(I, V, n, k)
                LogPrint("pow", "Running Equihash solver with nNonce = %s\n",
                         nonce.ToString());

                std::function<bool(std::vector<unsigned char>)> validBlock =
                        [&m_zmt, &header, &bNonce, &target, &miner]
                        (std::vector<unsigned char> soln) {
                    std::lock_guard<std::mutex> lock{*m_zmt.get()};
                    // Write the solution to the hash and compute the result.
                    LogPrint("pow", "- Checking solution against target...");
                    header.nNonce = bNonce;
                    header.nSolution = soln;
                    solutionTargetChecks.increment();

                    if (UintToArith256(header.GetHash()) > target) {
                        LogPrint("pow", " too large.\n");
                        return false;
                    }

                    // Found a solution
                    LogPrintf("Found solution satisfying the server target\n");
                    EquihashSolution solution {bNonce, soln};
                    miner->submitSolution(solution);

                    // We're a pooled miner, so try all solutions
                    return false;
                };
                std::function<bool(EhSolverCancelCheck)> cancelled =
                        [&cancelSolver](EhSolverCancelCheck pos) {
                    boost::this_thread::interruption_point();
                    return cancelSolver.load();
                };

                // TODO: factor this out into a function with the same API for each solver.
                if (solver == "tromp") {
                    // Create solver and initialize it.
                    equi eq(1);
                    eq.setstate(&curr_state);

                    // Intialization done, start algo driver.
                    eq.digit0(0);
                    eq.xfull = eq.bfull = eq.hfull = 0;
                    eq.showbsizes(0);
                    for (u32 r = 1; r < WK; r++) {
                        (r&1) ? eq.digitodd(r, 0) : eq.digiteven(r, 0);
                        eq.xfull = eq.bfull = eq.hfull = 0;
                        eq.showbsizes(r);
                    }
                    eq.digitK(0);
                    ehSolverRuns.increment();

                    // Convert solution indices to byte array (decompress) and pass it to validBlock method.
                    for (size_t s = 0; s < eq.nsols; s++) {
                        LogPrint("pow", "Checking solution %d\n", s+1);
                        std::vector<eh_index> index_vector(PROOFSIZE);
                        for (size_t i = 0; i < PROOFSIZE; i++) {
                            index_vector[i] = eq.sols[s][i];
                        }
                        std::vector<unsigned char> sol_char = GetMinimalFromIndices(index_vector, DIGITBITS);

                        if (validBlock(sol_char)) {
                            // If we find a POW solution, do not try other solutions
                            // because they become invalid as we created a new block in blockchain.
                            break;
                        }
                    }
                } else {
                    try {
                        // If we find a valid block, we get more work
                        bool found = EhOptimisedSolve(n, k, curr_state, validBlock, cancelled);
                        ehSolverRuns.increment();
                        if (found) {
                            break;
                        }
                    } catch (EhSolverCancelledException&) {
                        LogPrint("pow", "Equihash solver cancelled\n");
                        cancelSolver.store(false);
                        break;
                    }
                }

                // Check for stop
                boost::this_thread::interruption_point();
                if (nonce == nonceEnd) {
                    break;
                }

                // Check for new work
                if (workReady.load()) {
                    LogPrint("pow", "New work received, dropping current work\n");
                    break;
                }

                // Update nonce
                nonce += inc;
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("ZcashMinerThread terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("ZcashMinerThread runtime error: %s\n", e.what());
        return;
    }
}

ZcashJob* ZcashJob::clone() const
{
    ZcashJob* ret = new ZcashJob();
    ret->job = job;
    ret->header = header;
    ret->time = time;
    ret->nonce1Size = nonce1Size;
    ret->nonce2Space = nonce2Space;
    ret->nonce2Inc = nonce2Inc;
    ret->serverTarget = serverTarget;
    ret->clean = clean;
    return ret;
}

void ZcashJob::setTarget(std::string target)
{
    if (target.size() > 0) {
        serverTarget = UintToArith256(uint256S(target));
    } else {
        LogPrint("stratum", "New job but no server target, assuming powLimit\n");
        serverTarget = UintToArith256(Params().GetConsensus().powLimit);
    }
}

bool ZcashJob::evalSolution(const EquihashSolution* solution)
{
    unsigned int n = Params().EquihashN();
    unsigned int k = Params().EquihashK();

    // Hash state
    crypto_generichash_blake2b_state state;
    EhInitialiseState(n, k, state);

    // I = the block header minus nonce and solution.
    CEquihashInput I{header};
    // I||V
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;
    ss << solution->nonce;

    // H(I||V||...
    crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

    bool isValid;
    EhIsValidSolution(n, k, state, solution->solution, isValid);
    return isValid;
}

std::string ZcashJob::getSubmission(const EquihashSolution* solution)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << solution->nonce;
    ss << solution->solution;
    std::string strHex = HexStr(ss.begin(), ss.end());

    std::stringstream stream;
    stream << "\"" << job;
    stream << "\",\"" << time;
    stream << "\",\"" << strHex.substr(nonce1Size, 64-nonce1Size);
    stream << "\",\"" << strHex.substr(64);
    stream << "\"";
    return stream.str();
}

ZcashMiner::ZcashMiner(int threads)
    : nThreads{threads}, minerThreads{nullptr}
{
    if (nThreads < 0) {
        nThreads = boost::thread::hardware_concurrency();
    }
}

std::string ZcashMiner::userAgent()
{
    return FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<std::string>());
}

void ZcashMiner::start()
{
    if (minerThreads) {
        stop();
    }

    if (nThreads == 0) {
        return;
    }

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++) {
        minerThreads->create_thread(boost::bind(&ZcashMinerThread, this, nThreads, i));
    }
}

void ZcashMiner::stop()
{
    if (minerThreads) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = nullptr;
    }
}

void ZcashMiner::setServerNonce(const Array& params)
{
    auto n1str = params[1].get_str();
    std::vector<unsigned char> nonceData(ParseHex(n1str));
    while (nonceData.size() < 32) {
        nonceData.push_back(0);
    }
    CDataStream ss(nonceData, SER_NETWORK, PROTOCOL_VERSION);
    ss >> nonce1;

    nonce1Size = n1str.size();
    size_t nonce1Bits = nonce1Size * 4; // Hex length to bit length
    size_t nonce2Bits = 256 - nonce1Bits;

    nonce2Space = 1;
    nonce2Space <<= nonce2Bits;
    nonce2Space -= 1;

    nonce2Inc = 1;
    nonce2Inc <<= nonce1Bits;
}

ZcashJob* ZcashMiner::parseJob(const Array& params)
{
    if (params.size() < 2) {
        throw std::logic_error("Invalid job params");
    }

    ZcashJob* ret = new ZcashJob();
    ret->job = params[0].get_str();

    int32_t version;
    sscanf(params[1].get_str().c_str(), "%x", &version);
    // TODO: On a LE host shouldn't this be le32toh?
    ret->header.nVersion = be32toh(version);

    if (ret->header.nVersion == 4) {
        if (params.size() < 8) {
            throw std::logic_error("Invalid job params");
        }

        std::stringstream ssHeader;
        ssHeader << params[1].get_str()
                 << params[2].get_str()
                 << params[3].get_str()
                 << params[4].get_str()
                 << params[5].get_str()
                 << params[6].get_str()
                    // Empty nonce
                 << "0000000000000000000000000000000000000000000000000000000000000000"
                 << "00"; // Empty solution
        auto strHexHeader = ssHeader.str();
        std::vector<unsigned char> headerData(ParseHex(strHexHeader));
        CDataStream ss(headerData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ss >> ret->header;
        } catch (const std::ios_base::failure& _e) {
            throw std::logic_error("ZcashMiner::parseJob(): Invalid block header parameters");
        }

        ret->time = params[5].get_str();
        ret->clean = params[7].get_bool();
    } else {
        throw std::logic_error("ZcashMiner::parseJob(): Invalid or unsupported block header version");
    }

    ret->header.nNonce = nonce1;
    ret->nonce1Size = nonce1Size;
    ret->nonce2Space = nonce2Space;
    ret->nonce2Inc = nonce2Inc;

    return ret;
}

void ZcashMiner::setJob(ZcashJob* job)
{
    NewJob(job);
}

void ZcashMiner::onSolutionFound(
        const std::function<bool(const EquihashSolution&)> callback)
{
    solutionFoundCallback = callback;
}

void ZcashMiner::submitSolution(const EquihashSolution& solution)
{
    solutionFoundCallback(solution);
}

void ZcashMiner::acceptedSolution(bool stale)
{
    acceptedSolutions.increment();
}

void ZcashMiner::rejectedSolution(bool stale)
{
    rejectedSolutions.increment();
}

void ZcashMiner::failedSolution()
{
    failedSolutions.increment();
}
