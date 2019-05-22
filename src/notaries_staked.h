
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

#include "crosschain.h"
#include "cc/CCinclude.h"

static const int32_t iguanaPort = 9997;
static const int8_t BTCminsigs = 13;
static const int8_t overrideMinSigs = 6;
static const char *iguanaSeeds[8][1] =
{
  {"80.240.17.222"},
  {"103.6.12.112"},
  {"18.224.176.46"},
  {"45.76.120.247"},
  {"185.62.57.32"},
  {"103.6.12.112"},
  {"103.6.12.112"},
  {"103.6.12.112"},
};

static const int STAKED_ERA_GAP = 777;

static const int NUM_STAKED_ERAS = 4;
static const int STAKED_NOTARIES_TIMESTAMP[NUM_STAKED_ERAS] = {1604244444, 1604244444, 1604244444, 1604244444};
static const int32_t num_notaries_STAKED[NUM_STAKED_ERAS] = { 20, 25, 19, 17 };

// Era array of pubkeys.
static const char *notaries_STAKED[NUM_STAKED_ERAS][64][2] =
{
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
        {"alright", "0285657c689b903218c97f5f10fe1d10ace2ed6595112d9017f54fb42ea1c1dda8" }, //RXmXeQ8LfJK6Y1aTM97cRz9Gu5f6fmR3sg
        {"webworker01", "031d1fb39ae4dca28965c3abdbd21faa0f685f6d7b87a60561afa7c448343fef6d" }, //RGsQiArk5sTmjXZV9UzGMW5njyvtSnsTN8
        {"CrisF", "024d19acf0d5de212cdd50326cd143292545d366a71b2b9c6df9f2110de2dfa1f2" }, // RKtAD2kyRRMx4EiG1eeTNprF5h2nmGbzzu
        {"smk762", "029f6c1f38c4d6825acb3b4b5147f7992e943b617cdaa0f4f5f36187e239d52d5a" }, // RPy6Xj2LWrxNoEW9YyREDgBZDZZ5qURXBU
        {"jorian", "0288e682c1ac449f1b85c4acb2d0bcd216d5df34c15fd18b8a8dd5fa64b8ece8ef" }, // RR1yT5aB19VwFoUCGTW4q4pk4qmhHEEE4t
        {"TonyL", "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96" }, // RHq3JsvLxU45Z8ufYS6RsDpSG4wi6ucDev
        {"CHMEX", "03ed125d1beb118d12ff0a052bdb0cee32591386d718309b2924f2c36b4e7388e6" }, // RF4HiVeuYpaznRPs7fkRAKKYqT5tuxQQTL
        {"metaphilibert", "0344182c376f054e3755d712361672138660bda8005abb64067eb5aa98bdb40d10" }, // RG28QSnYFADBg1dAVkH1uPGYS6F8ioEUM2
        {"gt", "02312dcecb6e4a32927a075972d3c009f3c68635d8100562cc1813ea66751b9fde" }, // RCg4tzKWQ7i3wrZEU8bvCbCQ4xRJnHnyoo
        {"CMaurice", "025830ce81bd1301fb67d5872344efa7a9ff99ae85fe1234f18c085db9072b740f" }, // RX7pXUaV24xFn6DVKV8t3PrRF3gKw6TBjf
        {"Bar_F1sh_Rel", "0395f2d9dd9ccb78caf74bff49b6d959afb95af746462e1b35f4a167d8e82b3666" }, // RBbLxJagCA9QHDazQvfnDZe874V1K4Gu8t
        {"zatJUM", "030fff499b6dc0215344b28a0b6b4becdfb00cd34cd1b36b983ec14f47965fd4bc" }, // RSoEDLBasth7anxS8gbkg6KgeGiz8rhqv1
        {"dwy", "03669457b2934d98b5761121dd01b243aed336479625b293be9f8c43a6ae7aaeff" }, // RKhZMqRF361FSGFAzstP5AhozekPjoVh5q
        {"gcharang", "03336ca9db27cb6e882830e20dc525884e27dc94d557a5e68b972a5cbf9e8c62a8" }, // RJYiWn3FRCSSLf9Pe5RJcbrKQYosaMburP
        {"computergenie", "027313dabde94fb72f823231d0a1c59fc7baa2e5b3bb2af97ca7d70aae116026b9" }, // RLabsCGxTRqJcJvz6foKuXAB61puJ2x8yt
        {"daemonfox", "0383484bdc745b2b953c85b5a0c496a1f27bc42ae971f15779ed1532421b3dd943" }, //
        {"SHossain", "02791f5c215b8a19c143a98e3371ff03b5613df9ac430c4a331ca55fed5761c800" }, // RKdLoHkyeorXmMtj91B1AAnAGiwsdt9MdF
        {"Nabob", "03ee91c20b6d26e3604022f42df6bb8de6f669da4591f93b568778cba13d9e9ddf" }, // RRwCLPZDzpHEFJnLev4phy51e2stHRUAaU
        {"mylo", "03f6b7fcaf0b8b8ec432d0de839a76598b78418dadd50c8e5594c0e557d914ec09" }, // RXN4hoZkhUkkrnef9nTUDw3E3vVALAD8Kx
    },
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
        {"alright", "0285657c689b903218c97f5f10fe1d10ace2ed6595112d9017f54fb42ea1c1dda8" }, //RXmXeQ8LfJK6Y1aTM97cRz9Gu5f6fmR3sg
        {"webworker01", "031d1fb39ae4dca28965c3abdbd21faa0f685f6d7b87a60561afa7c448343fef6d" }, //RGsQiArk5sTmjXZV9UzGMW5njyvtSnsTN8
        {"CrisF", "024d19acf0d5de212cdd50326cd143292545d366a71b2b9c6df9f2110de2dfa1f2" }, // RKtAD2kyRRMx4EiG1eeTNprF5h2nmGbzzu
        {"smk762", "029f6c1f38c4d6825acb3b4b5147f7992e943b617cdaa0f4f5f36187e239d52d5a" }, // RPy6Xj2LWrxNoEW9YyREDgBZDZZ5qURXBU
        {"jorian", "0288e682c1ac449f1b85c4acb2d0bcd216d5df34c15fd18b8a8dd5fa64b8ece8ef" }, // RR1yT5aB19VwFoUCGTW4q4pk4qmhHEEE4t
        {"TonyL", "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96" }, // RHq3JsvLxU45Z8ufYS6RsDpSG4wi6ucDev
        {"Emman", "038f642dcdacbdf510b7869d74544dbc6792548d9d1f8d73a999dd9f45f513c935" }, //RN2KsQGW36Ah4NorJDxLJp2xiYJJEzk9Y6
        {"CHMEX", "03ed125d1beb118d12ff0a052bdb0cee32591386d718309b2924f2c36b4e7388e6" }, // RF4HiVeuYpaznRPs7fkRAKKYqT5tuxQQTL
        {"metaphilibert", "0344182c376f054e3755d712361672138660bda8005abb64067eb5aa98bdb40d10" }, // RG28QSnYFADBg1dAVkH1uPGYS6F8ioEUM2
        {"jusoaresf", "02dfb7ed72a23f6d07f0ea2f28192ee174733cc8412ec0f97b073007b78fab6346" }, // RBQGfE5Hxsjm1BPraTxbneRuNasPDuoLnu
        {"mylo", "03f6b7fcaf0b8b8ec432d0de839a76598b78418dadd50c8e5594c0e557d914ec09" }, // RXN4hoZkhUkkrnef9nTUDw3E3vVALAD8Kx
        {"greentea", "02054c14ae81838a063d22a75eaa3c961415f6825a57c8b8e4148d19dad64f128e" }, // REF7R76WpL1v7nSXjjiNHtRa2xYtq5qk1p
        {"CMaurice", "025830ce81bd1301fb67d5872344efa7a9ff99ae85fe1234f18c085db9072b740f" }, // RX7pXUaV24xFn6DVKV8t3PrRF3gKw6TBjf
        {"kmdkrazy", "02da444a2627d420f1f622fcdfb9bddb67d6d4241ad6b4d5054716ddbde8a25dfb" }, // RJPJBbHcm5mkAxhkkERHRfEE9Cvkr4Euoi
        {"Bar_F1sh_Rel", "0395f2d9dd9ccb78caf74bff49b6d959afb95af746462e1b35f4a167d8e82b3666" }, // RBbLxJagCA9QHDazQvfnDZe874V1K4Gu8t
        {"zatJUM", "030fff499b6dc0215344b28a0b6b4becdfb00cd34cd1b36b983ec14f47965fd4bc" }, // RSoEDLBasth7anxS8gbkg6KgeGiz8rhqv1
        {"dwy", "03669457b2934d98b5761121dd01b243aed336479625b293be9f8c43a6ae7aaeff" }, // RKhZMqRF361FSGFAzstP5AhozekPjoVh5q
        {"dukeleto", "03e4322510ee46d417b8382fe124f5a381a3cef6aef08f8a4e90c66a42a04b4015" }, // RB8vS1fkGuttoNYkA2B1ivNn8vhqbCEqbe
        {"gcharang", "03336ca9db27cb6e882830e20dc525884e27dc94d557a5e68b972a5cbf9e8c62a8" }, // RJYiWn3FRCSSLf9Pe5RJcbrKQYosaMburP
        {"ca333", "03a18a33313ccdbf3c9778776e33c423e073ff5833fa1de092ce9e921de52f22f6" }, // RX333A56jWdeW15MwZsaW3mHxGaDu2Yutp
        {"computergenie", "03448ce28fb21748e8b05bbe32d6b1e758b589ac1eb359e5d552f8868f2b75dc92" }, // RGeniexxkjnR34hg7ZnCf36kmfuJusf6rE
        {"daemonfox", "0383484bdc745b2b953c85b5a0c496a1f27bc42ae971f15779ed1532421b3dd943" }, //
        {"SHossain", "02791f5c215b8a19c143a98e3371ff03b5613df9ac430c4a331ca55fed5761c800" }, // RKdLoHkyeorXmMtj91B1AAnAGiwsdt9MdF
        {"Nabob", "03ee91c20b6d26e3604022f42df6bb8de6f669da4591f93b568778cba13d9e9ddf" }, // RRwCLPZDzpHEFJnLev4phy51e2stHRUAaU
    },
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
        {"alright", "0285657c689b903218c97f5f10fe1d10ace2ed6595112d9017f54fb42ea1c1dda8" }, //RXmXeQ8LfJK6Y1aTM97cRz9Gu5f6fmR3sg
        {"webworker01", "031d1fb39ae4dca28965c3abdbd21faa0f685f6d7b87a60561afa7c448343fef6d" }, //RGsQiArk5sTmjXZV9UzGMW5njyvtSnsTN8
        {"CrisF", "024d19acf0d5de212cdd50326cd143292545d366a71b2b9c6df9f2110de2dfa1f2" }, // RKtAD2kyRRMx4EiG1eeTNprF5h2nmGbzzu
        {"smk762", "029f6c1f38c4d6825acb3b4b5147f7992e943b617cdaa0f4f5f36187e239d52d5a" }, // RPy6Xj2LWrxNoEW9YyREDgBZDZZ5qURXBU
        {"jorian", "0288e682c1ac449f1b85c4acb2d0bcd216d5df34c15fd18b8a8dd5fa64b8ece8ef" }, // RR1yT5aB19VwFoUCGTW4q4pk4qmhHEEE4t
        {"TonyL", "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96" }, // RHq3JsvLxU45Z8ufYS6RsDpSG4wi6ucDev
        {"CHMEX", "03ed125d1beb118d12ff0a052bdb0cee32591386d718309b2924f2c36b4e7388e6" }, // RF4HiVeuYpaznRPs7fkRAKKYqT5tuxQQTL
        {"metaphilibert", "0344182c376f054e3755d712361672138660bda8005abb64067eb5aa98bdb40d10" }, // RG28QSnYFADBg1dAVkH1uPGYS6F8ioEUM2
        {"greentea", "02054c14ae81838a063d22a75eaa3c961415f6825a57c8b8e4148d19dad64f128e" }, // REF7R76WpL1v7nSXjjiNHtRa2xYtq5qk1p
        {"CMaurice", "025830ce81bd1301fb67d5872344efa7a9ff99ae85fe1234f18c085db9072b740f" }, // RX7pXUaV24xFn6DVKV8t3PrRF3gKw6TBjf
        {"Bar_F1sh_Rel", "0395f2d9dd9ccb78caf74bff49b6d959afb95af746462e1b35f4a167d8e82b3666" }, // RBbLxJagCA9QHDazQvfnDZe874V1K4Gu8t
        {"zatJUM", "030fff499b6dc0215344b28a0b6b4becdfb00cd34cd1b36b983ec14f47965fd4bc" }, // RSoEDLBasth7anxS8gbkg6KgeGiz8rhqv1
        {"dwy", "03669457b2934d98b5761121dd01b243aed336479625b293be9f8c43a6ae7aaeff" }, // RKhZMqRF361FSGFAzstP5AhozekPjoVh5q
        {"gcharang", "03336ca9db27cb6e882830e20dc525884e27dc94d557a5e68b972a5cbf9e8c62a8" }, // RJYiWn3FRCSSLf9Pe5RJcbrKQYosaMburP
        {"computergenie", "03448ce28fb21748e8b05bbe32d6b1e758b589ac1eb359e5d552f8868f2b75dc92" }, // RGeniexxkjnR34hg7ZnCf36kmfuJusf6rE
        {"daemonfox", "0383484bdc745b2b953c85b5a0c496a1f27bc42ae971f15779ed1532421b3dd943" }, //
        {"SHossain", "02791f5c215b8a19c143a98e3371ff03b5613df9ac430c4a331ca55fed5761c800" }, // RKdLoHkyeorXmMtj91B1AAnAGiwsdt9MdF
        {"Nabob", "03ee91c20b6d26e3604022f42df6bb8de6f669da4591f93b568778cba13d9e9ddf" }, // RRwCLPZDzpHEFJnLev4phy51e2stHRUAaU
    },
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
        {"alright", "0285657c689b903218c97f5f10fe1d10ace2ed6595112d9017f54fb42ea1c1dda8" }, //RXmXeQ8LfJK6Y1aTM97cRz9Gu5f6fmR3sg
        {"webworker01", "031d1fb39ae4dca28965c3abdbd21faa0f685f6d7b87a60561afa7c448343fef6d" }, //RGsQiArk5sTmjXZV9UzGMW5njyvtSnsTN8
        {"CrisF", "03f87f1bccb744d90fdbf7fad1515a98e9fc7feb1800e460d2e7565b88c3971bf3" }, //RMwEpnaVe3cesWbMqqKYPPkaLcDkooTDgZ
        {"smk762", "02eacef682d2f86e0103c18f4da46116e17196f3fb8f73ed931acb78e81d8e1aa5" }, // RQVvzJ8gepCDVjhqCAc5Tia1kTmt8KDPL9
        {"jorian", "02150c410a606b898bcab4f083e48e0f98a510e0d48d4db367d37f318d26ae72e3" }, // RFgzxZe2P4RWKx6E9QGPK3rx3TXeWxSqa8
        {"TonyL", "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96" }, // RHq3JsvLxU45Z8ufYS6RsDpSG4wi6ucDev
        {"Emman", "038f642dcdacbdf510b7869d74544dbc6792548d9d1f8d73a999dd9f45f513c935" }, //RN2KsQGW36Ah4NorJDxLJp2xiYJJEzk9Y6
        {"CHMEX", "03ed125d1beb118d12ff0a052bdb0cee32591386d718309b2924f2c36b4e7388e6" }, // RF4HiVeuYpaznRPs7fkRAKKYqT5tuxQQTL
        {"metaphilibert", "0344182c376f054e3755d712361672138660bda8005abb64067eb5aa98bdb40d10" }, // RG28QSnYFADBg1dAVkH1uPGYS6F8ioEUM2
        {"jusoaresf", "02dfb7ed72a23f6d07f0ea2f28192ee174733cc8412ec0f97b073007b78fab6346" }, // RBQGfE5Hxsjm1BPraTxbneRuNasPDuoLnu
        {"mylo", "03f6b7fcaf0b8b8ec432d0de839a76598b78418dadd50c8e5594c0e557d914ec09" }, // RXN4hoZkhUkkrnef9nTUDw3E3vVALAD8Kx
        {"blackjok3r2", "02f7597468703c1c5c8465dd6d43acaae697df9df30bed21494d193412a1ea193e" }, // RWHGbrLSP89fTzNVF9U9xiekDYJqcibTca
        {"blackjok3r3", "03c3e4c0206551dbf3a4b24d18e5d2737080541184211e3bfd2b1092177410b9c2" }, // RMMav2AVse5XHPvDfTzRpMbFhK3GqFmtSN
        {"kmdkrazy", "02f7597468703c1c5c8465dd6d43acaae697df9df30bed21494d193412a1ea193e" }, // RWHGbrLSP89fTzNVF9U9xiekDYJqcibTca
        {"alrighttest", "02e9dfe248f453b499315a90375e58a1c9ad79f5f3932ecb2205399a0f262d65fc" }, // RBevSstS8JtDXMEFNcJws4QTYN4PcE2VL5
        {"alrighttest1", "03527c7ecd6a8c5db6d685a64e6e18c1edb49e2f057a434f56c3f1253a26e9c6a2" }, // RBw2jNU3dnGk86ZLqPMadJwRwg3NU8eC6s
    }
};

int8_t is_STAKED(const char *chain_name);
int32_t STAKED_era(int timestamp);
int8_t numStakedNotaries(uint8_t pubkeys[64][33],int8_t era);
int8_t StakedNotaryID(std::string &notaryname, char *Raddress);
void UpdateNotaryAddrs(uint8_t pubkeys[64][33],int8_t numNotaries);

CrosschainAuthority Choose_auth_STAKED(int32_t chosen_era);

#endif
