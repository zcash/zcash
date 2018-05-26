// Copyright (c) 2018 The Verus developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string>
#include <cstring>
#include "veruslaunch.h"


const char *whitelist_ids[WHITELIST_COUNT] = {"1e4b6bf2306c4eb2e6be032176a749797facd387937b132c0d2ffd6dc8ec0c7b",
                                 "21681d1cacbdc8ba403bc66b129cce4ca72a61adf0ef12504b4fbb07a613f735",
                                 "2dc18aac1a8f97b991d1a2b67fa606f29c27b721e939c7b08a52da789900627c",
                                 "41beb053cf1e516033316a69ab883584cd42546d1d7613181cf3d948eeaf1e6a",
                                 "45fae92ebc1cdaf9187b92f0feb5e262b3dcadc37f3cde48a9b9da88f01b9195",
                                 "5e7b24d571d7b21c1a2077ec8dc1b479d16c490d51ef11cc7c81af5dcb36b56f",
                                 "6e1338aaf678121296d7a0ba26e2feead3f609cc928add8bd4798bb1af6ef70a",
                                 "75150d61723081dd13259a88e75968e7b3ac2f8ae7293fc73624d9f188dcc091",
                                 "b58418bdf076bf44d42552b426a5ab79cdabd08212a59d44779933df53b77ef3",
                                 "c1d9c633ec3be4a06a27b84aa31a9571841f8583a23fd100e90c2e7b80b7641e",
                                 "d73d18497cc888713655ee65b3525a502b9758196b0d6d3f81509f1c78f3d347",
                                 "e8a56cfa01f4dc10160f22c8a843875b3b6e31d323db4293967363ed10258970",
                                 "fe9fad13f69365496962dc8e02d2aa58c5ec605a2156ab40f24f6b9ea82d58b5"};

const char *whitelist_addrs[WHITELIST_COUNT] = {"RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9",
                                   "RNK1PPPFzzT4rBFE2CtKHjJNa7qkPRE3U9"};
