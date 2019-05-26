
#ifndef NOTARIES_STAKED
#define NOTARIES_STAKED

#include "crosschain.h"
#include "cc/CCinclude.h"

static const int32_t iguanaPort = 9333;
static const int8_t BTCminsigs = 13;
static const int8_t overrideMinSigs = 7;
static const char *iguanaSeeds[8][1] =
{
  {"94.23.1.95"},
  {"103.6.12.112"},
  {"18.224.176.46"},
  {"45.76.120.247"},
  {"185.62.57.32"},
  {"149.28.253.160"},
  {"68.183.226.124"},
  {"149.28.246.230"},
};

static const int STAKED_ERA_GAP = 777;

static const int NUM_STAKED_ERAS = 4;
static const int STAKED_NOTARIES_TIMESTAMP[NUM_STAKED_ERAS] = {1604244444, 1604244444, 1604244444, 1604244444};
static const int32_t num_notaries_STAKED[NUM_STAKED_ERAS] = { 22, 1, 1, 1 };

// Era array of pubkeys.
static const char *notaries_STAKED[NUM_STAKED_ERAS][64][2] =
{
    {
        {"blackjok3r", "035fc678bf796ad52f69e1f5759be54ec671c559a22adf27eed067e0ddf1531574" }, // RTcYRJ6WopYkUqcmksyjxoV1CueYyqxFuk    right
        {"Alright", "02b718c60a035f77b7103a507d36aed942b4f655b8d13bce6f28b8eac523944278" }, //RG77F4mQpP1K1q2CDSc2vZSJvKUZgF8R26
        {"webworker01", "031d1fb39ae4dca28965c3abdbd21faa0f685f6d7b87a60561afa7c448343fef6d" }, //RGsQiArk5sTmjXZV9UzGMW5njyvtSnsTN8    right
        {"CrisF", "03745656c8991c4597828aad2820760c43c00ff2e3b381fef3b5c040f32a7b3a34" }, // RNhYJAaPHJCVXGWNVEJeP3TfepEPdhjrRr         right
        {"smk762", "02381616fbc02d3f0398c912fe7b7daf2f3f29e55dc35287f686b15686d8135a9f" }, // RSchwBApVquaG6mXH31bQ6P83kMN4Hound        right
        {"jorian", "0343eec31037d7b909efd968a5b5e7af60321bf7e464da28f815f0fb23ee7aadd7" }, // RJorianBXNwfNDYPxtNYJJ6vX7Z3VdTR25        right
        {"TonyL", "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96" }, // RHq3JsvLxU45Z8ufYS6RsDpSG4wi6ucDev
        {"CHMEX", "03ed125d1beb118d12ff0a052bdb0cee32591386d718309b2924f2c36b4e7388e6" }, // RF4HiVeuYpaznRPs7fkRAKKYqT5tuxQQTL         right 
        {"metaphilibert", "0344182c376f054e3755d712361672138660bda8005abb64067eb5aa98bdb40d10" }, // RG28QSnYFADBg1dAVkH1uPGYS6F8ioEUM2 right
        {"gt", "02312dcecb6e4a32927a075972d3c009f3c68635d8100562cc1813ea66751b9fde" }, // RCg4tzKWQ7i3wrZEU8bvCbCQ4xRJnHnyoo            right
        {"CMaurice", "026c6d094523e810641b89f2d7f0ddd8f0b59d97c32e1fa97f0e3e0ac119c26ae4" }, // RSjayeSuYUE1E22rBjnqoexobaRjbAZ2Yb      right
        {"Bar_F1sh_Rel", "0395f2d9dd9ccb78caf74bff49b6d959afb95af746462e1b35f4a167d8e82b3666" }, // RBbLxJagCA9QHDazQvfnDZe874V1K4Gu8t
        {"zatJUM", "030fff499b6dc0215344b28a0b6b4becdfb00cd34cd1b36b983ec14f47965fd4bc" }, // RSoEDLBasth7anxS8gbkg6KgeGiz8rhqv1        right
        {"dwy", "03669457b2934d98b5761121dd01b243aed336479625b293be9f8c43a6ae7aaeff" }, // RKhZMqRF361FSGFAzstP5AhozekPjoVh5q
        {"gcharang", "021569dd350d99e685a739c5b36bd01f217efb4f448a6f9a56da80c5edf6ce20ee" }, // RE8SsNwhYoygXJSvw9DuQbJicDc28dwR78      right
        {"computergenie", "027313dabde94fb72f823231d0a1c59fc7baa2e5b3bb2af97ca7d70aae116026b9" }, // RLabsCGxTRqJcJvz6foKuXAB61puJ2x8yt right
        {"daemonfox", "0383484bdc745b2b953c85b5a0c496a1f27bc42ae971f15779ed1532421b3dd943" }, //
        {"SHossain", "02791f5c215b8a19c143a98e3371ff03b5613df9ac430c4a331ca55fed5761c800" }, // RKdLoHkyeorXmMtj91B1AAnAGiwsdt9MdF
        {"Nabob", "03ee91c20b6d26e3604022f42df6bb8de6f669da4591f93b568778cba13d9e9ddf" }, // RRwCLPZDzpHEFJnLev4phy51e2stHRUAaU
        {"mylo", "03f6b7fcaf0b8b8ec432d0de839a76598b78418dadd50c8e5594c0e557d914ec09" }, // RXN4hoZkhUkkrnef9nTUDw3E3vVALAD8Kx
        {"PHBA2061", "039fc98c764bc85aed97d690d7942a4fd1190b2fa4f5f4c5c8e0957fac5c6ede00" }, // RPHba2o61hcpX4ds91oj3sKJ8aDXv6QdQf      right
        {"Exile13", "0247b2120a39faf83678b5de6883e039180ff42925bcb298d32f3792cd59001aae" }, // RTDJ3CDZ6ANbeDKab8nqTVrGw7ViAKLeDV       right
    },
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
    },
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
    },
    {
        {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" }, // RTVti13NP4eeeZaCCmQxc2bnPdHxCJFP9x
    }
};

int8_t is_STAKED(const char *chain_name);
int32_t STAKED_era(int timestamp);
int8_t numStakedNotaries(uint8_t pubkeys[64][33],int8_t era);
int8_t StakedNotaryID(std::string &notaryname, char *Raddress);
void UpdateNotaryAddrs(uint8_t pubkeys[64][33],int8_t numNotaries);

CrosschainAuthority Choose_auth_STAKED(int32_t chosen_era);

#endif
