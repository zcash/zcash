/* Based on the public domain implementation in
 * crypto_hash/keccakc512/simple/ from http://bench.cr.yp.to/supercop.html
 * by Ronny Van Keer
 * and the public domain "TweetFips202" implementation
 * from https://twitter.com/tweetfips202
 * by Gilles Van Assche, Daniel J. Bernstein, and Peter Schwabe */

#include <stdint.h>

#define DBENCH_START()
#define DBENCH_STOP(arg)

#include "dilithium.h"


#define NROUNDS 24
#define ROL(a, offset) ((a << offset) ^ (a >> (64-offset)))

/*************************************************
* Name:        load64
*
* Description: Load 8 bytes into uint64_t in little-endian order
*
* Arguments:   - const uint8_t *x: pointer to input byte array
*
* Returns the loaded 64-bit unsigned integer
**************************************************/
static uint64_t load64(const uint8_t *x) {
  uint32_t i;
  uint64_t r = 0;

  for (i = 0; i < 8; ++i)
    r |= (uint64_t)x[i] << 8*i;

  return r;
}

/*************************************************
* Name:        store64
*
* Description: Store a 64-bit integer to array of 8 bytes in little-endian order
*
* Arguments:   - uint8_t *x: pointer to the output byte array (allocated)
*              - uint64_t u: input 64-bit unsigned integer
**************************************************/
static void store64(uint8_t *x, uint64_t u) {
  uint32_t i;

  for(i = 0; i < 8; ++i)
    x[i] = u >> 8*i;
}

/* Keccak round constants */
static const uint64_t KeccakF_RoundConstants[NROUNDS] = {
  (uint64_t)0x0000000000000001ULL,
  (uint64_t)0x0000000000008082ULL,
  (uint64_t)0x800000000000808aULL,
  (uint64_t)0x8000000080008000ULL,
  (uint64_t)0x000000000000808bULL,
  (uint64_t)0x0000000080000001ULL,
  (uint64_t)0x8000000080008081ULL,
  (uint64_t)0x8000000000008009ULL,
  (uint64_t)0x000000000000008aULL,
  (uint64_t)0x0000000000000088ULL,
  (uint64_t)0x0000000080008009ULL,
  (uint64_t)0x000000008000000aULL,
  (uint64_t)0x000000008000808bULL,
  (uint64_t)0x800000000000008bULL,
  (uint64_t)0x8000000000008089ULL,
  (uint64_t)0x8000000000008003ULL,
  (uint64_t)0x8000000000008002ULL,
  (uint64_t)0x8000000000000080ULL,
  (uint64_t)0x000000000000800aULL,
  (uint64_t)0x800000008000000aULL,
  (uint64_t)0x8000000080008081ULL,
  (uint64_t)0x8000000000008080ULL,
  (uint64_t)0x0000000080000001ULL,
  (uint64_t)0x8000000080008008ULL
};

/*************************************************
* Name:        KeccakF1600_StatePermute
*
* Description: The Keccak F1600 Permutation
*
* Arguments:   - uint64_t *state: pointer to input/output Keccak state
**************************************************/
static void KeccakF1600_StatePermute(uint64_t *state)
{
        int round;

        uint64_t Aba, Abe, Abi, Abo, Abu;
        uint64_t Aga, Age, Agi, Ago, Agu;
        uint64_t Aka, Ake, Aki, Ako, Aku;
        uint64_t Ama, Ame, Ami, Amo, Amu;
        uint64_t Asa, Ase, Asi, Aso, Asu;
        uint64_t BCa, BCe, BCi, BCo, BCu;
        uint64_t Da, De, Di, Do, Du;
        uint64_t Eba, Ebe, Ebi, Ebo, Ebu;
        uint64_t Ega, Ege, Egi, Ego, Egu;
        uint64_t Eka, Eke, Eki, Eko, Eku;
        uint64_t Ema, Eme, Emi, Emo, Emu;
        uint64_t Esa, Ese, Esi, Eso, Esu;

        //copyFromState(A, state)
        Aba = state[ 0];
        Abe = state[ 1];
        Abi = state[ 2];
        Abo = state[ 3];
        Abu = state[ 4];
        Aga = state[ 5];
        Age = state[ 6];
        Agi = state[ 7];
        Ago = state[ 8];
        Agu = state[ 9];
        Aka = state[10];
        Ake = state[11];
        Aki = state[12];
        Ako = state[13];
        Aku = state[14];
        Ama = state[15];
        Ame = state[16];
        Ami = state[17];
        Amo = state[18];
        Amu = state[19];
        Asa = state[20];
        Ase = state[21];
        Asi = state[22];
        Aso = state[23];
        Asu = state[24];

        for( round = 0; round < NROUNDS; round += 2 )
        {
            //    prepareTheta
            BCa = Aba^Aga^Aka^Ama^Asa;
            BCe = Abe^Age^Ake^Ame^Ase;
            BCi = Abi^Agi^Aki^Ami^Asi;
            BCo = Abo^Ago^Ako^Amo^Aso;
            BCu = Abu^Agu^Aku^Amu^Asu;

            //thetaRhoPiChiIotaPrepareTheta(round  , A, E)
            Da = BCu^ROL(BCe, 1);
            De = BCa^ROL(BCi, 1);
            Di = BCe^ROL(BCo, 1);
            Do = BCi^ROL(BCu, 1);
            Du = BCo^ROL(BCa, 1);

            Aba ^= Da;
            BCa = Aba;
            Age ^= De;
            BCe = ROL(Age, 44);
            Aki ^= Di;
            BCi = ROL(Aki, 43);
            Amo ^= Do;
            BCo = ROL(Amo, 21);
            Asu ^= Du;
            BCu = ROL(Asu, 14);
            Eba =   BCa ^((~BCe)&  BCi );
            Eba ^= (uint64_t)KeccakF_RoundConstants[round];
            Ebe =   BCe ^((~BCi)&  BCo );
            Ebi =   BCi ^((~BCo)&  BCu );
            Ebo =   BCo ^((~BCu)&  BCa );
            Ebu =   BCu ^((~BCa)&  BCe );

            Abo ^= Do;
            BCa = ROL(Abo, 28);
            Agu ^= Du;
            BCe = ROL(Agu, 20);
            Aka ^= Da;
            BCi = ROL(Aka,  3);
            Ame ^= De;
            BCo = ROL(Ame, 45);
            Asi ^= Di;
            BCu = ROL(Asi, 61);
            Ega =   BCa ^((~BCe)&  BCi );
            Ege =   BCe ^((~BCi)&  BCo );
            Egi =   BCi ^((~BCo)&  BCu );
            Ego =   BCo ^((~BCu)&  BCa );
            Egu =   BCu ^((~BCa)&  BCe );

            Abe ^= De;
            BCa = ROL(Abe,  1);
            Agi ^= Di;
            BCe = ROL(Agi,  6);
            Ako ^= Do;
            BCi = ROL(Ako, 25);
            Amu ^= Du;
            BCo = ROL(Amu,  8);
            Asa ^= Da;
            BCu = ROL(Asa, 18);
            Eka =   BCa ^((~BCe)&  BCi );
            Eke =   BCe ^((~BCi)&  BCo );
            Eki =   BCi ^((~BCo)&  BCu );
            Eko =   BCo ^((~BCu)&  BCa );
            Eku =   BCu ^((~BCa)&  BCe );

            Abu ^= Du;
            BCa = ROL(Abu, 27);
            Aga ^= Da;
            BCe = ROL(Aga, 36);
            Ake ^= De;
            BCi = ROL(Ake, 10);
            Ami ^= Di;
            BCo = ROL(Ami, 15);
            Aso ^= Do;
            BCu = ROL(Aso, 56);
            Ema =   BCa ^((~BCe)&  BCi );
            Eme =   BCe ^((~BCi)&  BCo );
            Emi =   BCi ^((~BCo)&  BCu );
            Emo =   BCo ^((~BCu)&  BCa );
            Emu =   BCu ^((~BCa)&  BCe );

            Abi ^= Di;
            BCa = ROL(Abi, 62);
            Ago ^= Do;
            BCe = ROL(Ago, 55);
            Aku ^= Du;
            BCi = ROL(Aku, 39);
            Ama ^= Da;
            BCo = ROL(Ama, 41);
            Ase ^= De;
            BCu = ROL(Ase,  2);
            Esa =   BCa ^((~BCe)&  BCi );
            Ese =   BCe ^((~BCi)&  BCo );
            Esi =   BCi ^((~BCo)&  BCu );
            Eso =   BCo ^((~BCu)&  BCa );
            Esu =   BCu ^((~BCa)&  BCe );

            //    prepareTheta
            BCa = Eba^Ega^Eka^Ema^Esa;
            BCe = Ebe^Ege^Eke^Eme^Ese;
            BCi = Ebi^Egi^Eki^Emi^Esi;
            BCo = Ebo^Ego^Eko^Emo^Eso;
            BCu = Ebu^Egu^Eku^Emu^Esu;

            //thetaRhoPiChiIotaPrepareTheta(round+1, E, A)
            Da = BCu^ROL(BCe, 1);
            De = BCa^ROL(BCi, 1);
            Di = BCe^ROL(BCo, 1);
            Do = BCi^ROL(BCu, 1);
            Du = BCo^ROL(BCa, 1);

            Eba ^= Da;
            BCa = Eba;
            Ege ^= De;
            BCe = ROL(Ege, 44);
            Eki ^= Di;
            BCi = ROL(Eki, 43);
            Emo ^= Do;
            BCo = ROL(Emo, 21);
            Esu ^= Du;
            BCu = ROL(Esu, 14);
            Aba =   BCa ^((~BCe)&  BCi );
            Aba ^= (uint64_t)KeccakF_RoundConstants[round+1];
            Abe =   BCe ^((~BCi)&  BCo );
            Abi =   BCi ^((~BCo)&  BCu );
            Abo =   BCo ^((~BCu)&  BCa );
            Abu =   BCu ^((~BCa)&  BCe );

            Ebo ^= Do;
            BCa = ROL(Ebo, 28);
            Egu ^= Du;
            BCe = ROL(Egu, 20);
            Eka ^= Da;
            BCi = ROL(Eka, 3);
            Eme ^= De;
            BCo = ROL(Eme, 45);
            Esi ^= Di;
            BCu = ROL(Esi, 61);
            Aga =   BCa ^((~BCe)&  BCi );
            Age =   BCe ^((~BCi)&  BCo );
            Agi =   BCi ^((~BCo)&  BCu );
            Ago =   BCo ^((~BCu)&  BCa );
            Agu =   BCu ^((~BCa)&  BCe );

            Ebe ^= De;
            BCa = ROL(Ebe, 1);
            Egi ^= Di;
            BCe = ROL(Egi, 6);
            Eko ^= Do;
            BCi = ROL(Eko, 25);
            Emu ^= Du;
            BCo = ROL(Emu, 8);
            Esa ^= Da;
            BCu = ROL(Esa, 18);
            Aka =   BCa ^((~BCe)&  BCi );
            Ake =   BCe ^((~BCi)&  BCo );
            Aki =   BCi ^((~BCo)&  BCu );
            Ako =   BCo ^((~BCu)&  BCa );
            Aku =   BCu ^((~BCa)&  BCe );

            Ebu ^= Du;
            BCa = ROL(Ebu, 27);
            Ega ^= Da;
            BCe = ROL(Ega, 36);
            Eke ^= De;
            BCi = ROL(Eke, 10);
            Emi ^= Di;
            BCo = ROL(Emi, 15);
            Eso ^= Do;
            BCu = ROL(Eso, 56);
            Ama =   BCa ^((~BCe)&  BCi );
            Ame =   BCe ^((~BCi)&  BCo );
            Ami =   BCi ^((~BCo)&  BCu );
            Amo =   BCo ^((~BCu)&  BCa );
            Amu =   BCu ^((~BCa)&  BCe );

            Ebi ^= Di;
            BCa = ROL(Ebi, 62);
            Ego ^= Do;
            BCe = ROL(Ego, 55);
            Eku ^= Du;
            BCi = ROL(Eku, 39);
            Ema ^= Da;
            BCo = ROL(Ema, 41);
            Ese ^= De;
            BCu = ROL(Ese, 2);
            Asa =   BCa ^((~BCe)&  BCi );
            Ase =   BCe ^((~BCi)&  BCo );
            Asi =   BCi ^((~BCo)&  BCu );
            Aso =   BCo ^((~BCu)&  BCa );
            Asu =   BCu ^((~BCa)&  BCe );
        }

        //copyToState(state, A)
        state[ 0] = Aba;
        state[ 1] = Abe;
        state[ 2] = Abi;
        state[ 3] = Abo;
        state[ 4] = Abu;
        state[ 5] = Aga;
        state[ 6] = Age;
        state[ 7] = Agi;
        state[ 8] = Ago;
        state[ 9] = Agu;
        state[10] = Aka;
        state[11] = Ake;
        state[12] = Aki;
        state[13] = Ako;
        state[14] = Aku;
        state[15] = Ama;
        state[16] = Ame;
        state[17] = Ami;
        state[18] = Amo;
        state[19] = Amu;
        state[20] = Asa;
        state[21] = Ase;
        state[22] = Asi;
        state[23] = Aso;
        state[24] = Asu;
}

/*************************************************
* Name:        keccak_absorb
*
* Description: Absorb step of Keccak;
*              non-incremental, starts by zeroeing the state.
*
* Arguments:   - uint64_t *s: pointer to (uninitialized) output Keccak state
*              - unsigned int r: rate in bytes (e.g., 168 for SHAKE128)
*              - const uint8_t *m: pointer to input to be absorbed into s
*              - int32_t mlen: length of input in bytes
*              - uint8_t p: domain-separation byte for different
*                                 Keccak-derived functions
**************************************************/
static void keccak_absorb(uint64_t *s,
                          uint32_t r,
                          const uint8_t *m,
                          int32_t mlen,
                          uint8_t p)
{
  uint32_t i;
  uint8_t t[200];
  DBENCH_START();

  /* Zero state */
  for(i = 0; i < 25; ++i)
    s[i] = 0;

  while(mlen >= r) {
    for(i = 0; i < r/8; ++i)
      s[i] ^= load64(m + 8*i);

    KeccakF1600_StatePermute(s);
    mlen -= r;
    m += r;
  }

  for(i = 0; i < r; ++i)
    t[i] = 0;
  for(i = 0; i < mlen; ++i)
    t[i] = m[i];
  t[i] = p;
  t[r-1] |= 128;
  for(i = 0; i < r/8; ++i)
    s[i] ^= load64(t + 8*i);

  DBENCH_STOP(*tshake);
}

/*************************************************
* Name:        keccak_squeezeblocks
*
* Description: Squeeze step of Keccak. Squeezes full blocks of r bytes each.
*              Modifies the state. Can be called multiple times to keep
*              squeezing, i.e., is incremental.
*
* Arguments:   - uint8_t *h: pointer to output blocks
*              - int32_t int nblocks: number of blocks to be
*                                                squeezed (written to h)
*              - uint64_t *s: pointer to input/output Keccak state
*              - uint32_t r: rate in bytes (e.g., 168 for SHAKE128)
**************************************************/
static void keccak_squeezeblocks(uint8_t *h,
                                 int32_t nblocks,
                                 uint64_t *s,
                                 uint32_t r)
{
  uint32_t i;
  DBENCH_START();

  while(nblocks > 0) {
    KeccakF1600_StatePermute(s);
    for(i=0; i < (r >> 3); i++) {
      store64(h + 8*i, s[i]);
    }
    h += r;
    --nblocks;
  }

  DBENCH_STOP(*tshake);
}

/*************************************************
* Name:        shake128_absorb
*
* Description: Absorb step of the SHAKE128 XOF.
*              non-incremental, starts by zeroeing the state.
*
* Arguments:   - uint64_t *s: pointer to (uninitialized) output Keccak state
*              - const uint8_t *input: pointer to input to be absorbed
*                                            into s
*              - int32_t inlen: length of input in bytes
**************************************************/
void shake128_absorb(uint64_t *s,
                     const uint8_t *input,
                     int32_t inlen)
{
  keccak_absorb(s, SHAKE128_RATE, input, inlen, 0x1F);
}

/*************************************************
* Name:        shake128_squeezeblocks
*
* Description: Squeeze step of SHAKE128 XOF. Squeezes full blocks of
*              SHAKE128_RATE bytes each. Modifies the state. Can be called
*              multiple times to keep squeezing, i.e., is incremental.
*
* Arguments:   - uint8_t *output: pointer to output blocks
*              - int32_t nblocks: number of blocks to be squeezed
*                                            (written to output)
*              - uint64_t *s: pointer to input/output Keccak state
**************************************************/
void shake128_squeezeblocks(uint8_t *output,
                            int32_t nblocks,
                            uint64_t *s)
{
  keccak_squeezeblocks(output, nblocks, s, SHAKE128_RATE);
}

/*************************************************
* Name:        shake256_absorb
*
* Description: Absorb step of the SHAKE256 XOF.
*              non-incremental, starts by zeroeing the state.
*
* Arguments:   - uint64_t *s: pointer to (uninitialized) output Keccak state
*              - const uint8_t *input: pointer to input to be absorbed
*                                            into s
*              - int32_t inlen: length of input in bytes
**************************************************/
void shake256_absorb(uint64_t *s,
                     const uint8_t *input,
                     int32_t inlen)
{
  keccak_absorb(s, SHAKE256_RATE, input, inlen, 0x1F);
}

/*************************************************
* Name:        shake256_squeezeblocks
*
* Description: Squeeze step of SHAKE256 XOF. Squeezes full blocks of
*              SHAKE256_RATE bytes each. Modifies the state. Can be called
*              multiple times to keep squeezing, i.e., is incremental.
*
* Arguments:   - uint8_t *output: pointer to output blocks
*              - int32_t nblocks: number of blocks to be squeezed
*                                            (written to output)
*              - uint64_t *s: pointer to input/output Keccak state
**************************************************/
void shake256_squeezeblocks(uint8_t *output,
                            int32_t nblocks,
                            uint64_t *s)
{
  keccak_squeezeblocks(output, nblocks, s, SHAKE256_RATE);
}

/*************************************************
* Name:        shake128
*
* Description: SHAKE128 XOF with non-incremental API
*
* Arguments:   - uint8_t *output: pointer to output
*              - int32_t outlen: requested output length in bytes
*              - const uint8_t *input: pointer to input
*              - int32_t inlen: length of input in bytes
**************************************************/
void shake128(uint8_t *output,
              int32_t outlen,
              const uint8_t *input,
              int32_t inlen)
{
  uint32_t i,nblocks = outlen/SHAKE128_RATE;
  uint8_t t[SHAKE128_RATE];
  uint64_t s[25];

  shake128_absorb(s, input, inlen);
  shake128_squeezeblocks(output, nblocks, s);

  output += nblocks*SHAKE128_RATE;
  outlen -= nblocks*SHAKE128_RATE;

  if(outlen) {
    shake128_squeezeblocks(t, 1, s);
    for(i = 0; i < outlen; ++i)
      output[i] = t[i];
  }
}

/*************************************************
* Name:        shake256
*
* Description: SHAKE256 XOF with non-incremental API
*
* Arguments:   - uint8_t *output: pointer to output
*              - int32_t outlen: requested output length in bytes
*              - const uint8_t *input: pointer to input
*              - int32_t inlen: length of input in bytes
**************************************************/
void shake256(uint8_t *output,
              int32_t outlen,
              const uint8_t *input,
              int32_t inlen)
{
  uint32_t i,nblocks = outlen/SHAKE256_RATE;
  uint8_t t[SHAKE256_RATE];
  uint64_t s[25];

  shake256_absorb(s, input, inlen);
  shake256_squeezeblocks(output, nblocks, s);

  output += nblocks*SHAKE256_RATE;
  outlen -= nblocks*SHAKE256_RATE;

  if(outlen) {
    shake256_squeezeblocks(t, 1, s);
    for(i = 0; i < outlen; ++i)
      output[i] = t[i];
  }
}
//#include "params.h"
//#include "reduce.h"
//#include "ntt.h"
//#include "poly.h"

/* Roots of unity in order needed by forward ntt */
static const uint32_t zetas[N] = {0, 25847, 5771523, 7861508, 237124, 7602457, 7504169, 466468, 1826347, 2353451, 8021166, 6288512, 3119733, 5495562, 3111497, 2680103, 2725464, 1024112, 7300517, 3585928, 7830929, 7260833, 2619752, 6271868, 6262231, 4520680, 6980856, 5102745, 1757237, 8360995, 4010497, 280005, 2706023, 95776, 3077325, 3530437, 6718724, 4788269, 5842901, 3915439, 4519302, 5336701, 3574422, 5512770, 3539968, 8079950, 2348700, 7841118, 6681150, 6736599, 3505694, 4558682, 3507263, 6239768, 6779997, 3699596, 811944, 531354, 954230, 3881043, 3900724, 5823537, 2071892, 5582638, 4450022, 6851714, 4702672, 5339162, 6927966, 3475950, 2176455, 6795196, 7122806, 1939314, 4296819, 7380215, 5190273, 5223087, 4747489, 126922, 3412210, 7396998, 2147896, 2715295, 5412772, 4686924, 7969390, 5903370, 7709315, 7151892, 8357436, 7072248, 7998430, 1349076, 1852771, 6949987, 5037034, 264944, 508951, 3097992, 44288, 7280319, 904516, 3958618, 4656075, 8371839, 1653064, 5130689, 2389356, 8169440, 759969, 7063561, 189548, 4827145, 3159746, 6529015, 5971092, 8202977, 1315589, 1341330, 1285669, 6795489, 7567685, 6940675, 5361315, 4499357, 4751448, 3839961, 2091667, 3407706, 2316500, 3817976, 5037939, 2244091, 5933984, 4817955, 266997, 2434439, 7144689, 3513181, 4860065, 4621053, 7183191, 5187039, 900702, 1859098, 909542, 819034, 495491, 6767243, 8337157, 7857917, 7725090, 5257975, 2031748, 3207046, 4823422, 7855319, 7611795, 4784579, 342297, 286988, 5942594, 4108315, 3437287, 5038140, 1735879, 203044, 2842341, 2691481, 5790267, 1265009, 4055324, 1247620, 2486353, 1595974, 4613401, 1250494, 2635921, 4832145, 5386378, 1869119, 1903435, 7329447, 7047359, 1237275, 5062207, 6950192, 7929317, 1312455, 3306115, 6417775, 7100756, 1917081, 5834105, 7005614, 1500165, 777191, 2235880, 3406031, 7838005, 5548557, 6709241, 6533464, 5796124, 4656147, 594136, 4603424, 6366809, 2432395, 2454455, 8215696, 1957272, 3369112, 185531, 7173032, 5196991, 162844, 1616392, 3014001, 810149, 1652634, 4686184, 6581310, 5341501, 3523897, 3866901, 269760, 2213111, 7404533, 1717735, 472078, 7953734, 1723600, 6577327, 1910376, 6712985, 7276084, 8119771, 4546524, 5441381, 6144432, 7959518, 6094090, 183443, 7403526, 1612842, 4834730, 7826001, 3919660, 8332111, 7018208, 3937738, 1400424, 7534263, 1976782};

/* Roots of unity in order needed by inverse ntt */
static const uint32_t zetas_inv[N] = {6403635, 846154, 6979993, 4442679, 1362209, 48306, 4460757, 554416, 3545687, 6767575, 976891, 8196974, 2286327, 420899, 2235985, 2939036, 3833893, 260646, 1104333, 1667432, 6470041, 1803090, 6656817, 426683, 7908339, 6662682, 975884, 6167306, 8110657, 4513516, 4856520, 3038916, 1799107, 3694233, 6727783, 7570268, 5366416, 6764025, 8217573, 3183426, 1207385, 8194886, 5011305, 6423145, 164721, 5925962, 5948022, 2013608, 3776993, 7786281, 3724270, 2584293, 1846953, 1671176, 2831860, 542412, 4974386, 6144537, 7603226, 6880252, 1374803, 2546312, 6463336, 1279661, 1962642, 5074302, 7067962, 451100, 1430225, 3318210, 7143142, 1333058, 1050970, 6476982, 6511298, 2994039, 3548272, 5744496, 7129923, 3767016, 6784443, 5894064, 7132797, 4325093, 7115408, 2590150, 5688936, 5538076, 8177373, 6644538, 3342277, 4943130, 4272102, 2437823, 8093429, 8038120, 3595838, 768622, 525098, 3556995, 5173371, 6348669, 3122442, 655327, 522500, 43260, 1613174, 7884926, 7561383, 7470875, 6521319, 7479715, 3193378, 1197226, 3759364, 3520352, 4867236, 1235728, 5945978, 8113420, 3562462, 2446433, 6136326, 3342478, 4562441, 6063917, 4972711, 6288750, 4540456, 3628969, 3881060, 3019102, 1439742, 812732, 1584928, 7094748, 7039087, 7064828, 177440, 2409325, 1851402, 5220671, 3553272, 8190869, 1316856, 7620448, 210977, 5991061, 3249728, 6727353, 8578, 3724342, 4421799, 7475901, 1100098, 8336129, 5282425, 7871466, 8115473, 3343383, 1430430, 6527646, 7031341, 381987, 1308169, 22981, 1228525, 671102, 2477047, 411027, 3693493, 2967645, 5665122, 6232521, 983419, 4968207, 8253495, 3632928, 3157330, 3190144, 1000202, 4083598, 6441103, 1257611, 1585221, 6203962, 4904467, 1452451, 3041255, 3677745, 1528703, 3930395, 2797779, 6308525, 2556880, 4479693, 4499374, 7426187, 7849063, 7568473, 4680821, 1600420, 2140649, 4873154, 3821735, 4874723, 1643818, 1699267, 539299, 6031717, 300467, 4840449, 2867647, 4805995, 3043716, 3861115, 4464978, 2537516, 3592148, 1661693, 4849980, 5303092, 8284641, 5674394, 8100412, 4369920, 19422, 6623180, 3277672, 1399561, 3859737, 2118186, 2108549, 5760665, 1119584, 549488, 4794489, 1079900, 7356305, 5654953, 5700314, 5268920, 2884855, 5260684, 2091905, 359251, 6026966, 6554070, 7913949, 876248, 777960, 8143293, 518909, 2608894, 8354570};

/*************************************************
* Name:        ntt
*
* Description: Forward NTT, in-place. No modular reduction is performed after
*              additions or subtractions. Hence output coefficients can be up
*              to 16*Q larger than the coefficients of the input polynomial.
*              Output vector is in bitreversed order.
*
* Arguments:   - uint32_t p[N]: input/output coefficient array
**************************************************/
void ntt(uint32_t p[N]) {
  uint32_t len, start, j, k;
  uint32_t zeta, t;

  k = 1;
  for(len = 128; len > 0; len >>= 1) {
    for(start = 0; start < N; start = j + len) {
      zeta = zetas[k++];
      for(j = start; j < start + len; ++j) {
        t = montgomery_reduce((uint64_t)zeta * p[j + len]);
        p[j + len] = p[j] + 2*Q - t;
        p[j] = p[j] + t;
      }
    }
  }
}

/*************************************************
* Name:        invntt_frominvmont
*
* Description: Inverse NTT and multiplication by Montgomery factor 2^32.
*              In-place. No modular reductions after additions or
*              subtractions. Input coefficient need to be smaller than 2*Q.
*              Output coefficient are smaller than 2*Q.
*
* Arguments:   - uint32_t p[N]: input/output coefficient array
**************************************************/
void invntt_frominvmont(uint32_t p[N]) {
  uint32_t start, len, j, k;
  uint32_t t, zeta;
  const uint32_t f = (((uint64_t)MONT*MONT % Q) * (Q-1) % Q) * ((Q-1) >> 8) % Q;

  k = 0;
  for(len = 1; len < N; len <<= 1) {
    for(start = 0; start < N; start = j + len) {
      zeta = zetas_inv[k++];
      for(j = start; j < start + len; ++j) {
        t = p[j];
        p[j] = t + p[j + len];
        p[j + len] = t + 256*Q - p[j + len];
        p[j + len] = montgomery_reduce((uint64_t)zeta * p[j + len]);
      }
    }
  }

  for(j = 0; j < N; ++j) {
    p[j] = montgomery_reduce((uint64_t)f * p[j]);
  }
}
//#include "params.h"
//#include "poly.h"
//#include "polyvec.h"
//#include "packing.h"

/*************************************************
* Name:        pack_pk
*
* Description: Bit-pack public key pk = (rho, t1).
*
* Arguments:   - uint8_t pk[]: output byte array
*              - const uint8_t rho[]: byte array containing rho
*              - const polyveck *t1: pointer to vector t1
**************************************************/
void pack_pk(uint8_t pk[CRYPTO_PUBLICKEYBYTES],
             const uint8_t rho[SEEDBYTES],
             const polyveck *t1)
{
  uint32_t i;

  for(i = 0; i < SEEDBYTES; ++i)
    pk[i] = rho[i];
  pk += SEEDBYTES;

  for(i = 0; i < K; ++i)
    polyt1_pack(pk + i*POLT1_SIZE_PACKED, t1->vec+i);
}

/*************************************************
* Name:        unpack_pk
*
* Description: Unpack public key pk = (rho, t1).
*
* Arguments:   - const uint8_t rho[]: output byte array for rho
*              - const polyveck *t1: pointer to output vector t1
*              - uint8_t pk[]: byte array containing bit-packed pk
**************************************************/
void unpack_pk(uint8_t rho[SEEDBYTES],
               polyveck *t1,
               const uint8_t pk[CRYPTO_PUBLICKEYBYTES])
{
  uint32_t i;

  for(i = 0; i < SEEDBYTES; ++i)
    rho[i] = pk[i];
  pk += SEEDBYTES;

  for(i = 0; i < K; ++i)
    polyt1_unpack(t1->vec+i, pk + i*POLT1_SIZE_PACKED);
}

/*************************************************
* Name:        pack_sk
*
* Description: Bit-pack secret key sk = (rho, key, tr, s1, s2, t0).
*
* Arguments:   - uint8_t sk[]: output byte array
*              - const uint8_t rho[]: byte array containing rho
*              - const uint8_t key[]: byte array containing key
*              - const uint8_t tr[]: byte array containing tr
*              - const polyvecl *s1: pointer to vector s1
*              - const polyveck *s2: pointer to vector s2
*              - const polyveck *t0: pointer to vector t0
**************************************************/
void pack_sk(uint8_t sk[CRYPTO_SECRETKEYBYTES],
             const uint8_t rho[SEEDBYTES],
             const uint8_t key[SEEDBYTES],
             const uint8_t tr[CRHBYTES],
             const polyvecl *s1,
             const polyveck *s2,
             const polyveck *t0)
{
  uint32_t i;

  for(i = 0; i < SEEDBYTES; ++i)
    sk[i] = rho[i];
  sk += SEEDBYTES;

  for(i = 0; i < SEEDBYTES; ++i)
    sk[i] = key[i];
  sk += SEEDBYTES;

  for(i = 0; i < CRHBYTES; ++i)
    sk[i] = tr[i];
  sk += CRHBYTES;

  for(i = 0; i < L; ++i)
    polyeta_pack(sk + i*POLETA_SIZE_PACKED, s1->vec+i);
  sk += L*POLETA_SIZE_PACKED;

  for(i = 0; i < K; ++i)
    polyeta_pack(sk + i*POLETA_SIZE_PACKED, s2->vec+i);
  sk += K*POLETA_SIZE_PACKED;

  for(i = 0; i < K; ++i)
    polyt0_pack(sk + i*POLT0_SIZE_PACKED, t0->vec+i);
}

/*************************************************
* Name:        unpack_sk
*
* Description: Unpack secret key sk = (rho, key, tr, s1, s2, t0).
*
* Arguments:   - const uint8_t rho[]: output byte array for rho
*              - const uint8_t key[]: output byte array for key
*              - const uint8_t tr[]: output byte array for tr
*              - const polyvecl *s1: pointer to output vector s1
*              - const polyveck *s2: pointer to output vector s2
*              - const polyveck *r0: pointer to output vector t0
*              - uint8_t sk[]: byte array containing bit-packed sk
**************************************************/
void unpack_sk(uint8_t rho[SEEDBYTES],
               uint8_t key[SEEDBYTES],
               uint8_t tr[CRHBYTES],
               polyvecl *s1,
               polyveck *s2,
               polyveck *t0,
               const uint8_t sk[CRYPTO_SECRETKEYBYTES])
{
  uint32_t i;

  for(i = 0; i < SEEDBYTES; ++i)
    rho[i] = sk[i];
  sk += SEEDBYTES;

  for(i = 0; i < SEEDBYTES; ++i)
    key[i] = sk[i];
  sk += SEEDBYTES;

  for(i = 0; i < CRHBYTES; ++i)
    tr[i] = sk[i];
  sk += CRHBYTES;

  for(i=0; i < L; ++i)
    polyeta_unpack(s1->vec+i, sk + i*POLETA_SIZE_PACKED);
  sk += L*POLETA_SIZE_PACKED;

  for(i=0; i < K; ++i)
    polyeta_unpack(s2->vec+i, sk + i*POLETA_SIZE_PACKED);
  sk += K*POLETA_SIZE_PACKED;

  for(i=0; i < K; ++i)
    polyt0_unpack(t0->vec+i, sk + i*POLT0_SIZE_PACKED);
}

/*************************************************
* Name:        pack_sig
*
* Description: Bit-pack signature sig = (z, h, c).
*
* Arguments:   - uint8_t sig[]: output byte array
*              - const polyvecl *z: pointer to vector z
*              - const polyveck *h: pointer to hint vector h
*              - const poly *c: pointer to challenge polynomial
**************************************************/
void pack_sig(uint8_t sig[CRYPTO_BYTES],
              const polyvecl *z,
              const polyveck *h,
              const poly *c)
{
  uint32_t i, j, k;
  uint64_t signs, mask;

  for(i = 0; i < L; ++i)
    polyz_pack(sig + i*POLZ_SIZE_PACKED, z->vec+i);
  sig += L*POLZ_SIZE_PACKED;

  /* Encode h */
  k = 0;
  for(i = 0; i < K; ++i) {
    for(j = 0; j < N; ++j)
      if(h->vec[i].coeffs[j] != 0)
        sig[k++] = j;

    sig[OMEGA + i] = k;
  }
  while(k < OMEGA) sig[k++] = 0;
  sig += OMEGA + K;

  /* Encode c */
  signs = 0;
  mask = 1;
  for(i = 0; i < N/8; ++i) {
    sig[i] = 0;
    for(j = 0; j < 8; ++j) {
      if(c->coeffs[8*i+j] != 0) {
        sig[i] |= (1U << j);
        if(c->coeffs[8*i+j] == (Q - 1)) signs |= mask;
        mask <<= 1;
      }
    }
  }
  sig += N/8;
  for(i = 0; i < 8; ++i)
    sig[i] = signs >> 8*i;
}

/*************************************************
* Name:        unpack_sig
*
* Description: Unpack signature sig = (z, h, c).
*
* Arguments:   - polyvecl *z: pointer to output vector z
*              - polyveck *h: pointer to output hint vector h
*              - poly *c: pointer to output challenge polynomial
*              - const uint8_t sig[]: byte array containing
*                bit-packed signature
*
* Returns 1 in case of malformed signature; otherwise 0.
**************************************************/
int unpack_sig(polyvecl *z,
               polyveck *h,
               poly *c,
               const uint8_t sig[CRYPTO_BYTES])
{
  uint32_t i, j, k;
  uint64_t signs, mask;

  for(i = 0; i < L; ++i)
    polyz_unpack(z->vec+i, sig + i*POLZ_SIZE_PACKED);
  sig += L*POLZ_SIZE_PACKED;

  /* Decode h */
  k = 0;
  for(i = 0; i < K; ++i) {
    for(j = 0; j < N; ++j)
      h->vec[i].coeffs[j] = 0;

    if(sig[OMEGA + i] < k || sig[OMEGA + i] > OMEGA)
      return 1;

    for(j = k; j < sig[OMEGA + i]; ++j) {
      /* Coefficients are ordered for strong unforgeability */
      if(j > k && sig[j] <= sig[j-1]) return 1;
      h->vec[i].coeffs[sig[j]] = 1;
    }

    k = sig[OMEGA + i];
  }

  /* Extra indices are zero for strong unforgeability */
  for(j = k; j < OMEGA; ++j)
    if(sig[j])
      return 1;

  sig += OMEGA + K;

  /* Decode c */
  for(i = 0; i < N; ++i)
    c->coeffs[i] = 0;

  signs = 0;
  for(i = 0; i < 8; ++i)
    signs |= (uint64_t)sig[N/8+i] << 8*i;

  /* Extra sign bits are zero for strong unforgeability */
  if(signs >> 60)
    return 1;

  mask = 1;
  for(i = 0; i < N/8; ++i) {
    for(j = 0; j < 8; ++j) {
      if((sig[i] >> j) & 0x01) {
        c->coeffs[8*i+j] = (signs & mask) ? Q - 1 : 1;
        mask <<= 1;
      }
    }
  }

  return 0;
}
//#include <stdint.h>
//#include "test/cpucycles.h"
//#include "fips202.h"
//#include "params.h"
//#include "reduce.h"
//#include "rounding.h"
//#include "ntt.h"
//#include "poly.h"

#ifdef DBENCH
extern const uint64_t timing_overhead;
extern uint64_t *tred, *tadd, *tmul, *tround, *tsample, *tpack;
#endif

/*************************************************
* Name:        poly_reduce
*
* Description: Reduce all coefficients of input polynomial to representative
*              in [0,2*Q[.
*
* Arguments:   - poly *a: pointer to input/output polynomial
**************************************************/
void poly_reduce(poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a->coeffs[i] = reduce32(a->coeffs[i]);

  DBENCH_STOP(*tred);
}

/*************************************************
* Name:        poly_csubq
*
* Description: For all coefficients of input polynomial subtract Q if
*              coefficient is bigger than Q.
*
* Arguments:   - poly *a: pointer to input/output polynomial
**************************************************/
void poly_csubq(poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a->coeffs[i] = csubq(a->coeffs[i]);

  DBENCH_STOP(*tred);
}

/*************************************************
* Name:        poly_freeze
*
* Description: Reduce all coefficients of the polynomial to standard
*              representatives.
*
* Arguments:   - poly *a: pointer to input/output polynomial
**************************************************/
void poly_freeze(poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a->coeffs[i] = freeze(a->coeffs[i]);

  DBENCH_STOP(*tred);
}

/*************************************************
* Name:        poly_add
*
* Description: Add polynomials. No modular reduction is performed.
*
* Arguments:   - poly *c: pointer to output polynomial
*              - const poly *a: pointer to first summand
*              - const poly *b: pointer to second summand
**************************************************/
void poly_add(poly *c, const poly *a, const poly *b)  {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    c->coeffs[i] = a->coeffs[i] + b->coeffs[i];

  DBENCH_STOP(*tadd);
}

/*************************************************
* Name:        poly_sub
*
* Description: Subtract polynomials. Assumes coefficients of second input
*              polynomial to be less than 2*Q. No modular reduction is
*              performed.
*
* Arguments:   - poly *c: pointer to output polynomial
*              - const poly *a: pointer to first input polynomial
*              - const poly *b: pointer to second input polynomial to be
*                               subtraced from first input polynomial
**************************************************/
void poly_sub(poly *c, const poly *a, const poly *b) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    c->coeffs[i] = a->coeffs[i] + 2*Q - b->coeffs[i];

  DBENCH_STOP(*tadd);
}

/*************************************************
* Name:        poly_neg
*
* Description: Negate polynomial. Assumes input coefficients to be standard
*              representatives.
*
* Arguments:   - poly *a: pointer to input/output polynomial
**************************************************/
void poly_neg(poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a->coeffs[i] = Q - a->coeffs[i];

  DBENCH_STOP(*tadd);
}

/*************************************************
* Name:        poly_shiftl
*
* Description: Multiply polynomial by 2^k without modular reduction. Assumes
*              input coefficients to be less than 2^{32-k}.
*
* Arguments:   - poly *a: pointer to input/output polynomial
*              - uint32_t k: exponent
**************************************************/
void poly_shiftl(poly *a, uint32_t k) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a->coeffs[i] <<= k;

  DBENCH_STOP(*tmul);
}

/*************************************************
* Name:        poly_ntt
*
* Description: Forward NTT. Output coefficients can be up to 16*Q larger than
*              input coefficients.
*
* Arguments:   - poly *a: pointer to input/output polynomial
**************************************************/
void poly_ntt(poly *a) {
  DBENCH_START();

  ntt(a->coeffs);

  DBENCH_STOP(*tmul);
}

/*************************************************
* Name:        poly_invntt_montgomery
*
* Description: Inverse NTT and multiplication with 2^{32}. Input coefficients
*              need to be less than 2*Q. Output coefficients are less than 2*Q.
*
* Arguments:   - poly *a: pointer to input/output polynomial
**************************************************/
void poly_invntt_montgomery(poly *a) {
  DBENCH_START();

  invntt_frominvmont(a->coeffs);

  DBENCH_STOP(*tmul);
}

/*************************************************
* Name:        poly_pointwise_invmontgomery
*
* Description: Pointwise multiplication of polynomials in NTT domain
*              representation and multiplication of resulting polynomial
*              with 2^{-32}. Output coefficients are less than 2*Q if input
*              coefficient are less than 22*Q.
*
* Arguments:   - poly *c: pointer to output polynomial
*              - const poly *a: pointer to first input polynomial
*              - const poly *b: pointer to second input polynomial
**************************************************/
void poly_pointwise_invmontgomery(poly *c, const poly *a, const poly *b) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    c->coeffs[i] = montgomery_reduce((uint64_t)a->coeffs[i] * b->coeffs[i]);

  DBENCH_STOP(*tmul);
}

/*************************************************
* Name:        poly_power2round
*
* Description: For all coefficients c of the input polynomial,
*              compute c0, c1 such that c mod Q = c1*2^D + c0
*              with -2^{D-1} < c0 <= 2^{D-1}. Assumes coefficients to be
*              standard representatives.
*
* Arguments:   - poly *a1: pointer to output polynomial with coefficients c1
*              - poly *a0: pointer to output polynomial with coefficients Q + a0
*              - const poly *v: pointer to input polynomial
**************************************************/
void poly_power2round(poly *a1, poly *a0, const poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a1->coeffs[i] = power2round(a->coeffs[i], a0->coeffs+i);

  DBENCH_STOP(*tround);
}

/*************************************************
* Name:        poly_decompose
*
* Description: For all coefficients c of the input polynomial,
*              compute high and low bits c0, c1 such c mod Q = c1*ALPHA + c0
*              with -ALPHA/2 < c0 <= ALPHA/2 except c1 = (Q-1)/ALPHA where we
*              set c1 = 0 and -ALPHA/2 <= c0 = c mod Q - Q < 0.
*              Assumes coefficients to be standard representatives.
*
* Arguments:   - poly *a1: pointer to output polynomial with coefficients c1
*              - poly *a0: pointer to output polynomial with coefficients Q + a0
*              - const poly *c: pointer to input polynomial
**************************************************/
void poly_decompose(poly *a1, poly *a0, const poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a1->coeffs[i] = decompose(a->coeffs[i], a0->coeffs+i);

  DBENCH_STOP(*tround);
}

/*************************************************
* Name:        poly_make_hint
*
* Description: Compute hint polynomial. The coefficients of which indicate
*              whether the high bits of the corresponding coefficients
*              of the first input polynomial and of the sum of the input
*              polynomials differ.
*
* Arguments:   - poly *h: pointer to output hint polynomial
*              - const poly *a: pointer to first input polynomial
*              - const poly *b: pointer to second input polynomial
*
* Returns number of 1 bits.
**************************************************/
uint32_t poly_make_hint(poly *h, const poly *a, const poly *b) {
  uint32_t i, s = 0;
  DBENCH_START();

  for(i = 0; i < N; ++i) {
    h->coeffs[i] = make_hint(a->coeffs[i], b->coeffs[i]);
    s += h->coeffs[i];
  }

  DBENCH_STOP(*tround);
  return s;
}

/*************************************************
* Name:        poly_use_hint
*
* Description: Use hint polynomial to correct the high bits of a polynomial.
*
* Arguments:   - poly *a: pointer to output polynomial with corrected high bits
*              - const poly *b: pointer to input polynomial
*              - const poly *h: pointer to input hint polynomial
**************************************************/
void poly_use_hint(poly *a, const poly *b, const poly *h) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N; ++i)
    a->coeffs[i] = use_hint(b->coeffs[i], h->coeffs[i]);

  DBENCH_STOP(*tround);
}

/*************************************************
* Name:        poly_chknorm
*
* Description: Check infinity norm of polynomial against given bound.
*              Assumes input coefficients to be standard representatives.
*
* Arguments:   - const poly *a: pointer to polynomial
*              - uint32_t B: norm bound
*
* Returns 0 if norm is strictly smaller than B and 1 otherwise.
**************************************************/
int poly_chknorm(const poly *a, uint32_t B) {
  uint32_t i;
  int32_t t;
  DBENCH_START();

  /* It is ok to leak which coefficient violates the bound since
     the probability for each coefficient is independent of secret
     data but we must not leak the sign of the centralized representative. */
  for(i = 0; i < N; ++i) {
    /* Absolute value of centralized representative */
    t = (Q-1)/2 - a->coeffs[i];
    t ^= (t >> 31);
    t = (Q-1)/2 - t;

    if((uint32_t)t >= B) {
      DBENCH_STOP(*tsample);
      return 1;
    }
  }

  DBENCH_STOP(*tsample);
  return 0;
}

/*************************************************
* Name:        poly_uniform
*
* Description: Sample uniformly random polynomial using stream of random bytes.
*              Assumes that enough random bytes are given (e.g.
*              5*SHAKE128_RATE bytes).
*
* Arguments:   - poly *a: pointer to output polynomial
*              - const uint8_t *buf: array of random bytes
**************************************************/
void poly_uniform(poly *a, const uint8_t *buf) {
  uint32_t ctr, pos;
  uint32_t t;
  DBENCH_START();

  ctr = pos = 0;
  while(ctr < N) {
    t  = buf[pos++];
    t |= (uint32_t)buf[pos++] << 8;
    t |= (uint32_t)buf[pos++] << 16;
    t &= 0x7FFFFF;

    if(t < Q)
      a->coeffs[ctr++] = t;
  }

  DBENCH_STOP(*tsample);
}

/*************************************************
* Name:        rej_eta
*
* Description: Sample uniformly random coefficients in [-ETA, ETA] by
*              performing rejection sampling using array of random bytes.
*
* Arguments:   - uint32_t *a: pointer to output array (allocated)
*              - uint32_t len: number of coefficients to be sampled
*              - const uint8_t *buf: array of random bytes
*              - uint32_t buflen: length of array of random bytes
*
* Returns number of sampled coefficients. Can be smaller than len if not enough
* random bytes were given.
**************************************************/
static uint32_t rej_eta(uint32_t *a,
                            uint32_t len,
                            const uint8_t *buf,
                            uint32_t buflen)
{
#if ETA > 7
#error "rej_eta() assumes ETA <= 7"
#endif
  uint32_t ctr, pos;
  uint8_t t0, t1;
  DBENCH_START();

  ctr = pos = 0;
  while(ctr < len && pos < buflen) {
#if ETA <= 3
    t0 = buf[pos] & 0x07;
    t1 = buf[pos++] >> 5;
#else
    t0 = buf[pos] & 0x0F;
    t1 = buf[pos++] >> 4;
#endif

    if(t0 <= 2*ETA)
      a[ctr++] = Q + ETA - t0;
    if(t1 <= 2*ETA && ctr < len)
      a[ctr++] = Q + ETA - t1;
  }

  DBENCH_STOP(*tsample);
  return ctr;
}

/*************************************************
* Name:        poly_uniform_eta
*
* Description: Sample polynomial with uniformly random coefficients
*              in [-ETA,ETA] by performing rejection sampling using the
*              output stream from SHAKE256(seed|nonce).
*
* Arguments:   - poly *a: pointer to output polynomial
*              - const uint8_t seed[]: byte array with seed of length
*                                            SEEDBYTES
*              - uint8_t nonce: nonce byte
**************************************************/
void poly_uniform_eta(poly *a,
                      const uint8_t seed[SEEDBYTES],
                      uint8_t nonce)
{
  uint32_t i, ctr;
  uint8_t inbuf[SEEDBYTES + 1];
  /* Probability that we need more than 2 blocks: < 2^{-84}
     Probability that we need more than 3 blocks: < 2^{-352} */
  uint8_t outbuf[2*SHAKE256_RATE];
  uint64_t state[25];

  for(i= 0; i < SEEDBYTES; ++i)
    inbuf[i] = seed[i];
  inbuf[SEEDBYTES] = nonce;

  shake256_absorb(state, inbuf, SEEDBYTES + 1);
  shake256_squeezeblocks(outbuf, 2, state);

  ctr = rej_eta(a->coeffs, N, outbuf, 2*SHAKE256_RATE);
  if(ctr < N) {
    shake256_squeezeblocks(outbuf, 1, state);
    rej_eta(a->coeffs + ctr, N - ctr, outbuf, SHAKE256_RATE);
  }
}

/*************************************************
* Name:        rej_gamma1m1
*
* Description: Sample uniformly random coefficients
*              in [-(GAMMA1 - 1), GAMMA1 - 1] by performing rejection sampling
*              using array of random bytes.
*
* Arguments:   - uint32_t *a: pointer to output array (allocated)
*              - uint32_t len: number of coefficients to be sampled
*              - const uint8_t *buf: array of random bytes
*              - uint32_t buflen: length of array of random bytes
*
* Returns number of sampled coefficients. Can be smaller than len if not enough
* random bytes were given.
**************************************************/
static uint32_t rej_gamma1m1(uint32_t *a,
                                 uint32_t len,
                                 const uint8_t *buf,
                                 uint32_t buflen)
{
#if GAMMA1 > (1 << 19)
#error "rej_gamma1m1() assumes GAMMA1 - 1 fits in 19 bits"
#endif
  uint32_t ctr, pos;
  uint32_t t0, t1;
  DBENCH_START();

  ctr = pos = 0;
  while(ctr < len && pos + 5 <= buflen) {
    t0  = buf[pos];
    t0 |= (uint32_t)buf[pos + 1] << 8;
    t0 |= (uint32_t)buf[pos + 2] << 16;
    t0 &= 0xFFFFF;

    t1  = buf[pos + 2] >> 4;
    t1 |= (uint32_t)buf[pos + 3] << 4;
    t1 |= (uint32_t)buf[pos + 4] << 12;

    pos += 5;

    if(t0 <= 2*GAMMA1 - 2)
      a[ctr++] = Q + GAMMA1 - 1 - t0;
    if(t1 <= 2*GAMMA1 - 2 && ctr < len)
      a[ctr++] = Q + GAMMA1 - 1 - t1;
  }

  DBENCH_STOP(*tsample);
  return ctr;
}

/*************************************************
* Name:        poly_uniform_gamma1m1
*
* Description: Sample polynomial with uniformly random coefficients
*              in [-(GAMMA1 - 1), GAMMA1 - 1] by performing rejection
*              sampling on output stream of SHAKE256(seed|nonce).
*
* Arguments:   - poly *a: pointer to output polynomial
*              - const uint8_t seed[]: byte array with seed of length
*                                            SEEDBYTES + CRHBYTES
*              - uint16_t nonce: 16-bit nonce
**************************************************/
void poly_uniform_gamma1m1(poly *a,
                           const uint8_t seed[SEEDBYTES + CRHBYTES],
                           uint16_t nonce)
{
  uint32_t i, ctr;
  uint8_t inbuf[SEEDBYTES + CRHBYTES + 2];
  /* Probability that we need more than 5 blocks: < 2^{-81}
     Probability that we need more than 6 blocks: < 2^{-467} */
  uint8_t outbuf[5*SHAKE256_RATE];
  uint64_t state[25];

  for(i = 0; i < SEEDBYTES + CRHBYTES; ++i)
    inbuf[i] = seed[i];
  inbuf[SEEDBYTES + CRHBYTES] = nonce & 0xFF;
  inbuf[SEEDBYTES + CRHBYTES + 1] = nonce >> 8;

  shake256_absorb(state, inbuf, SEEDBYTES + CRHBYTES + 2);
  shake256_squeezeblocks(outbuf, 5, state);

  ctr = rej_gamma1m1(a->coeffs, N, outbuf, 5*SHAKE256_RATE);
  if(ctr < N) {
    /* There are no bytes left in outbuf
       since 5*SHAKE256_RATE is divisible by 5 */
    shake256_squeezeblocks(outbuf, 1, state);
    rej_gamma1m1(a->coeffs + ctr, N - ctr, outbuf, SHAKE256_RATE);
  }
}

/*************************************************
* Name:        polyeta_pack
*
* Description: Bit-pack polynomial with coefficients in [-ETA,ETA].
*              Input coefficients are assumed to lie in [Q-ETA,Q+ETA].
*
* Arguments:   - uint8_t *r: pointer to output byte array with at least
*                                  POLETA_SIZE_PACKED bytes
*              - const poly *a: pointer to input polynomial
**************************************************/
void polyeta_pack(uint8_t *r, const poly *a) {
#if ETA > 7
#error "polyeta_pack() assumes ETA <= 7"
#endif
  uint32_t i;
  uint8_t t[8];
  DBENCH_START();

#if ETA <= 3
  for(i = 0; i < N/8; ++i) {
    t[0] = Q + ETA - a->coeffs[8*i+0];
    t[1] = Q + ETA - a->coeffs[8*i+1];
    t[2] = Q + ETA - a->coeffs[8*i+2];
    t[3] = Q + ETA - a->coeffs[8*i+3];
    t[4] = Q + ETA - a->coeffs[8*i+4];
    t[5] = Q + ETA - a->coeffs[8*i+5];
    t[6] = Q + ETA - a->coeffs[8*i+6];
    t[7] = Q + ETA - a->coeffs[8*i+7];

    r[3*i+0]  = t[0];
    r[3*i+0] |= t[1] << 3;
    r[3*i+0] |= t[2] << 6;
    r[3*i+1]  = t[2] >> 2;
    r[3*i+1] |= t[3] << 1;
    r[3*i+1] |= t[4] << 4;
    r[3*i+1] |= t[5] << 7;
    r[3*i+2]  = t[5] >> 1;
    r[3*i+2] |= t[6] << 2;
    r[3*i+2] |= t[7] << 5;
  }
#else
  for(i = 0; i < N/2; ++i) {
    t[0] = Q + ETA - a->coeffs[2*i+0];
    t[1] = Q + ETA - a->coeffs[2*i+1];
    r[i] = t[0] | (t[1] << 4);
  }
#endif

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyeta_unpack
*
* Description: Unpack polynomial with coefficients in [-ETA,ETA].
*              Output coefficients lie in [Q-ETA,Q+ETA].
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *a: byte array with bit-packed polynomial
**************************************************/
void polyeta_unpack(poly *r, const uint8_t *a) {
  uint32_t i;
  DBENCH_START();

#if ETA <= 3
  for(i = 0; i < N/8; ++i) {
    r->coeffs[8*i+0] = a[3*i+0] & 0x07;
    r->coeffs[8*i+1] = (a[3*i+0] >> 3) & 0x07;
    r->coeffs[8*i+2] = (a[3*i+0] >> 6) | ((a[3*i+1] & 0x01) << 2);
    r->coeffs[8*i+3] = (a[3*i+1] >> 1) & 0x07;
    r->coeffs[8*i+4] = (a[3*i+1] >> 4) & 0x07;
    r->coeffs[8*i+5] = (a[3*i+1] >> 7) | ((a[3*i+2] & 0x03) << 1);
    r->coeffs[8*i+6] = (a[3*i+2] >> 2) & 0x07;
    r->coeffs[8*i+7] = (a[3*i+2] >> 5);

    r->coeffs[8*i+0] = Q + ETA - r->coeffs[8*i+0];
    r->coeffs[8*i+1] = Q + ETA - r->coeffs[8*i+1];
    r->coeffs[8*i+2] = Q + ETA - r->coeffs[8*i+2];
    r->coeffs[8*i+3] = Q + ETA - r->coeffs[8*i+3];
    r->coeffs[8*i+4] = Q + ETA - r->coeffs[8*i+4];
    r->coeffs[8*i+5] = Q + ETA - r->coeffs[8*i+5];
    r->coeffs[8*i+6] = Q + ETA - r->coeffs[8*i+6];
    r->coeffs[8*i+7] = Q + ETA - r->coeffs[8*i+7];
  }
#else
  for(i = 0; i < N/2; ++i) {
    r->coeffs[2*i+0] = a[i] & 0x0F;
    r->coeffs[2*i+1] = a[i] >> 4;
    r->coeffs[2*i+0] = Q + ETA - r->coeffs[2*i+0];
    r->coeffs[2*i+1] = Q + ETA - r->coeffs[2*i+1];
  }
#endif

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyt1_pack
*
* Description: Bit-pack polynomial t1 with coefficients fitting in 9 bits.
*              Input coefficients are assumed to be standard representatives.
*
* Arguments:   - uint8_t *r: pointer to output byte array with at least
*                                  POLT1_SIZE_PACKED bytes
*              - const poly *a: pointer to input polynomial
**************************************************/
void polyt1_pack(uint8_t *r, const poly *a) {
#if D != 14
#error "polyt1_pack() assumes D == 14"
#endif
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N/8; ++i) {
    r[9*i+0]  =  a->coeffs[8*i+0] & 0xFF;
    r[9*i+1]  = (a->coeffs[8*i+0] >> 8) | ((a->coeffs[8*i+1] & 0x7F) << 1);
    r[9*i+2]  = (a->coeffs[8*i+1] >> 7) | ((a->coeffs[8*i+2] & 0x3F) << 2);
    r[9*i+3]  = (a->coeffs[8*i+2] >> 6) | ((a->coeffs[8*i+3] & 0x1F) << 3);
    r[9*i+4]  = (a->coeffs[8*i+3] >> 5) | ((a->coeffs[8*i+4] & 0x0F) << 4);
    r[9*i+5]  = (a->coeffs[8*i+4] >> 4) | ((a->coeffs[8*i+5] & 0x07) << 5);
    r[9*i+6]  = (a->coeffs[8*i+5] >> 3) | ((a->coeffs[8*i+6] & 0x03) << 6);
    r[9*i+7]  = (a->coeffs[8*i+6] >> 2) | ((a->coeffs[8*i+7] & 0x01) << 7);
    r[9*i+8]  =  a->coeffs[8*i+7] >> 1;
  }

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyt1_unpack
*
* Description: Unpack polynomial t1 with 9-bit coefficients.
*              Output coefficients are standard representatives.
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *a: byte array with bit-packed polynomial
**************************************************/
void polyt1_unpack(poly *r, const uint8_t *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N/8; ++i) {
    r->coeffs[8*i+0] =  a[9*i+0]       | ((uint32_t)(a[9*i+1] & 0x01) << 8);
    r->coeffs[8*i+1] = (a[9*i+1] >> 1) | ((uint32_t)(a[9*i+2] & 0x03) << 7);
    r->coeffs[8*i+2] = (a[9*i+2] >> 2) | ((uint32_t)(a[9*i+3] & 0x07) << 6);
    r->coeffs[8*i+3] = (a[9*i+3] >> 3) | ((uint32_t)(a[9*i+4] & 0x0F) << 5);
    r->coeffs[8*i+4] = (a[9*i+4] >> 4) | ((uint32_t)(a[9*i+5] & 0x1F) << 4);
    r->coeffs[8*i+5] = (a[9*i+5] >> 5) | ((uint32_t)(a[9*i+6] & 0x3F) << 3);
    r->coeffs[8*i+6] = (a[9*i+6] >> 6) | ((uint32_t)(a[9*i+7] & 0x7F) << 2);
    r->coeffs[8*i+7] = (a[9*i+7] >> 7) | ((uint32_t)(a[9*i+8] & 0xFF) << 1);
  }

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyt0_pack
*
* Description: Bit-pack polynomial t0 with coefficients in ]-2^{D-1}, 2^{D-1}].
*              Input coefficients are assumed to lie in ]Q-2^{D-1}, Q+2^{D-1}].
*
* Arguments:   - uint8_t *r: pointer to output byte array with at least
*                                  POLT0_SIZE_PACKED bytes
*              - const poly *a: pointer to input polynomial
**************************************************/
void polyt0_pack(uint8_t *r, const poly *a) {
  uint32_t i;
  uint32_t t[4];
  DBENCH_START();

  for(i = 0; i < N/4; ++i) {
    t[0] = Q + (1 << (D-1)) - a->coeffs[4*i+0];
    t[1] = Q + (1 << (D-1)) - a->coeffs[4*i+1];
    t[2] = Q + (1 << (D-1)) - a->coeffs[4*i+2];
    t[3] = Q + (1 << (D-1)) - a->coeffs[4*i+3];

    r[7*i+0]  =  t[0];
    r[7*i+1]  =  t[0] >> 8;
    r[7*i+1] |=  t[1] << 6;
    r[7*i+2]  =  t[1] >> 2;
    r[7*i+3]  =  t[1] >> 10;
    r[7*i+3] |=  t[2] << 4;
    r[7*i+4]  =  t[2] >> 4;
    r[7*i+5]  =  t[2] >> 12;
    r[7*i+5] |=  t[3] << 2;
    r[7*i+6]  =  t[3] >> 6;
  }

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyt0_unpack
*
* Description: Unpack polynomial t0 with coefficients in ]-2^{D-1}, 2^{D-1}].
*              Output coefficients lie in ]Q-2^{D-1},Q+2^{D-1}].
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *a: byte array with bit-packed polynomial
**************************************************/
void polyt0_unpack(poly *r, const uint8_t *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N/4; ++i) {
    r->coeffs[4*i+0]  = a[7*i+0];
    r->coeffs[4*i+0] |= (uint32_t)(a[7*i+1] & 0x3F) << 8;

    r->coeffs[4*i+1]  = a[7*i+1] >> 6;
    r->coeffs[4*i+1] |= (uint32_t)a[7*i+2] << 2;
    r->coeffs[4*i+1] |= (uint32_t)(a[7*i+3] & 0x0F) << 10;

    r->coeffs[4*i+2]  = a[7*i+3] >> 4;
    r->coeffs[4*i+2] |= (uint32_t)a[7*i+4] << 4;
    r->coeffs[4*i+2] |= (uint32_t)(a[7*i+5] & 0x03) << 12;

    r->coeffs[4*i+3]  = a[7*i+5] >> 2;
    r->coeffs[4*i+3] |= (uint32_t)a[7*i+6] << 6;

    r->coeffs[4*i+0] = Q + (1 << (D-1)) - r->coeffs[4*i+0];
    r->coeffs[4*i+1] = Q + (1 << (D-1)) - r->coeffs[4*i+1];
    r->coeffs[4*i+2] = Q + (1 << (D-1)) - r->coeffs[4*i+2];
    r->coeffs[4*i+3] = Q + (1 << (D-1)) - r->coeffs[4*i+3];
  }

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyz_pack
*
* Description: Bit-pack polynomial z with coefficients
*              in [-(GAMMA1 - 1), GAMMA1 - 1].
*              Input coefficients are assumed to be standard representatives.
*
* Arguments:   - uint8_t *r: pointer to output byte array with at least
*                                  POLZ_SIZE_PACKED bytes
*              - const poly *a: pointer to input polynomial
**************************************************/
void polyz_pack(uint8_t *r, const poly *a) {
#if GAMMA1 > (1 << 19)
#error "polyz_pack() assumes GAMMA1 <= 2^{19}"
#endif
  uint32_t i;
  uint32_t t[2];
  DBENCH_START();

  for(i = 0; i < N/2; ++i) {
    /* Map to {0,...,2*GAMMA1 - 2} */
    t[0] = GAMMA1 - 1 - a->coeffs[2*i+0];
    t[0] += ((int32_t)t[0] >> 31) & Q;
    t[1] = GAMMA1 - 1 - a->coeffs[2*i+1];
    t[1] += ((int32_t)t[1] >> 31) & Q;

    r[5*i+0]  = t[0];
    r[5*i+1]  = t[0] >> 8;
    r[5*i+2]  = t[0] >> 16;
    r[5*i+2] |= t[1] << 4;
    r[5*i+3]  = t[1] >> 4;
    r[5*i+4]  = t[1] >> 12;
  }

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyz_unpack
*
* Description: Unpack polynomial z with coefficients
*              in [-(GAMMA1 - 1), GAMMA1 - 1].
*              Output coefficients are standard representatives.
*
* Arguments:   - poly *r: pointer to output polynomial
*              - const uint8_t *a: byte array with bit-packed polynomial
**************************************************/
void polyz_unpack(poly *r, const uint8_t *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N/2; ++i) {
    r->coeffs[2*i+0]  = a[5*i+0];
    r->coeffs[2*i+0] |= (uint32_t)a[5*i+1] << 8;
    r->coeffs[2*i+0] |= (uint32_t)(a[5*i+2] & 0x0F) << 16;

    r->coeffs[2*i+1]  = a[5*i+2] >> 4;
    r->coeffs[2*i+1] |= (uint32_t)a[5*i+3] << 4;
    r->coeffs[2*i+1] |= (uint32_t)a[5*i+4] << 12;

    r->coeffs[2*i+0] = GAMMA1 - 1 - r->coeffs[2*i+0];
    r->coeffs[2*i+0] += ((int32_t)r->coeffs[2*i+0] >> 31) & Q;
    r->coeffs[2*i+1] = GAMMA1 - 1 - r->coeffs[2*i+1];
    r->coeffs[2*i+1] += ((int32_t)r->coeffs[2*i+1] >> 31) & Q;
  }

  DBENCH_STOP(*tpack);
}

/*************************************************
* Name:        polyw1_pack
*
* Description: Bit-pack polynomial w1 with coefficients in [0, 15].
*              Input coefficients are assumed to be standard representatives.
*
* Arguments:   - uint8_t *r: pointer to output byte array with at least
*                                  POLW1_SIZE_PACKED bytes
*              - const poly *a: pointer to input polynomial
**************************************************/
void polyw1_pack(uint8_t *r, const poly *a) {
  uint32_t i;
  DBENCH_START();

  for(i = 0; i < N/2; ++i)
    r[i] = a->coeffs[2*i+0] | (a->coeffs[2*i+1] << 4);

  DBENCH_STOP(*tpack);
}
//#include <stdint.h>
//#include "params.h"
//#include "poly.h"
//#include "polyvec.h"

/**************************************************************/
/************ Vectors of polynomials of length L **************/
/**************************************************************/

/*************************************************
* Name:        polyvecl_freeze
*
* Description: Reduce coefficients of polynomials in vector of length L
*              to standard representatives.
*
* Arguments:   - polyvecl *v: pointer to input/output vector
**************************************************/
void polyvecl_freeze(polyvecl *v) {
  uint32_t i;

  for(i = 0; i < L; ++i)
    poly_freeze(v->vec+i);
}

/*************************************************
* Name:        polyvecl_add
*
* Description: Add vectors of polynomials of length L.
*              No modular reduction is performed.
*
* Arguments:   - polyvecl *w: pointer to output vector
*              - const polyvecl *u: pointer to first summand
*              - const polyvecl *v: pointer to second summand
**************************************************/
void polyvecl_add(polyvecl *w, const polyvecl *u, const polyvecl *v) {
  uint32_t i;

  for(i = 0; i < L; ++i)
    poly_add(w->vec+i, u->vec+i, v->vec+i);
}

/*************************************************
* Name:        polyvecl_ntt
*
* Description: Forward NTT of all polynomials in vector of length L. Output
*              coefficients can be up to 16*Q larger than input coefficients.
*
* Arguments:   - polyvecl *v: pointer to input/output vector
**************************************************/
void polyvecl_ntt(polyvecl *v) {
  uint32_t i;

  for(i = 0; i < L; ++i)
    poly_ntt(v->vec+i);
}

/*************************************************
* Name:        polyvecl_pointwise_acc_invmontgomery
*
* Description: Pointwise multiply vectors of polynomials of length L, multiply
*              resulting vector by 2^{-32} and add (accumulate) polynomials
*              in it. Input/output vectors are in NTT domain representation.
*              Input coefficients are assumed to be less than 22*Q. Output
*              coeffcient are less than 2*L*Q.
*
* Arguments:   - poly *w: output polynomial
*              - const polyvecl *u: pointer to first input vector
*              - const polyvecl *v: pointer to second input vector
**************************************************/
void polyvecl_pointwise_acc_invmontgomery(poly *w,
                                          const polyvecl *u,
                                          const polyvecl *v)
{
  uint32_t i;
  poly t;

  poly_pointwise_invmontgomery(w, u->vec+0, v->vec+0);

  for(i = 1; i < L; ++i) {
    poly_pointwise_invmontgomery(&t, u->vec+i, v->vec+i);
    poly_add(w, w, &t);
  }
}

/*************************************************
* Name:        polyvecl_chknorm
*
* Description: Check infinity norm of polynomials in vector of length L.
*              Assumes input coefficients to be standard representatives.
*
* Arguments:   - const polyvecl *v: pointer to vector
*              - uint32_t B: norm bound
*
* Returns 0 if norm of all polynomials is strictly smaller than B and 1
* otherwise.
**************************************************/
int polyvecl_chknorm(const polyvecl *v, uint32_t bound)  {
  uint32_t i;
  int ret = 0;

  for(i = 0; i < L; ++i)
    ret |= poly_chknorm(v->vec+i, bound);

  return ret;
}

/**************************************************************/
/************ Vectors of polynomials of length K **************/
/**************************************************************/


/*************************************************
* Name:        polyveck_reduce
*
* Description: Reduce coefficients of polynomials in vector of length K
*              to representatives in [0,2*Q[.
*
* Arguments:   - polyveck *v: pointer to input/output vector
**************************************************/
void polyveck_reduce(polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_reduce(v->vec+i);
}

/*************************************************
* Name:        polyveck_csubq
*
* Description: For all coefficients of polynomials in vector of length K
*              subtract Q if coefficient is bigger than Q.
*
* Arguments:   - polyveck *v: pointer to input/output vector
**************************************************/
void polyveck_csubq(polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_csubq(v->vec+i);
}

/*************************************************
* Name:        polyveck_freeze
*
* Description: Reduce coefficients of polynomials in vector of length K
*              to standard representatives.
*
* Arguments:   - polyveck *v: pointer to input/output vector
**************************************************/
void polyveck_freeze(polyveck *v)  {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_freeze(v->vec+i);
}

/*************************************************
* Name:        polyveck_add
*
* Description: Add vectors of polynomials of length K.
*              No modular reduction is performed.
*
* Arguments:   - polyveck *w: pointer to output vector
*              - const polyveck *u: pointer to first summand
*              - const polyveck *v: pointer to second summand
**************************************************/
void polyveck_add(polyveck *w, const polyveck *u, const polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_add(w->vec+i, u->vec+i, v->vec+i);
}

/*************************************************
* Name:        polyveck_sub
*
* Description: Subtract vectors of polynomials of length K.
*              Assumes coefficients of polynomials in second input vector
*              to be less than 2*Q. No modular reduction is performed.
*
* Arguments:   - polyveck *w: pointer to output vector
*              - const polyveck *u: pointer to first input vector
*              - const polyveck *v: pointer to second input vector to be
*                                   subtracted from first input vector
**************************************************/
void polyveck_sub(polyveck *w, const polyveck *u, const polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_sub(w->vec+i, u->vec+i, v->vec+i);
}

/*************************************************
* Name:        polyveck_shiftl
*
* Description: Multiply vector of polynomials of Length K by 2^k without modular
*              reduction. Assumes input coefficients to be less than 2^{32-k}.
*
* Arguments:   - polyveck *v: pointer to input/output vector
*              - uint32_t k: exponent
**************************************************/
void polyveck_shiftl(polyveck *v, uint32_t k) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_shiftl(v->vec+i, k);
}

/*************************************************
* Name:        polyveck_ntt
*
* Description: Forward NTT of all polynomials in vector of length K. Output
*              coefficients can be up to 16*Q larger than input coefficients.
*
* Arguments:   - polyveck *v: pointer to input/output vector
**************************************************/
void polyveck_ntt(polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_ntt(v->vec+i);
}

/*************************************************
* Name:        polyveck_invntt_montgomery
*
* Description: Inverse NTT and multiplication by 2^{32} of polynomials
*              in vector of length K. Input coefficients need to be less
*              than 2*Q.
*
* Arguments:   - polyveck *v: pointer to input/output vector
**************************************************/
void polyveck_invntt_montgomery(polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_invntt_montgomery(v->vec+i);
}

/*************************************************
* Name:        polyveck_chknorm
*
* Description: Check infinity norm of polynomials in vector of length K.
*              Assumes input coefficients to be standard representatives.
*
* Arguments:   - const polyveck *v: pointer to vector
*              - uint32_t B: norm bound
*
* Returns 0 if norm of all polynomials are strictly smaller than B and 1
* otherwise.
**************************************************/
int polyveck_chknorm(const polyveck *v, uint32_t bound) {
  uint32_t i;
  int ret = 0;

  for(i = 0; i < K; ++i)
    ret |= poly_chknorm(v->vec+i, bound);

  return ret;
}

/*************************************************
* Name:        polyveck_power2round
*
* Description: For all coefficients a of polynomials in vector of length K,
*              compute a0, a1 such that a mod Q = a1*2^D + a0
*              with -2^{D-1} < a0 <= 2^{D-1}. Assumes coefficients to be
*              standard representatives.
*
* Arguments:   - polyveck *v1: pointer to output vector of polynomials with
*                              coefficients a1
*              - polyveck *v0: pointer to output vector of polynomials with
*                              coefficients Q + a0
*              - const polyveck *v: pointer to input vector
**************************************************/
void polyveck_power2round(polyveck *v1, polyveck *v0, const polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_power2round(v1->vec+i, v0->vec+i, v->vec+i);
}

/*************************************************
* Name:        polyveck_decompose
*
* Description: For all coefficients a of polynomials in vector of length K,
*              compute high and low bits a0, a1 such a mod Q = a1*ALPHA + a0
*              with -ALPHA/2 < a0 <= ALPHA/2 except a1 = (Q-1)/ALPHA where we
*              set a1 = 0 and -ALPHA/2 <= a0 = a mod Q - Q < 0.
*              Assumes coefficients to be standard representatives.
*
* Arguments:   - polyveck *v1: pointer to output vector of polynomials with
*                              coefficients a1
*              - polyveck *v0: pointer to output vector of polynomials with
*                              coefficients Q + a0
*              - const polyveck *v: pointer to input vector
**************************************************/
void polyveck_decompose(polyveck *v1, polyveck *v0, const polyveck *v) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_decompose(v1->vec+i, v0->vec+i, v->vec+i);
}

/*************************************************
* Name:        polyveck_make_hint
*
* Description: Compute hint vector.
*
* Arguments:   - polyveck *h: pointer to output vector
*              - const polyveck *u: pointer to first input vector
*              - const polyveck *u: pointer to second input vector
*
* Returns number of 1 bits.
**************************************************/
uint32_t polyveck_make_hint(polyveck *h,
                                const polyveck *u,
                                const polyveck *v)
{
  uint32_t i, s = 0;

  for(i = 0; i < K; ++i)
    s += poly_make_hint(h->vec+i, u->vec+i, v->vec+i);

  return s;
}

/*************************************************
* Name:        polyveck_use_hint
*
* Description: Use hint vector to correct the high bits of input vector.
*
* Arguments:   - polyveck *w: pointer to output vector of polynomials with
*                             corrected high bits
*              - const polyveck *u: pointer to input vector
*              - const polyveck *h: pointer to input hint vector
**************************************************/
void polyveck_use_hint(polyveck *w, const polyveck *u, const polyveck *h) {
  uint32_t i;

  for(i = 0; i < K; ++i)
    poly_use_hint(w->vec+i, u->vec+i, h->vec+i);
}
//#include <stdint.h>
//#include "params.h"
//#include "reduce.h"

/*************************************************
* Name:        montgomery_reduce
*
* Description: For finite field element a with 0 <= a <= Q*2^32,
*              compute r \equiv a*2^{-32} (mod Q) such that 0 <= r < 2*Q.
*
* Arguments:   - uint64_t: finite field element a
*
* Returns r.
**************************************************/
uint32_t montgomery_reduce(uint64_t a) {
  uint64_t t;

  t = a * QINV;
  t &= (1ULL << 32) - 1;
  t *= Q;
  t = a + t;
  t >>= 32;
  return t;
}

/*************************************************
* Name:        reduce32
*
* Description: For finite field element a, compute r \equiv a (mod Q)
*              such that 0 <= r < 2*Q.
*
* Arguments:   - uint32_t: finite field element a
*
* Returns r.
**************************************************/
uint32_t reduce32(uint32_t a) {
  uint32_t t;

  t = a & 0x7FFFFF;
  a >>= 23;
  t += (a << 13) - a;
  return t;
}

/*************************************************
* Name:        csubq
*
* Description: Subtract Q if input coefficient is bigger than Q.
*
* Arguments:   - uint32_t: finite field element a
*
* Returns r.
**************************************************/
uint32_t csubq(uint32_t a) {
  a -= Q;
  a += ((int32_t)a >> 31) & Q;
  return a;
}

/*************************************************
* Name:        freeze
*
* Description: For finite field element a, compute standard
*              representative r = a mod Q.
*
* Arguments:   - uint32_t: finite field element a
*
* Returns r.
**************************************************/
uint32_t freeze(uint32_t a) {
  a = reduce32(a);
  a = csubq(a);
  return a;
}
//#include <stdint.h>
//#include "params.h"

/*************************************************
* Name:        power2round
*
* Description: For finite field element a, compute a0, a1 such that
*              a mod Q = a1*2^D + a0 with -2^{D-1} < a0 <= 2^{D-1}.
*              Assumes a to be standard representative.
*
* Arguments:   - uint32_t a: input element
*              - uint32_t *a0: pointer to output element Q + a0
*
* Returns a1.
**************************************************/
uint32_t power2round(uint32_t a, uint32_t *a0)  {
  int32_t t;

  /* Centralized remainder mod 2^D */
  t = a & ((1 << D) - 1);
  t -= (1 << (D-1)) + 1;
  t += (t >> 31) & (1 << D);
  t -= (1 << (D-1)) - 1;
  *a0 = Q + t;
  a = (a - t) >> D;
  return a;
}

/*************************************************
* Name:        decompose
*
* Description: For finite field element a, compute high and low bits a0, a1 such
*              that a mod Q = a1*ALPHA + a0 with -ALPHA/2 < a0 <= ALPHA/2 except
*              if a1 = (Q-1)/ALPHA where we set a1 = 0 and
*              -ALPHA/2 <= a0 = a mod Q - Q < 0. Assumes a to be standard
*              representative.
*
* Arguments:   - uint32_t a: input element
*              - uint32_t *a0: pointer to output element Q + a0
*
* Returns a1.
**************************************************/
uint32_t decompose(uint32_t a, uint32_t *a0) {
#if ALPHA != (Q-1)/16
#error "decompose assumes ALPHA == (Q-1)/16"
#endif
  int32_t t, u;

  /* Centralized remainder mod ALPHA */
  t = a & 0x7FFFF;
  t += (a >> 19) << 9;
  t -= ALPHA/2 + 1;
  t += (t >> 31) & ALPHA;
  t -= ALPHA/2 - 1;
  a -= t;

  /* Divide by ALPHA (possible to avoid) */
  u = a - 1;
  u >>= 31;
  a = (a >> 19) + 1;
  a -= u & 1;

  /* Border case */
  *a0 = Q + t - (a >> 4);
  a &= 0xF;
  return a;
}

/*************************************************
* Name:        make_hint
*
* Description: Compute hint bit indicating whether or not high bits of two
*              finite field elements differ. Assumes input elements to be
*              standard representatives.
*
* Arguments:   - uint32_t a: first input element
*              - uint32_t b: second input element
*
* Returns 1 if high bits of a and b differ and 0 otherwise.
**************************************************/
uint32_t make_hint(const uint32_t a, const uint32_t b) {
  uint32_t t;

  return decompose(a, &t) != decompose(b, &t);
}

/*************************************************
* Name:        use_hint
*
* Description: Correct high bits according to hint.
*
* Arguments:   - uint32_t a: input element
*              - uint32_t hint: hint bit
*
* Returns corrected high bits.
**************************************************/
uint32_t use_hint(const uint32_t a, const uint32_t hint) {
  uint32_t a0, a1;

  a1 = decompose(a, &a0);
  if(hint == 0)
    return a1;
  else if(a0 > Q)
    return (a1 + 1) & 0xF;
  else
    return (a1 - 1) & 0xF;

  /* If decompose does not divide out ALPHA:
  if(hint == 0)
    return a1;
  else if(a0 > Q)
    return (a1 + ALPHA) % (Q - 1);
  else
    return (a1 - ALPHA) % (Q - 1);
  */
}
//#include <stdint.h>
//#include "params.h"
//#include "sign.h"
//#include "randombytes.h"
//#include "fips202.h"
//#include "poly.h"
//#include "polyvec.h"
//#include "packing.h"
#ifdef STANDALONE
#ifdef _WIN32
#include <wincrypt.h>
void randombytes(unsigned char *x,long xlen)
{
    HCRYPTPROV prov = 0;
    CryptAcquireContextW(&prov, NULL, NULL,PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    CryptGenRandom(prov, xlen, x);
    CryptReleaseContext(prov, 0);
}
#else
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
void randombytes(unsigned char *x,long xlen)
{
    static int fd = -1;
    int32_t i;
    if (fd == -1) {
        for (;;) {
            fd = open("/dev/urandom",O_RDONLY);
            if (fd != -1) break;
            sleep(1);
        }
    }
    while (xlen > 0) {
        if (xlen < 1048576) i = (int32_t)xlen; else i = 1048576;
        i = (int32_t)read(fd,x,i);
        if (i < 1) {
            sleep(1);
            continue;
        }
        if ( 0 )
        {
            int32_t j;
            for (j=0; j<i; j++)
                printf("%02x ",x[j]);
            printf("-> %p\n",x);
        }
        x += i;
        xlen -= i;
    }
}
#endif
#endif

/*************************************************
* Name:        expand_mat
*
* Description: Implementation of ExpandA. Generates matrix A with uniformly
*              random coefficients a_{i,j} by performing rejection
*              sampling on the output stream of SHAKE128(rho|i|j).
*
* Arguments:   - polyvecl mat[K]: output matrix
*              - const uint8_t rho[]: byte array containing seed rho
**************************************************/
void expand_mat(polyvecl mat[K], const uint8_t rho[SEEDBYTES]) {
  uint32_t i, j;
  uint8_t inbuf[SEEDBYTES + 1];
  /* Don't change this to smaller values,
   * sampling later assumes sufficient SHAKE output!
   * Probability that we need more than 5 blocks: < 2^{-132}.
   * Probability that we need more than 6 blocks: < 2^{-546}. */
  uint8_t outbuf[5*SHAKE128_RATE];

  for(i = 0; i < SEEDBYTES; ++i)
    inbuf[i] = rho[i];

  for(i = 0; i < K; ++i) {
    for(j = 0; j < L; ++j) {
      inbuf[SEEDBYTES] = i + (j << 4);
      shake128(outbuf, sizeof(outbuf), inbuf, SEEDBYTES + 1);
      poly_uniform(mat[i].vec+j, outbuf);
    }
  }
}

/*************************************************
* Name:        challenge
*
* Description: Implementation of H. Samples polynomial with 60 nonzero
*              coefficients in {-1,1} using the output stream of
*              SHAKE256(mu|w1).
*
* Arguments:   - poly *c: pointer to output polynomial
*              - const uint8_t mu[]: byte array containing mu
*              - const polyveck *w1: pointer to vector w1
**************************************************/
void challenge(poly *c,
               const uint8_t mu[CRHBYTES],
               const polyveck *w1)
{
  uint32_t i, b, pos;
  uint8_t inbuf[CRHBYTES + K*POLW1_SIZE_PACKED];
  uint8_t outbuf[SHAKE256_RATE];
  uint64_t state[25], signs, mask;

  for(i = 0; i < CRHBYTES; ++i)
    inbuf[i] = mu[i];
  for(i = 0; i < K; ++i)
    polyw1_pack(inbuf + CRHBYTES + i*POLW1_SIZE_PACKED, w1->vec+i);

  shake256_absorb(state, inbuf, sizeof(inbuf));
  shake256_squeezeblocks(outbuf, 1, state);

  signs = 0;
  for(i = 0; i < 8; ++i)
    signs |= (uint64_t)outbuf[i] << 8*i;

  pos = 8;
  mask = 1;

  for(i = 0; i < N; ++i)
    c->coeffs[i] = 0;

  for(i = 196; i < 256; ++i) {
    do {
      if(pos >= SHAKE256_RATE) {
        shake256_squeezeblocks(outbuf, 1, state);
        pos = 0;
      }

      b = outbuf[pos++];
    } while(b > i);

    c->coeffs[i] = c->coeffs[b];
    c->coeffs[b] = (signs & mask) ? Q - 1 : 1;
    mask <<= 1;
  }
}

/*************************************************
* Name:        _dilithium_keypair
*
* Description: Generates public and private key.
*
* Arguments:   - uint8_t *pk: pointer to output public key (allocated
*                                   array of CRYPTO_PUBLICKEYBYTES bytes)
*              - uint8_t *sk: pointer to output private key (allocated
*                                   array of CRYPTO_SECRETKEYBYTES bytes)
*
* Returns 0 (success)
**************************************************/
int _dilithium_keypair(uint8_t *pk, uint8_t *sk,uint8_t *privkey)
{
  uint32_t i;
  uint8_t seedbuf[3*SEEDBYTES];
  uint8_t tr[CRHBYTES];
  uint8_t *rho, *rhoprime, *key;
  uint16_t nonce = 0;
  polyvecl mat[K];
  polyvecl s1, s1hat;
  polyveck s2, t, t1, t0;

  /* Expand 32 bytes of randomness into rho, rhoprime and key */
  //randombytes(seedbuf, SEEDBYTES);
    memcpy(seedbuf,privkey,SEEDBYTES);
  shake256(seedbuf, 3*SEEDBYTES, seedbuf, SEEDBYTES);
  rho = seedbuf;
  rhoprime = rho + SEEDBYTES;
  key = rho + 2*SEEDBYTES;

  /* Expand matrix */
  expand_mat(mat, rho);

  /* Sample short vectors s1 and s2 */
  for(i = 0; i < L; ++i)
    poly_uniform_eta(s1.vec+i, rhoprime, nonce++);
  for(i = 0; i < K; ++i)
    poly_uniform_eta(s2.vec+i, rhoprime, nonce++);

  /* Matrix-vector multiplication */
  s1hat = s1;
  polyvecl_ntt(&s1hat);
  for(i = 0; i < K; ++i) {
    polyvecl_pointwise_acc_invmontgomery(t.vec+i, mat+i, &s1hat);
    poly_reduce(t.vec+i);
    poly_invntt_montgomery(t.vec+i);
  }

  /* Add noise vector s2 */
  polyveck_add(&t, &t, &s2);

  /* Extract t1 and write public key */
  polyveck_freeze(&t);
  polyveck_power2round(&t1, &t0, &t);
  pack_pk(pk, rho, &t1);

  /* Compute CRH(rho, t1) and write secret key */
  shake256(tr, CRHBYTES, pk, CRYPTO_PUBLICKEYBYTES);
  pack_sk(sk, rho, key, tr, &s1, &s2, &t0);

  return 0;
}

/*************************************************
* Name:        _dilithium_sign
*
* Description: Compute signed message.
*
* Arguments:   - uint8_t *sm: pointer to output signed message (allocated
*                                   array with CRYPTO_BYTES + mlen bytes),
*                                   can be equal to m
*              - int32_t *smlen: pointer to output length of signed
*                                           message
*              - const uint8_t *m: pointer to message to be signed
*              - int32_t mlen: length of message
*              - const uint8_t *sk: pointer to bit-packed secret key
*
* Returns 0 (success)
**************************************************/
int _dilithium_sign(uint8_t *sm,
                int32_t *smlen,
                const uint8_t *m,
                int32_t mlen,
                const uint8_t *sk)
{
  int32_t i, j;
  uint32_t n;
  uint8_t seedbuf[2*SEEDBYTES + CRHBYTES]; // TODO: nonce in seedbuf (2x)
  uint8_t tr[CRHBYTES];
  uint8_t *rho, *key, *mu;
  uint16_t nonce = 0;
  poly c, chat;
  polyvecl mat[K], s1, y, yhat, z;
  polyveck s2, t0, w, w1;
  polyveck h, wcs2, wcs20, ct0, tmp;

  rho = seedbuf;
  key = seedbuf + SEEDBYTES;
  mu = seedbuf + 2*SEEDBYTES;
  unpack_sk(rho, key, tr, &s1, &s2, &t0, sk);

  /* Copy tr and message into the sm buffer,
   * backwards since m and sm can be equal in SUPERCOP API */
  for(i = 1; i <= mlen; ++i)
    sm[CRYPTO_BYTES + mlen - i] = m[mlen - i];
  for(i = 0; i < CRHBYTES; ++i)
    sm[CRYPTO_BYTES - CRHBYTES + i] = tr[i];

  /* Compute CRH(tr, msg) */
  shake256(mu, CRHBYTES, sm + CRYPTO_BYTES - CRHBYTES, CRHBYTES + mlen);

  /* Expand matrix and transform vectors */
  expand_mat(mat, rho);
  polyvecl_ntt(&s1);
  polyveck_ntt(&s2);
  polyveck_ntt(&t0);

  rej:
  /* Sample intermediate vector y */
  for(i = 0; i < L; ++i)
    poly_uniform_gamma1m1(y.vec+i, key, nonce++);

  /* Matrix-vector multiplication */
  yhat = y;
  polyvecl_ntt(&yhat);
  for(i = 0; i < K; ++i) {
    polyvecl_pointwise_acc_invmontgomery(w.vec+i, mat+i, &yhat);
    poly_reduce(w.vec+i);
    poly_invntt_montgomery(w.vec+i);
  }

  /* Decompose w and call the random oracle */
  polyveck_csubq(&w);
  polyveck_decompose(&w1, &tmp, &w);
  challenge(&c, mu, &w1);

  /* Compute z, reject if it reveals secret */
  chat = c;
  poly_ntt(&chat);
  for(i = 0; i < L; ++i) {
    poly_pointwise_invmontgomery(z.vec+i, &chat, s1.vec+i);
    poly_invntt_montgomery(z.vec+i);
  }
  polyvecl_add(&z, &z, &y);
  polyvecl_freeze(&z);
  if(polyvecl_chknorm(&z, GAMMA1 - BETA))
    goto rej;

  /* Compute w - cs2, reject if w1 can not be computed from it */
  for(i = 0; i < K; ++i) {
    poly_pointwise_invmontgomery(wcs2.vec+i, &chat, s2.vec+i);
    poly_invntt_montgomery(wcs2.vec+i);
  }
  polyveck_sub(&wcs2, &w, &wcs2);
  polyveck_freeze(&wcs2);
  polyveck_decompose(&tmp, &wcs20, &wcs2);
  polyveck_csubq(&wcs20);
  if(polyveck_chknorm(&wcs20, GAMMA2 - BETA))
    goto rej;

  for(i = 0; i < K; ++i)
    for(j = 0; j < N; ++j)
      if(tmp.vec[i].coeffs[j] != w1.vec[i].coeffs[j])
        goto rej;

  /* Compute hints for w1 */
  for(i = 0; i < K; ++i) {
    poly_pointwise_invmontgomery(ct0.vec+i, &chat, t0.vec+i);
    poly_invntt_montgomery(ct0.vec+i);
  }

  polyveck_csubq(&ct0);
  if(polyveck_chknorm(&ct0, GAMMA2))
    goto rej;

  polyveck_add(&tmp, &wcs2, &ct0);
  polyveck_csubq(&tmp);
  n = polyveck_make_hint(&h, &wcs2, &tmp);
  if(n > OMEGA)
    goto rej;

  /* Write signature */
  pack_sig(sm, &z, &h, &c);

  *smlen = mlen + CRYPTO_BYTES;
  return 0;
}

/*************************************************
* Name:        _dilithium_verify
*
* Description: Verify signed message.
*
* Arguments:   - uint8_t *m: pointer to output message (allocated
*                                  array with smlen bytes), can be equal to sm
*              - int32_t *mlen: pointer to output length of message
*              - const uint8_t *sm: pointer to signed message
*              - int32_t smlen: length of signed message
*              - const uint8_t *pk: pointer to bit-packed public key
*
* Returns 0 if signed message could be verified correctly and -1 otherwise
**************************************************/
int _dilithium_verify(uint8_t *m,
                     int32_t *mlen,
                     const uint8_t *sm,
                     int32_t smlen,
                     const uint8_t *pk)
{
  int32_t i;
  uint8_t rho[SEEDBYTES];
  uint8_t mu[CRHBYTES];
  poly c, chat, cp;
  polyvecl mat[K], z;
  polyveck t1, w1, h, tmp1, tmp2;

  if(smlen < CRYPTO_BYTES)
    goto badsig;

  *mlen = smlen - CRYPTO_BYTES;

  unpack_pk(rho, &t1, pk);
  if(unpack_sig(&z, &h, &c, sm))
    goto badsig;
  if(polyvecl_chknorm(&z, GAMMA1 - BETA))
    goto badsig;

  /* Compute CRH(CRH(rho, t1), msg) using m as "playground" buffer */
  if(sm != m)
    for(i = 0; i < *mlen; ++i)
      m[CRYPTO_BYTES + i] = sm[CRYPTO_BYTES + i];

  shake256(m + CRYPTO_BYTES - CRHBYTES, CRHBYTES, pk, CRYPTO_PUBLICKEYBYTES);
  shake256(mu, CRHBYTES, m + CRYPTO_BYTES - CRHBYTES, CRHBYTES + *mlen);

  /* Matrix-vector multiplication; compute Az - c2^dt1 */
  expand_mat(mat, rho);
  polyvecl_ntt(&z);
  for(i = 0; i < K ; ++i)
    polyvecl_pointwise_acc_invmontgomery(tmp1.vec+i, mat+i, &z);

  chat = c;
  poly_ntt(&chat);
  polyveck_shiftl(&t1, D);
  polyveck_ntt(&t1);
  for(i = 0; i < K; ++i)
    poly_pointwise_invmontgomery(tmp2.vec+i, &chat, t1.vec+i);

  polyveck_sub(&tmp1, &tmp1, &tmp2);
  polyveck_reduce(&tmp1);
  polyveck_invntt_montgomery(&tmp1);

  /* Reconstruct w1 */
  polyveck_csubq(&tmp1);
  polyveck_use_hint(&w1, &tmp1, &h);

  /* Call random oracle and verify challenge */
  challenge(&cp, mu, &w1);
  for(i = 0; i < N; ++i)
    if(c.coeffs[i] != cp.coeffs[i])
    {
        /* Signature verification failed */
    badsig:
        *mlen = (int32_t) -1;
        for(i = 0; i < smlen; ++i)
            m[i] = 0;
        
        return -1;
    }

  /* All good, copy msg, return 0 */
  for(i = 0; i < *mlen; ++i)
    m[i] = sm[CRYPTO_BYTES + i];
  return 0;
}

#ifdef STANDALONE
///////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>

#define MLEN 59
#define NTESTS 10000

int64_t timing_overhead;
#ifdef DBENCH
int64_t *tred, *tadd, *tmul, *tround, *tsample, *tpack, *tshake;
#endif

static int cmp_llu(const void *a, const void*b)
{
    if(*(int64_t *)a < *(int64_t *)b) return -1;
    else if(*(int64_t *)a > *(int64_t *)b) return 1;
    //else if ( (uint64_t)a < (uint64_t)b ) return -1;
    //else return 1;
    return(0);
}

static int64_t median(int64_t *l, size_t llen)
{
    qsort(l,llen,sizeof(uint64_t),cmp_llu);
    
    if(llen%2) return l[llen/2];
    else return (l[llen/2-1]+l[llen/2])/2;
}

static int64_t average(int64_t *t, size_t tlen)
{
    uint64_t acc=0;
    size_t i;
    for(i=0;i<tlen;i++)
        acc += t[i];
    return acc/(tlen);
}

static void print_results(const char *s, int64_t *t, size_t tlen)
{
    size_t i;
    printf("%s", s);
    for(i=0;i<tlen-1;i++)
    {
        t[i] = t[i+1] - t[i];
        //fprintf(stderr,"%lld ", (long long)t[i]);
    }
    printf("\n");
    printf("median: %lld\n", (long long)median(t, tlen));
    printf("average: %lld\n", (long long)average(t, tlen-1));
    printf("\n");
}

int32_t main(void)
{
    uint32_t i;
    int32_t ret;
    int32_t j, mlen, smlen;
    uint8_t m[MLEN];
    uint8_t sm[MLEN + CRYPTO_BYTES];
    uint8_t m2[MLEN + CRYPTO_BYTES];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    int64_t tkeygen[NTESTS], tsign[NTESTS], tverify[NTESTS];
#ifdef DBENCH
    int64_t t[7][NTESTS], dummy;
    
    memset(t, 0, sizeof(t));
    tred = tadd = tmul = tround = tsample = tpack = tshake = &dummy;
#endif
    
    timing_overhead = cpucycles_overhead();
    
    for(i = 0; i < NTESTS; ++i)
    {
        
#ifdef DBENCH
        tred = t[0] + i;
        tadd = t[1] + i;
        tmul = t[2] + i;
        tround = t[3] + i;
        tsample = t[4] + i;
        tpack = t[5] + i;
        tshake = t[6] + i;
        tkeygen[i] = cpucycles_start();
#endif
        
        _dilithium_keypair(pk, sk); // 1.3
#ifdef DBENCH
        tkeygen[i] = cpucycles_stop() - tkeygen[i] - timing_overhead;
        // tred = tadd = tmul = tround = tsample = tpack = tshake = &dummy;
        tsign[i] = cpucycles_start();
#endif
        randombytes(m, MLEN); // 1.27

        _dilithium_sign(sm, &smlen, m, MLEN, sk); // 7.2
#ifdef DBENCH
        tsign[i] = cpucycles_stop() - tsign[i] - timing_overhead;
        
        tverify[i] = cpucycles_start();
#endif
        ret = _dilithium_verify(m2, &mlen, sm, smlen, pk);
#ifdef DBENCH
        tverify[i] = cpucycles_stop() - tverify[i] - timing_overhead;
#endif
        if(ret) {
            printf("Verification failed\n");
            return -1;
        }
        
        if(mlen != MLEN) {
            printf("Message lengths don't match\n");
            return -1;
        }
        
        for(j = 0; j < mlen; ++j) {
            if(m[j] != m2[j]) {
                printf("Messages don't match\n");
                return -1;
            }
        }
    }
    
#ifdef DBENCH
    print_results("keygen:", tkeygen, NTESTS);
    print_results("sign: ", tsign, NTESTS);
    print_results("verify: ", tverify, NTESTS);
    
    print_results("modular reduction:", t[0], NTESTS);
    print_results("addition:", t[1], NTESTS);
    print_results("multiplication:", t[2], NTESTS);
    print_results("rounding:", t[3], NTESTS);
    print_results("rejection sampling:", t[4], NTESTS);
    print_results("packing:", t[5], NTESTS);
    print_results("SHAKE:", t[6], NTESTS);
#endif
    
    return 0;
}
#endif

//////////////////////////////////////////////////////

/*
 dilithium has very big pubkeys and privkeys, so some practical things are done to make them more manageable. luckily the big privkey can be generated from a normal 256bit seed in about 100 microseconds. Of course, if you use a normal privkey that is also having its pubkey known, it defeats the purpose of using quantum secure protocol. however it is convenient for testing. just make sure to use externally generated seeds that never get used for secp256k1 if you want to keep it quantum secure.
 
 there are some useful "addresses" starting with 'P' and 'S' that are the base58 encoded dilithium pubkey and privkey. this is just so you can make sure the right one was used in an operation as the ~3kb of hex is very hard to compare visually.
 
 Now comes the cool part. Instead of having to specify these giant pubkeys in each spend and maybe even send, we send to a pubtxid instead. the pubtxid is the txid of a registration tx where a handle, secp256k1 pubkey and the dilithium pubkey are bound together. So by referring to the txid, you refer to all three. Again, for convenience it is possible to use the same secp256k1 pubkey that is derived from the 256bit seed that the dilithium pubkey is generated, but that offers no additonal quantum protection. To gain the quantum protection, use an externally provided seed to generate the dilithium pubkey. there should be no algorithmic linkage between the pubtxid secp256k1 pubkey and the dilithium pubkey. They are linked simply by being in the same register transaction.
 
 Once you have registered the pubkey(s), then you can do a send to it. Both pubkeys are used so that to spend you need to have a proper CC signature and a dilithium signature. The spend will necessarily need to have the almost 4kb signature in the opreturn, but at least the big pubkey is only referenced via the pubtxid
 
 
 First register a pubkey,ie. bind handle, pub33 and bigpub together and then can be referred by pubtxid in other calls
 
 cclib register 19 \"[%22jl777%22]\"
 {
 "handle": "jl777",
 "warning": "test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!",
 "pkaddr": "PNoTcVH8G5TBTQigyVZTsaMMNYYRvywUNu",
 "skaddr": "SejsccjwGrZKaziD1kpfgQhXA32xvzP75i",
 "hex": "0400008085202f89010184fa95fce1a13d441e6c87631f7d0ca5f22ad8b28ae4321e02177b125b5f2400000000494830450221009fb8ff0ea4e810f34e54f0a872952f364e6eb697bb4ab34ea571fd213299b685022017c0b09fc71ec2d2abf49e435a72d32ecc874d14aac39be7b9753704fad7d06c01ffffffff041027000000000000302ea22c8020979f9b424db4e028cdba433622c6cd17b9193763e68b4572cd7f3727dcd335978....00000000000",
 "txid": "9d856b2be6e54c8f04ae3f86aef722b0535180b3e9eb926c53740e481a1715f9",
 "result": "success"
 }
 
 sendrawtransaction <hex> from above -> pubtxid 9d856b2be6e54c8f04ae3f86aef722b0535180b3e9eb926c53740e481a1715f9
 
 now test signing some random 32 byte message
 
 cclib sign 19 \"[%22aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848%22]\"
 {
 "warning": "test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!",
 "msg32": "aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848",
 "pkaddr": "PNoTcVH8G5TBTQigyVZTsaMMNYYRvywUNu",
 "skaddr": "SejsccjwGrZKaziD1kpfgQhXA32xvzP75i",
 "signature": "be067f4bd81b9b0b772e0e2872cc086f6c2ff4c558a465afe80ab71c2c7b39a25ad8300629337c022d8c477cf7728cd11a3f6135bccfdbd68de5cd4517e70a70ec3b836041dc9c2f1abed65f2519e43a31ca6ad4991ce98460a14ee70d28c47f5a1d967c25b1ac93afea7e2b11...836b0f0efbcb26ee679f4f4848",
 "sighash": "cfed6d7f059b87635bde6cb31accd736bf99ff3d"
 }
 
 it is a very big signature, but that seems to be dilithium sig size. let us verify it:
 
 cclib verify 19 \"[%229d856b2be6e54c8f04ae3f86aef722b0535180b3e9eb926c53740e481a1715f9%22,%22aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848%22,%22be067f4bd81b9b0b772e0e2872cc086f6c2ff4c558a465afe80ab71c2c7b39a25ad8300629337c022d8c477cf7728cd11a3f6135bccfdbd68de5cd4517e70a70ec3b836041dc9c2f1abed65f2519e43a31ca6ad4991ce98460a14ee70d28c47f5a1d967c25b1ac93afea7e2b11aa2fb715ac08bd3eac739425c67974ecd682f711a0b175b30278febfe55586650ed8b0098de745944450a6836b6ab23e0c5ebdd7503188428c3159f1671ca27d9d529d344d246e116b2001dbba085afe1bfcdd12d88ae2efbcead268b10cec4f76531aba594887dd239b59c4c676b348a56a1cc2e0032590c74513cfba7f03f8b6d7a14bb6f6a16ae743317ecd8551b3362dc892bcae550032682d130772f65b2e96a5ad4ce2b8e9a41a48c2a52c80f349c99dc110807e7c662f7ef960f628001ca9a9f249b53b23c4680e3a6acec89e3c26d0265b617353654f55a752f9ea3689570c068a414793c3575fae66f6fa425ce282a574981228a52e2ede14fbde3ac66a8e061a538bee737d17fbb48afc39cd914518ef2a182ce1feb66b1a8bf9934b6fef491f2bd3598e3421399fe11754bc61e149e8846f74d44d96c7dc47f06d04d6c09dc2b2c9d78e76a713722eec637f8e3fb5cd5adfd8ba2ce05dacdf2f9522e89bff2ee745d49873755a0079835e982c6c55fd9a96597505d79090da8df4feb422422b1d6427fde4242aafcb6ed581d8e4ffd722daf56fd45b017a2a2fa2f4e30a3a457686bdd184505461fc6749e4a20b7faa2a1d9a295a445ea564b84c1b820d9cf5c06142353671f989565a3767bd6ddabfc3bf1368acdae8870580f21baa2093cea4447688e35719bd78c785821f944ecc9a093f9a65bf2584f1a0c68f70f11f2485e02f288c2c8b6692883983607960aa16065d22082121f6fd6588f07cd3fb57bba624fbb9c7077cb1400fe4edf48156b7622fab70cce1cbd17bde2f4c24b9a86d485727df413e06a6c31cab27284a69fd46e00fc6e80872ed5291b598c74964488ffdb19d0dc94fce37db3f5230d947cb4d83ae55e0357aab1ec86b63fe606f86a77aa78fc4fe986be450b74f1ffbb5ba9eeaea11c7c7ffa6d87a9d49767ce761614bd6cc5df3767ed6396b84354a9634bb3e35606e961fc023504473bf3b7e13244f19d1dee101af1854f80899f95409bb402a5267ad21ddba80e2dd0dd513d0fc88067ac4078e69c12bd19807c03a916d2a42cdbe7b4cdac4bc2314fe3369723d16c30bf277db823c1457f5ff64f3117b82b991ee8b65b7e6b8f7814a15b4ca8cebe88d12236cf1b7dd06b75cab506d78c2072fddf2002be366f43ca68866f87fe9a56808ab7f82aa925091e1f0fba371642039316939446b769973a9c93efe3104699ad3eceac89eb1c2507b65b43d21388f93ff28b194110d7114b97a10cb212515127ede0287d455791e1c6d554b0d8a4e75f2701bc3430786cc69081dbd96a73a308fc6a60fc773fdc7df49b1865c3e989f2a528872fd4c1715dadb11c801c1492ce07bde59e25a801bb542e2caef35f99ca4cb0a3f1d2c2c6e3895c94001a0b2cc648057c2e44c780655f93d56a2cd62a9d55eb8de45e9ec75bfa3d121223aba700062ba3f54162fb9ba136aca6aeb119bca9a0d6bf18e89f54d9ff09c6a2036f767098fbbaf20e10db25e43386ecda201c05e794805269f1a77e50657052d16ae1e154d706a7fa81c419b9d262766e8edb8fd6343f509bed44098ef741f10a6206474c3490354695762a5a4532dd0279abc38ef75a44899a5d8d0e77af638aedd07071f37a3c5f82bbbd05a7b4c0e23d2fc3a5bbc40a52f588c8592f02fb30be56ae0990b24a80690c0b5c9df29549f7dec89f62920a37d05c62c27a62ee01fea164bf28937cdc7d3f2937a5756ada91c2615ce7ed20f0ed07cf486b76d0a63d193363567746eff0ff90ace3dbdfb770d55161c84ccdebca1a600337e7ffed0fdfbc041ed44e0014cced03d1af55ae9fa14d87d60dfe96ac7cde67a1d8ea2150c00ba5fb9a0ec0eff5bd9f734da71edbe7e2f71b6465984c411de8a3cc77a337b2ffdee6ab6d904a79316c15d15176401bc7e72fabb1e9571c7e7188ba09a295400437e4b96549d9827fba6d3493bc6f58f95e240b0a0159054014e5e3103e3af4eef77d3896290c7bf930edbe77615d56aa0a93034c92830c1382c0c06726d2ec7d6c2ed45d3a9fb9646892402812f1df9a003705d3f549d84f9ed3b5fe3c40fcb0bcb28a0d32f2fa27fcdb82509a0725d7314a3eb99a701169fae9e3dcdc2cc20d73aa8b2c5feb645556a8b946581e4e9e82f6a19a21f5babd35d49810dc88923c4908eca3690b774f367a41c3a37b54af9847d73a7eed1ee45edaaed0f316d551c08e3e642cebc97ce71a811664ee9296e7fedffb90011cc353302acd931bc0d152d7e6332a8f0d71059987c3b90f3f57178dec3f30c58ded0bc80eb65b0c9b8d16ec73ebe17e31259181b2376405db17e279419f1c685ad71b6cc91c81a120de2db2c532e67bc3a58d22b549fae61f32398d03cb1f5e245cfec65c40c9dfd0b8a93812f67840c653c5304402a1ff6189fd24f8ce3482e5cf92b3581445009c3b586bb421459ce9457868787c78b787bd45df7e55c3165a92194d38b913a6ef6f31af4c2afcfd0158eb8eb2820f7d41e3efca9367528a0b6fe6ec3fd01082bc60a9fe2a13ab3705b3b0c07173d4d762c8de4b6598d30b97e32339aeb706de47170e1033603267c6ce8caa2977990cbda75984de4e5ede6e36ff889b53b2cbbebc37f9e56e78c62ff856bcb27aba8892ace8fcaae09b31d7f5f850596014e868003d632c9dc12e7c83f6de676d9ae4328862326572e2e0353d5547f7f73fdf5b0227b6d108ae28e3dc622d5ac3dcf98bd1461917d78468ac2912329027c1085611dce7a6b7b3fa8451a5c3c6b448b1b9ad9dd84308991e4688595bcb289ec4b99f63db0c18969bd4b5cdc14d85007d683f936ab3207b59e3971f86f8fb388e72bdc7c9fb3b466061223e85138ee6a5657e8862ca51819c9d92b339ac6900e9f60a71d4a1eb09707cedc32bb477c91a8b5792e850606e1de57122d017a2025423d40b48e0bbe711ec03381630b9003ff55e10ac6f0031dfc54ed54ccd3309abd17ee026958fdf23bb74d53b84d8e2ef150fb2216265454c5f6446e221ab1c95c086571cad14251f618c9c58a9dfb83f9a8c58c9c5c026b9bc8f90860acde16557c064f95b178a9776e463b2d7d658e4acfa1ea30c429c0b813a5872b02d7b0bafcc095e979f737834933fbbf1220c05a0b0346f5932c669c534e22ab5ab42c39fd0e062abff05a2d34060e6f539c7ae9244903d981095fac6cff5d20ac9d298de27cb1ea7079d6dcc47504f988e3bdd1c48ca23f9ec305950459446c51b879a62e75cbc3570d2dbf93594f299111e27b60e5193d6e766a40130ee5d33a43eb43aba5c5701de878fdeaa16c998607e7fbf6c8827cb1f914db9d73c6ae48a0cb416218cc50b335f171e4df050561dfb1669939ccf2c498ff1d8f53a7d7c77195348502c4ffd5c18362f4eb4c3077e504853ff1e84c6166e1f889781bf5dcccf0daf8ac0881ee7202650abdff8d6cda2f8bf3b6a96d23f5ffa0104ee72dd1e8ae7cd08258d36b50cb40048756216845815a3e01efd33d5fae86a0680920422325893296dcb2af0d6df21c7193e387092b61408aac63df4a79c3b1e54869ba3c43ae2a54446e64053c061dd8bb3e132be46d9a83b6675791f49aa9617345801e97be7f4f7159ba1d7da623c7868ad281ddbb0f75fec7fe56ff0a44a8ac3b51a1f784b2b039d6434f92d3254fd83b4221ca18883637a0eb12217ebc8e149681c21e0edbd11289cfa7f78d536d8858a60056b8c28916e1d34ce1a6d344034b2e72162a5fc92b137354c2b791e7ad6ee4679f71181188ba69c9ded078421885a6cc18bc58c383d190c11d236e53eaa39a99d157e4dd74bc4aa2ce1354511128d6b407007dbcaeb9c3b712ed2b334de23c66735f534a9dddb7ab2d06c6a4669d2bd38c8c812b287b39b3591ac77e617834ea7c4c38b1133f2cafdf51f9afca7f44e9b527d3e0e840b05ec8bf57fcceb8a28546a3593ff1b94ee6a8d7d28b8e6007d0ea7da80552e4382b3ff3b6152175083717f42c5c902131b0a27e23bbcf4ba03140a6dc3bcccbc8ca93ba6161fe3c36a1835e9e02695bac571a07f6b2267998213aa0c4c7b93c2ed3a58e12cab5a51edf462a30df14e7e32727b4da1f7f29e9ea30f65ab090b22e9ae00ae9419bf26a44482d536812e2b4c2e1fd2af622d827b04b67eac1052d2ccee68207b3b6ca3d96bc4de4039a3a3e50c58a17786edb08caad6091dab0e7beffd0acb748d5c5ef6a171d8d113c7c310f18712a53607dbf01653157090cdd19c5845c1b7e11a4a61c2229cbb1e6927c74f187964c646b007051841b1b83e670611c1e9eb0b2406ee432122613a4c7e9f60c2cf8db2d6032225604c1d5468b1e90bb57651c2223363743516164a4aab0b4bac2d70d1a254f687384889daee2fc2d32365d78878b8c9aabbbbcc8d7f4fb191d23283f4d5359767e8c99a1b8c8cddfe5040c1e2339606e788ca9cad6f2fc0712236a70c9cdd6fb0000000000000000000000000000000000000000000000000000000000000000000000101c2c3e4c55b80404422409560084401072601824140801b8244ae84401008080081022408cdea5834e5fd1220daff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848%22]\"
 {
 "sighash": "cfed6d7f059b87635bde6cb31accd736bf99ff3d",
 "msg32": "aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848",
 "handle": "jl777",
 "pkaddr": "PNoTcVH8G5TBTQigyVZTsaMMNYYRvywUNu",
 "result": "success"
 }
 
 the basics are working, now it is time to send and spend
 
 cclib send 19 \"[%22jl777%22,%229d856b2be6e54c8f04ae3f86aef722b0535180b3e9eb926c53740e481a1715f9%22,7.77]\"
 {
 "handle": "jl777",
 "hex": "0400008085202f8901ff470ca3fb4f935a32dd312db801dcabce0e8b49c7774bb4f1d39a45b3a68bab0100000049483045022100d1c29d5f870dd18aa865e12632fa0cc8df9a8a770a23360e9c443d39cb141c5f0220304c7c77a6d711888d4bcb836530b6509eabe158496029b0bf57b5716f24beb101ffffffff034014502e00000000302ea22c8020b09ee47b12b5b9a2edcf0e7c4fb2a517b879eb88ac98b16185dfef476506b1dd8103120c008203000401cc3cd0ff7646070000232102aff51dad774a1c612dc82e63f85f07b992b665836b0f0efbcb26ee679f4f4848ac0000000000000000246a221378f915171a480e74536c92ebe9b3805153b022f7ae863fae048f4ce5e62b6b859d00000000120c00000000000000000000000000",
 "txid": "4aac73ebe82c12665d1d005a0ae1a1493cb1e2c714680ef9d016f48a7c77b4a2",
 "result": "success"
 }
 dont forget to broadcast it: 4aac73ebe82c12665d1d005a0ae1a1493cb1e2c714680ef9d016f48a7c77b4a2
 notice how small the tx is! 289 bytes as it is sent to the destpubtxid, which in turn contains the handle, pub33 and bigpub. the handle is used for error check, pub33 is used to make the destination CC address, so the normal CC signing needs to be passed in addition to the spend restrictions for dilithium.
 
 cclib spend 19 \"[%224aac73ebe82c12665d1d005a0ae1a1493cb1e2c714680ef9d016f48a7c77b4a2%22,%22210255c46dbce584e3751081b39d7fc054fc807100557e73fc444481618b5706afb4ac%22]\"
 
 this generates a really big hex, broadcast it and if all went well it will get confirmed.
 a dilithium spend!
 
 to generate a seed that wont be directly derivable from an secp256k1 keypair, do:
 cclib keypair 19 \"[%22rand%22]\"
 
 to do a Qsend (multiple dilithium inputs and outputs)
 
 cclib Qsend 19 \"[%22mypubtxid%22,%22<hexseed>%22,%22<destpubtxid>%22,0.777]\"
 there can be up to 64 outputs, where each one can be a different destpubtxid or scriptPubKey. The only restriction is that scriptPubKey hex cant be 32 bytes.
 
 Qsend is able to spend many Qvins as long as they are for the same dilithium bigpub + secp pub33. And the outputs can be to many different Qvouts or normal vouts. This allows to keep funds totally within the dilithium system and also to transfer back to normal taddrs. Qsend currently only sends from Qfunds, though it could also use funds from normal inputs.
 
 Currently, to get funds from normal inputs to a dilithium, the send rpc can be used as above. So that provides a way to push funds into dilithium. The spend rpc becomes redundant with Qsend.
 
 To properly test this, we need to make sure that transactions Qsend can use send outputs, and Qsend outputs and a combination. Of course, it needs to be validated that funds are not lost, Qsends work properly, etc.
 
 */

#define DILITHIUM_TXFEE 10000

void calc_rmd160_sha256(uint8_t rmd160[20],uint8_t *data,int32_t datalen);
char *bitcoin_address(char *coinaddr,uint8_t addrtype,uint8_t *pubkey_or_rmd160,int32_t len);

struct dilithium_handle
{
    UT_hash_handle hh;
    uint256 destpubtxid;
    char handle[32];
} *Dilithium_handles;

pthread_mutex_t DILITHIUM_MUTEX;

struct dilithium_handle *dilithium_handlenew(char *handle)
{
    struct dilithium_handle *hashstr = 0; int32_t len = (int32_t)strlen(handle);
    if ( len < sizeof(Dilithium_handles[0].handle)-1 )
    {
        pthread_mutex_lock(&DILITHIUM_MUTEX);
        HASH_FIND(hh,Dilithium_handles,handle,len,hashstr);
        if ( hashstr == 0 )
        {
            hashstr = (struct dilithium_handle *)calloc(1,sizeof(*hashstr));
            strncpy(hashstr->handle,handle,sizeof(hashstr->handle));
            HASH_ADD_KEYPTR(hh,Dilithium_handles,hashstr->handle,len,hashstr);
        }
        pthread_mutex_unlock(&DILITHIUM_MUTEX);
    }
    return(hashstr);
}

struct dilithium_handle *dilithium_handlefind(char *handle)
{
    struct dilithium_handle *hashstr = 0; int32_t len = (int32_t)strlen(handle);
    if ( len < sizeof(Dilithium_handles[0].handle)-1 )
    {
        pthread_mutex_lock(&DILITHIUM_MUTEX);
        HASH_FIND(hh,Dilithium_handles,handle,len,hashstr);
        pthread_mutex_unlock(&DILITHIUM_MUTEX);
    }
    return(hashstr);
}

int32_t dilithium_Qmsghash(uint8_t *msg,CTransaction tx,int32_t numvouts,std::vector<uint256> voutpubtxids)
{
    CScript data; uint256 hash; int32_t i,numvins,len = 0; std::vector<uint256> vintxids; std::vector<int32_t> vinprevns; std::vector<CTxOut> vouts;
    numvins = tx.vin.size();
    for (i=0; i<numvins; i++)
    {
        vintxids.push_back(tx.vin[i].prevout.hash);
        vinprevns.push_back(tx.vin[i].prevout.n);
        //fprintf(stderr,"%s/v%d ",tx.vin[i].prevout.hash.GetHex().c_str(),tx.vin[i].prevout.n);
    }
    for (i=0; i<numvouts; i++)
    {
        //char destaddr[64];
        //Getscriptaddress(destaddr,tx.vout[i].scriptPubKey);
        //fprintf(stderr,"%s %.8f ",destaddr,(double)tx.vout[i].nValue/COIN);
        vouts.push_back(tx.vout[i]);
    }
    data << E_MARSHAL(ss << vintxids << vinprevns << vouts << voutpubtxids);
    //fprintf(stderr,"numvins.%d numvouts.%d size of data.%d\n",numvins,numvouts,(int32_t)data.size());
    hash = Hash(data.begin(),data.end());
    memcpy(msg,&hash,sizeof(hash));
    return(0);
}

CScript dilithium_registeropret(std::string handle,CPubKey pk,std::vector<uint8_t> bigpub)
{
    CScript opret; uint8_t evalcode = EVAL_DILITHIUM;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'R' << handle << pk << bigpub);
    return(opret);
}

uint8_t dilithium_registeropretdecode(std::string &handle,CPubKey &pk,std::vector<uint8_t> &bigpub,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> handle; ss >> pk; ss >> bigpub) != 0 && e == EVAL_DILITHIUM && f == 'R' )
    {
        return(f);
    }
    return(0);
}

CScript dilithium_sendopret(uint256 destpubtxid)
{
    CScript opret; uint8_t evalcode = EVAL_DILITHIUM;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'x' << destpubtxid);
    return(opret);
}

uint8_t dilithium_sendopretdecode(uint256 &destpubtxid,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> destpubtxid) != 0 && e == EVAL_DILITHIUM && f == 'x' )
    {
        return(f);
    }
    return(0);
}

CScript dilithium_spendopret(uint256 destpubtxid,std::vector<uint8_t> sig)
{
    CScript opret; uint8_t evalcode = EVAL_DILITHIUM;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'y' << destpubtxid << sig);
    return(opret);
}

uint8_t dilithium_spendopretdecode(uint256 &destpubtxid,std::vector<uint8_t> &sig,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> destpubtxid; ss >> sig) != 0 && e == EVAL_DILITHIUM && f == 'y' )
    {
        return(f);
    }
    return(0);
}

CScript dilithium_Qsendopret(uint256 destpubtxid,std::vector<uint8_t>sig,std::vector<uint256> voutpubtxids)
{
    CScript opret; uint8_t evalcode = EVAL_DILITHIUM;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'Q' << destpubtxid << sig << voutpubtxids);
    return(opret);
}

uint8_t dilithium_Qsendopretdecode(uint256 &destpubtxid,std::vector<uint8_t>&sig,std::vector<uint256> &voutpubtxids,CScript scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> destpubtxid; ss >> sig; ss >> voutpubtxids) != 0 && e == EVAL_DILITHIUM && f == 'Q' )
    {
        return(f);
    }
    return(0);
}

UniValue dilithium_rawtxresult(UniValue &result,std::string rawtx)
{
    CTransaction tx;
    if ( rawtx.size() > 0 )
    {
        result.push_back(Pair("hex",rawtx));
        if ( DecodeHexTx(tx,rawtx) != 0 )
        {
            //if ( broadcastflag != 0 && myAddtomempool(tx) != 0 )
            //    RelayTransaction(tx);
            result.push_back(Pair("txid",tx.GetHash().ToString()));
            result.push_back(Pair("result","success"));
        } else result.push_back(Pair("error","decode hex"));
    } else result.push_back(Pair("error","couldnt finalize CCtx"));
    return(result);
}

char *dilithium_addr(char *coinaddr,uint8_t *buf,int32_t len)
{
    uint8_t rmd160[20],addrtype;
    if ( len == CRYPTO_PUBLICKEYBYTES )
        addrtype = 55;
    else if ( len == CRYPTO_SECRETKEYBYTES )
        addrtype = 63;
    else
    {
        sprintf(coinaddr,"unexpected len.%d",len);
        return(coinaddr);
    }
    calc_rmd160_sha256(rmd160,buf,len);
    bitcoin_address(coinaddr,addrtype,rmd160,20);
    return(coinaddr);
}

char *dilithium_hexstr(char *str,uint8_t *buf,int32_t len)
{
    int32_t i;
    for (i=0; i<len; i++)
        sprintf(&str[i<<1],"%02x",buf[i]);
    str[i<<1] = 0;
    return(str);
}

int32_t dilithium_bigpubget(std::string &handle,CPubKey &pk33,uint8_t *pk,uint256 pubtxid)
{
    CTransaction tx; uint8_t funcid; uint256 hashBlock; int32_t numvouts=0; std::vector<uint8_t> bigpub;
    if ( myGetTransaction(pubtxid,tx,hashBlock) != 0 )
    {
        if ( (numvouts= tx.vout.size()) > 1 )
        {
            if ( (funcid= dilithium_registeropretdecode(handle,pk33,bigpub,tx.vout[numvouts-1].scriptPubKey)) == 'R' && bigpub.size() == CRYPTO_PUBLICKEYBYTES )
            {
                memcpy(pk,&bigpub[0],CRYPTO_PUBLICKEYBYTES);
                return(0);
            } else return(-2);
        }
    }
    return(-1);
}

UniValue dilithium_keypair(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); uint8_t seed[SEEDBYTES],pk[CRYPTO_PUBLICKEYBYTES],sk[CRYPTO_SECRETKEYBYTES]; char coinaddr[64],str[CRYPTO_SECRETKEYBYTES*2+1]; int32_t i,n,externalflag=0;
    Myprivkey(seed);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 1 )
    {
        if ( cclib_parsehash(seed,jitem(params,0),32) < 0 )
        {
            randombytes(seed,SEEDBYTES);
            result.push_back(Pair("status","using random high entropy seed"));
            result.push_back(Pair("seed",dilithium_hexstr(str,seed,SEEDBYTES)));
        }
        externalflag = 1;
    }
    _dilithium_keypair(pk,sk,seed);
    result.push_back(Pair("pubkey",dilithium_hexstr(str,pk,CRYPTO_PUBLICKEYBYTES)));
    result.push_back(Pair("privkey",dilithium_hexstr(str,sk,CRYPTO_SECRETKEYBYTES)));
    result.push_back(Pair("pkaddr",dilithium_addr(coinaddr,pk,CRYPTO_PUBLICKEYBYTES)));
    result.push_back(Pair("skaddr",dilithium_addr(coinaddr,sk,CRYPTO_SECRETKEYBYTES)));
    if ( externalflag == 0 )
        result.push_back(Pair("warning","test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!"));
    result.push_back(Pair("result","success"));
    memset(seed,0,32);
    memset(sk,0,sizeof(sk));
    return(result);
}

CPubKey Faucet_pubkeyget()
{
    struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_FAUCET);
    return(GetUnspendable(cp,0));
}

UniValue dilithium_register(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey faucetpk,mypk,dilithiumpk; uint8_t seed[SEEDBYTES],pk[CRYPTO_PUBLICKEYBYTES],sk[CRYPTO_SECRETKEYBYTES]; char coinaddr[64],str[CRYPTO_SECRETKEYBYTES*2+1]; int64_t CCchange,inputs; std::vector<uint8_t> bigpub; int32_t i,n,warningflag = 0;
    if ( txfee == 0 )
        txfee = DILITHIUM_TXFEE;
    faucetpk = Faucet_pubkeyget();
    mypk = pubkey2pk(Mypubkey());
    dilithiumpk = GetUnspendable(cp,0);
    if ( params != 0 && ((n= cJSON_GetArraySize(params)) == 1 || n == 2) )
    {
        std::string handle(jstr(jitem(params,0),0));
        result.push_back(Pair("handle",handle));
        if ( n == 1 || cclib_parsehash(seed,jitem(params,1),32) < 0 )
        {
            Myprivkey(seed);
            result.push_back(Pair("warning","test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!"));
        }
        _dilithium_keypair(pk,sk,seed);
        result.push_back(Pair("pkaddr",dilithium_addr(coinaddr,pk,CRYPTO_PUBLICKEYBYTES)));
        result.push_back(Pair("skaddr",dilithium_addr(coinaddr,sk,CRYPTO_SECRETKEYBYTES)));
        for (i=0; i<CRYPTO_PUBLICKEYBYTES; i++)
            bigpub.push_back(pk[i]);
        if ( (inputs= AddCClibtxfee(cp,mtx,dilithiumpk)) > 0 )
        {
            if ( inputs > txfee )
                CCchange = (inputs - txfee);
            else CCchange = 0;
            if ( AddNormalinputs(mtx,mypk,COIN+3*txfee,64) >= 3*txfee )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,2*txfee,dilithiumpk));
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,txfee,mypk));
                mtx.vout.push_back(MakeCC1vout(EVAL_FAUCET,COIN,faucetpk));
                if ( CCchange != 0 )
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,dilithiumpk));
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,dilithium_registeropret(handle,mypk,bigpub));
                memset(seed,0,32);
                memset(sk,0,sizeof(sk));
                return(musig_rawtxresult(result,rawtx));
            }
            else
            {
                memset(seed,0,32);
                memset(sk,0,sizeof(sk));
                return(cclib_error(result,"couldnt find enough funds"));
            }
        }
        else
        {
            memset(seed,0,32);
            memset(sk,0,sizeof(sk));
            return(cclib_error(result,"not enough parameters"));
        }
    } else return(cclib_error(result,"not dilithiumpk funds"));
}

UniValue dilithium_sign(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); uint8_t seed[SEEDBYTES],msg[32],rmd160[20],pk[CRYPTO_PUBLICKEYBYTES],sk[CRYPTO_SECRETKEYBYTES],sm[32+CRYPTO_BYTES]; char coinaddr[64],str[(32+CRYPTO_BYTES)*2+1]; int32_t n,smlen;
    if ( params != 0 && ((n= cJSON_GetArraySize(params)) == 1 || n == 2) )
    {
        if ( cclib_parsehash(msg,jitem(params,0),32) < 0 )
            return(cclib_error(result,"couldnt parse message to sign"));
        else if ( n == 1 || cclib_parsehash(seed,jitem(params,1),32) < 0 )
        {
            Myprivkey(seed);
            result.push_back(Pair("warning","test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!"));
        }
        _dilithium_keypair(pk,sk,seed);
        result.push_back(Pair("msg32",dilithium_hexstr(str,msg,32)));
        result.push_back(Pair("pkaddr",dilithium_addr(coinaddr,pk,CRYPTO_PUBLICKEYBYTES)));
        result.push_back(Pair("skaddr",dilithium_addr(coinaddr,sk,CRYPTO_SECRETKEYBYTES)));
        _dilithium_sign(sm,&smlen,msg,32,sk);
        if ( smlen == 32+CRYPTO_BYTES )
        {
            result.push_back(Pair("signature",dilithium_hexstr(str,sm,smlen)));
            calc_rmd160_sha256(rmd160,sm,smlen);
            result.push_back(Pair("sighash",dilithium_hexstr(str,rmd160,20)));
            memset(seed,0,32);
            memset(sk,0,sizeof(sk));
            return(result);
        }
        else
        {
            memset(seed,0,32);
            memset(sk,0,sizeof(sk));
            return(cclib_error(result,"unexpected signed message len"));
        }
    }
    else
    {
        memset(seed,0,32);
        memset(sk,0,sizeof(sk));
        return(cclib_error(result,"not enough parameters"));
    }
}

UniValue dilithium_verify(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); CPubKey pk33; uint8_t rmd160[20],msg[32],msg2[CRYPTO_BYTES+32],pk[CRYPTO_PUBLICKEYBYTES],sm[32+CRYPTO_BYTES]; uint256 pubtxid; char coinaddr[64],str[(32+CRYPTO_BYTES)*2+1]; int32_t smlen=32+CRYPTO_BYTES,mlen,n; std::string handle;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        pubtxid = juint256(jitem(params,0));
        if ( dilithium_bigpubget(handle,pk33,pk,pubtxid) < 0 )
            return(cclib_error(result,"couldnt parse message to sign"));
        else if ( cclib_parsehash(msg,jitem(params,1),32) < 0 )
            return(cclib_error(result,"couldnt parse message to sign"));
        else if ( cclib_parsehash(sm,jitem(params,2),smlen) < 0 )
            return(cclib_error(result,"couldnt parse sig"));
        else
        {
            calc_rmd160_sha256(rmd160,sm,smlen);
            result.push_back(Pair("sighash",dilithium_hexstr(str,rmd160,20)));
            if ( _dilithium_verify(msg2,&mlen,sm,smlen,pk) < 0 )
                return(cclib_error(result,"dilithium verify error"));
            else if ( mlen != 32 )
                return(cclib_error(result,"message len mismatch"));
            else if ( memcmp(msg2,msg,32) != 0 )
                return(cclib_error(result,"message content mismatch"));
            result.push_back(Pair("msg32",dilithium_hexstr(str,msg,32)));
            result.push_back(Pair("handle",handle));
            result.push_back(Pair("pkaddr",dilithium_addr(coinaddr,pk,CRYPTO_PUBLICKEYBYTES)));
            result.push_back(Pair("result","success"));
            return(result);
        }
    } else return(cclib_error(result,"not enough parameters"));
}

UniValue dilithium_send(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx,checkhandle; CPubKey destpub33,mypk,dilithiumpk; int32_t i,n; int64_t amount; uint256 destpubtxid; uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    if ( txfee == 0 )
        txfee = DILITHIUM_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    dilithiumpk = GetUnspendable(cp,0);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 3 )
    {
        amount = jdouble(jitem(params,2),0)*COIN + 0.0000000049;
        std::string handle(jstr(jitem(params,0),0));
        result.push_back(Pair("handle",handle));
        destpubtxid = juint256(jitem(params,1));
        if ( dilithium_bigpubget(checkhandle,destpub33,pk,destpubtxid) < 0 )
            return(cclib_error(result,"couldnt parse message to sign"));
        else if ( handle == checkhandle )
        {
            if ( AddNormalinputs(mtx,mypk,amount+txfee,64) >= amount+txfee )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,destpub33));
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,dilithium_sendopret(destpubtxid));
                return(musig_rawtxresult(result,rawtx));
            } else return(cclib_error(result,"couldnt find enough funds"));
        } else return(cclib_error(result,"handle mismatch"));
    } else return(cclib_error(result,"not enough parameters"));
}

UniValue dilithium_spend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey mypk,destpub33; CTransaction vintx; uint256 prevhash,hashBlock,destpubtxid; int32_t i,smlen,n,numvouts; char str[129],*scriptstr,*retstr=""; CTxOut vout; std::string handle; uint8_t pk[CRYPTO_PUBLICKEYBYTES],pk2[CRYPTO_PUBLICKEYBYTES],sk[CRYPTO_SECRETKEYBYTES],msg[32],seed[32]; std::vector<uint8_t> sig;
    if ( txfee == 0 )
        txfee = DILITHIUM_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    if ( params != 0 && ((n= cJSON_GetArraySize(params)) == 2 || n == 3) )
    {
        prevhash = juint256(jitem(params,0));
        scriptstr = jstr(jitem(params,1),0);
        if ( n == 2 || cclib_parsehash(seed,jitem(params,2),32) < 0 )
        {
            Myprivkey(seed);
            result.push_back(Pair("warning","test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!"));
        }
        _dilithium_keypair(pk,sk,seed);
        if ( is_hexstr(scriptstr,0) != 0 )
        {
            CScript scriptPubKey;
            scriptPubKey.resize(strlen(scriptstr)/2);
            decode_hex(&scriptPubKey[0],strlen(scriptstr)/2,scriptstr);
            if ( myGetTransaction(prevhash,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
            {
                vout.nValue = vintx.vout[0].nValue - txfee;
                vout.scriptPubKey = scriptPubKey;
                musig_prevoutmsg(msg,prevhash,vout.scriptPubKey);
                sig.resize(32+CRYPTO_BYTES);
                if ( dilithium_sendopretdecode(destpubtxid,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
                {
                    if ( dilithium_bigpubget(handle,destpub33,pk2,destpubtxid) < 0 )
                        retstr = (char *)"couldnt get bigpub";
                    else if ( memcmp(pk,pk2,sizeof(pk)) != 0 )
                        retstr = (char *)"dilithium bigpub mismatch";
                    else if ( destpub33 != mypk )
                        retstr = (char *)"destpub33 is not for this -pubkey";
                    else if ( _dilithium_sign(&sig[0],&smlen,msg,32,sk) < 0 )
                        retstr = (char *)"dilithium signing error";
                    else if ( smlen != 32+CRYPTO_BYTES )
                        retstr = (char *)"siglen error";
                    else
                    {
                        mtx.vin.push_back(CTxIn(prevhash,0));
                        mtx.vout.push_back(vout);
                        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,dilithium_spendopret(destpubtxid,sig));
                        memset(seed,0,32);
                        memset(sk,0,sizeof(sk));
                        return(dilithium_rawtxresult(result,rawtx));
                    }
                } else retstr = (char *)"couldnt decode send opret";
            } else retstr = (char *)"couldnt find vin0";
        } else retstr = (char *)"script or bad destpubtxid is not hex";
    } else retstr = (char *)"need to have exactly 2 params sendtxid, scriptPubKey";
    memset(seed,0,32);
    memset(sk,0,sizeof(sk));
    return(cclib_error(result,retstr));
}

int64_t dilithium_inputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint256 destpubtxid,int64_t total,int32_t maxinputs,char *cmpaddr)
{
    char coinaddr[64]; int64_t threshold,nValue,price,totalinputs = 0; uint256 checktxid,txid,hashBlock; std::vector<uint8_t> origpubkey,tmpsig; CTransaction vintx; int32_t vout,numvouts,n = 0; std::vector<uint256> voutpubtxids;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //char str[65]; fprintf(stderr,"%s check %s/v%d %.8f vs %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN,(double)threshold/COIN);
        if ( it->second.satoshis < threshold || it->second.satoshis == DILITHIUM_TXFEE )
            continue;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
        {
            if ( (nValue= IsCClibvout(cp,vintx,vout,cmpaddr)) > DILITHIUM_TXFEE && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                if ( (dilithium_Qsendopretdecode(checktxid,tmpsig,voutpubtxids,vintx.vout[numvouts-1].scriptPubKey) == 'Q' && vout < voutpubtxids.size() && destpubtxid == voutpubtxids[vout]) || (dilithium_sendopretdecode(checktxid,vintx.vout[numvouts-1].scriptPubKey) == 'x' && destpubtxid == checktxid) )
                {
                    if ( total != 0 && maxinputs != 0 )
                        mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    nValue = it->second.satoshis;
                    totalinputs += nValue;
                    n++;
                    if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                        break;
                }
            } //else fprintf(stderr,"nValue %.8f too small or already spent in mempool\n",(double)nValue/COIN);
        } else fprintf(stderr,"couldnt get tx\n");
    }
    return(totalinputs);
}

UniValue dilithium_Qsend(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); std::string rawtx; CPubKey mypk,destpub33; CTransaction tx,vintx; uint256 prevhash,mypubtxid,hashBlock,destpubtxid; int64_t amount,inputsum,outputsum,change; int32_t i,smlen,n,numvouts; char str[129],myCCaddr[64],*scriptstr,*retstr=""; CTxOut vout; std::string handle; uint8_t pk[CRYPTO_PUBLICKEYBYTES],pk2[CRYPTO_PUBLICKEYBYTES],sk[CRYPTO_SECRETKEYBYTES],msg[32],seed[32]; std::vector<uint8_t> sig; std::vector<uint256> voutpubtxids;
    if ( txfee == 0 )
        txfee = DILITHIUM_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    GetCCaddress(cp,myCCaddr,mypk);
    if ( params != 0 && (n= cJSON_GetArraySize(params)) >= 4 && (n & 1) == 0 )
    {
        mypubtxid = juint256(jitem(params,0));
        if ( cclib_parsehash(seed,jitem(params,1),32) < 0 )
        {
            Myprivkey(seed);
            result.push_back(Pair("warning","test mode using privkey for -pubkey, only for testing. there is no point using quantum secure signing if you are using a privkey with a known secp256k1 pubkey!!"));
        }
        _dilithium_keypair(pk,sk,seed);
        outputsum = 0;
        for (i=2; i<n; i+=2)
        {
            amount = jdouble(jitem(params,i+1),0)*COIN + 0.0000000049;
            scriptstr = jstr(jitem(params,i),0);
            if ( is_hexstr(scriptstr,0) == 64 )
            {
                prevhash = juint256(jitem(params,i));
                if ( dilithium_bigpubget(handle,destpub33,pk2,prevhash) < 0 )
                {
                    result.push_back(Pair("destpubtxid",prevhash.GetHex().c_str()));
                    memset(seed,0,32);
                    memset(sk,0,sizeof(sk));
                    return(cclib_error(result,"couldnt find bigpub at destpubtxid"));
                }
                else
                {
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,destpub33));
                    voutpubtxids.push_back(prevhash); // binds destpub22 CC addr with dilithium bigpub
                }
            }
            else
            {
                CScript scriptPubKey;
                scriptPubKey.resize(strlen(scriptstr)/2);
                decode_hex(&scriptPubKey[0],strlen(scriptstr)/2,scriptstr);
                vout.nValue = amount;
                vout.scriptPubKey = scriptPubKey;
                mtx.vout.push_back(vout);
                voutpubtxids.push_back(zeroid);
            }
            outputsum += amount;
        }
        if ( (inputsum= dilithium_inputs(cp,mtx,mypk,mypubtxid,outputsum+txfee,64,myCCaddr)) >= outputsum+txfee )
        {
            change = (inputsum - outputsum - txfee);
            if ( change >= txfee )
            {
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,change,mypk));
                voutpubtxids.push_back(mypubtxid);
            }
            tx = mtx;
            dilithium_Qmsghash(msg,tx,(int32_t)voutpubtxids.size(),voutpubtxids);
            //for (i=0; i<32; i++)
            //    fprintf(stderr,"%02x",msg[i]);
            //fprintf(stderr," msg\n");
            sig.resize(32+CRYPTO_BYTES);
            if ( dilithium_bigpubget(handle,destpub33,pk2,mypubtxid) < 0 )
                retstr = (char *)"couldnt get bigpub";
            else if ( memcmp(pk,pk2,sizeof(pk)) != 0 )
                retstr = (char *)"dilithium bigpub mismatch";
            else if ( destpub33 != mypk )
                retstr = (char *)"destpub33 is not for this -pubkey";
            else if ( _dilithium_sign(&sig[0],&smlen,msg,32,sk) < 0 )
                retstr = (char *)"dilithium signing error";
            else if ( smlen != 32+CRYPTO_BYTES )
                retstr = (char *)"siglen error";
            else
            {
                rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,dilithium_Qsendopret(mypubtxid,sig,voutpubtxids));
                memset(seed,0,32);
                memset(sk,0,sizeof(sk));
                return(dilithium_rawtxresult(result,rawtx));
            }
        } else retstr = (char *)"Q couldnt find enough Q or x inputs";
    } else retstr = (char *)"need to have exactly 2 params sendtxid, scriptPubKey";
    memset(seed,0,32);
    memset(sk,0,sizeof(sk));
    return(cclib_error(result,retstr));
}

bool dilithium_Qvalidate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    int32_t i,numvins,numvouts,mlen,smlen=CRYPTO_BYTES+32; CPubKey destpub33; std::string handle; uint256 tmptxid,hashBlock,destpubtxid,signerpubtxid; CTransaction vintx; std::vector<uint8_t> tmpsig,sig,vopret; uint8_t msg[32],msg2[CRYPTO_BYTES+32],pk[CRYPTO_PUBLICKEYBYTES],*script; std::vector<uint256> voutpubtxids;
    numvins = tx.vin.size();
    signerpubtxid = zeroid;
    for (i=0; i<numvins; i++)
    {
        if ( IsCCInput(tx.vin[i].scriptSig) != 0 )
        {
            if ( myGetTransaction(tx.vin[i].prevout.hash,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
            {
                GetOpReturnData(vintx.vout[numvouts-1].scriptPubKey,vopret);
                script = (uint8_t *)vopret.data();
                if ( script[1] == 'Q' )
                {
                    if ( dilithium_Qsendopretdecode(tmptxid,tmpsig,voutpubtxids,vintx.vout[numvouts-1].scriptPubKey) != 'Q' )
                        return eval->Invalid("couldnt decode destpubtxid from Qsend");
                    else if ( tx.vin[i].prevout.n > voutpubtxids.size() )
                        return eval->Invalid("no destpubtxid for prevout.n");
                    destpubtxid = voutpubtxids[tx.vin[i].prevout.n];
                }
                else
                {
                    if ( dilithium_sendopretdecode(destpubtxid,vintx.vout[numvouts-1].scriptPubKey) != 'x' )
                        return eval->Invalid("couldnt decode destpubtxid from send");
                }
                if ( signerpubtxid == zeroid )
                    signerpubtxid = destpubtxid;
                else if ( destpubtxid != signerpubtxid )
                    return eval->Invalid("destpubtxid of vini doesnt match first one");
            }
        }
    }
    if ( signerpubtxid != zeroid )
    {
        numvouts = tx.vout.size();
        if ( dilithium_Qsendopretdecode(destpubtxid,sig,voutpubtxids,tx.vout[numvouts-1].scriptPubKey) == 'Q' && destpubtxid == signerpubtxid && sig.size() == smlen )
        {
            if ( dilithium_Qmsghash(msg,tx,numvouts-1,voutpubtxids) < 0 )
                return eval->Invalid("couldnt get Qmsghash");
            else if ( dilithium_bigpubget(handle,destpub33,pk,signerpubtxid) < 0 )
                return eval->Invalid("couldnt get bigpub");
            else
            {
                if ( _dilithium_verify(msg2,&mlen,&sig[0],smlen,pk) < 0 )
                    return eval->Invalid("failed dilithium verify");
                else if ( mlen != 32 || memcmp(msg,msg2,32) != 0 )
                {
                    for (i=0; i<32; i++)
                        fprintf(stderr,"%02x",msg[i]);
                    fprintf(stderr," vs ");
                    for (i=0; i<mlen; i++)
                        fprintf(stderr,"%02x",msg2[i]);
                    fprintf(stderr,"mlen.%d\n",mlen);
                    return eval->Invalid("failed dilithium msg verify");
                }
                else return true;
            }
        } else return eval->Invalid("failed decode Qsend");
    } else return eval->Invalid("unexpected zero signerpubtxid");
}

int32_t dilithium_registrationpub33(char *pkaddr,CPubKey &pub33,uint256 txid)
{
    std::string handle; std::vector<uint8_t> bigpub; CTransaction tx; uint256 hashBlock; int32_t numvouts;
    pkaddr[0] = 0;
    if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
    {
        if ( dilithium_registeropretdecode(handle,pub33,bigpub,tx.vout[numvouts-1].scriptPubKey) == 'R' )
        {
            dilithium_addr(pkaddr,&bigpub[0],CRYPTO_PUBLICKEYBYTES);
            return(0);
        }
    }
    return(-1);
}

void dilithium_handleinit(struct CCcontract_info *cp)
{
    static int32_t didinit;
    std::vector<uint256> txids; struct dilithium_handle *hashstr; CPubKey dilithiumpk,pub33; uint256 txid,hashBlock; CTransaction txi; int32_t numvouts; std::vector<uint8_t> bigpub; std::string handle; char CCaddr[64];
    if ( didinit != 0 )
        return;
    pthread_mutex_init(&DILITHIUM_MUTEX,NULL);
    dilithiumpk = GetUnspendable(cp,0);
    GetCCaddress(cp,CCaddr,dilithiumpk);
    SetCCtxids(txids,CCaddr,true,cp->evalcode,zeroid,'R');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,txi,hashBlock) != 0 && (numvouts= txi.vout.size()) > 1 )
        {
            if ( dilithium_registeropretdecode(handle,pub33,bigpub,txi.vout[numvouts-1].scriptPubKey) == 'R' )
            {
                if ( (hashstr= dilithium_handlenew((char *)handle.c_str())) != 0 )
                {
                    if ( hashstr->destpubtxid != txid )
                    {
                        if ( hashstr->destpubtxid != zeroid )
                            fprintf(stderr,"overwriting %s %s with %s\n",handle.c_str(),hashstr->destpubtxid.GetHex().c_str(),txid.GetHex().c_str());
                        fprintf(stderr,"%s <- %s\n",handle.c_str(),txid.GetHex().c_str());
                        hashstr->destpubtxid = txid;
                    }
                }
            }
        }
    }
    didinit = 1;
}

UniValue dilithium_handleinfo(uint64_t txfee,struct CCcontract_info *cp,cJSON *params)
{
    UniValue result(UniValue::VOBJ); CPubKey pub33; int32_t i,n; char *handlestr,pkaddr[64],str[67]; struct dilithium_handle *hashstr;
    if ( params != 0 && (n= cJSON_GetArraySize(params)) == 1 )
    {
        dilithium_handleinit(cp);
        if ( (handlestr= jstr(jitem(params,0),0)) != 0 )
        {
            result.push_back(Pair("result","success"));
            result.push_back(Pair("handle",handlestr));
            if ( (hashstr= dilithium_handlefind(handlestr)) != 0 )
            {
                result.push_back(Pair("destpubtxid",hashstr->destpubtxid.GetHex().c_str()));
                if ( dilithium_registrationpub33(pkaddr,pub33,hashstr->destpubtxid) == 0 )
                {
                    for (i=0; i<33; i++)
                        sprintf(&str[i<<1],"%02x",((uint8_t *)pub33.begin())[i]);
                    str[i<<1] = 0;
                    result.push_back(Pair("pkaddr",pkaddr));
                }
                result.push_back(Pair("pubkey",str));
            } else result.push_back(Pair("status","available"));
            return(result);
        }
    }
    result.push_back(Pair("result","error"));
    return(result);
}

bool dilithium_Rvalidate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    static int32_t didinit;
    uint256 txid; int32_t numvouts; struct dilithium_handle *hashstr; std::string handle; std::vector<uint8_t> bigpub; CPubKey oldpub33,pub33,dilithiumpk; CTxOut vout,vout0; char pkaddr[64];
    if ( height < 14500 )
        return(true);
    dilithium_handleinit(cp);
    dilithiumpk = GetUnspendable(cp,0);
    if ( (numvouts= tx.vout.size()) <= 1 )
        return eval->Invalid("not enough vouts for registration tx");
    else if ( dilithium_registeropretdecode(handle,pub33,bigpub,tx.vout[numvouts-1].scriptPubKey) == 'R' )
    {
        // relies on all current block tx to be put into mempool
        txid = tx.GetHash();
        vout0 = MakeCC1vout(cp->evalcode,2*DILITHIUM_TXFEE,dilithiumpk);
        vout = MakeCC1vout(EVAL_FAUCET,COIN,Faucet_pubkeyget());
        if ( tx.vout[0] != vout0 )
            return eval->Invalid("mismatched vout0 for register");
        else if ( tx.vout[1].nValue != DILITHIUM_TXFEE )
            return eval->Invalid("vout1 for register not txfee");
        else if ( tx.vout[2] != vout )
            return eval->Invalid("register not sending to faucet");
        else if ( (hashstr= dilithium_handlenew((char *)handle.c_str())) == 0 )
            return eval->Invalid("error creating dilithium handle");
        else if ( hashstr->destpubtxid == zeroid )
        {
            hashstr->destpubtxid = txid;
            return(true);
        }
        else
        {
            if ( hashstr->destpubtxid == txid )
                return(true);
            else if ( dilithium_registrationpub33(pkaddr,oldpub33,hashstr->destpubtxid) == 0 )
            {
                if ( oldpub33 == pub33 )
                {
                    hashstr->destpubtxid = txid;
                    fprintf(stderr,"ht.%d %s <- %s\n",height,handle.c_str(),txid.GetHex().c_str());
                    return(true);
                }
            }
            return eval->Invalid("duplicate dilithium handle rejected");
        }
    } else return eval->Invalid("couldnt decode register opret");
}

bool dilithium_validate(struct CCcontract_info *cp,int32_t height,Eval *eval,const CTransaction tx)
{
    CPubKey destpub33; std::string handle; uint256 hashBlock,destpubtxid,checktxid; CTransaction vintx; int32_t numvouts,mlen,smlen=CRYPTO_BYTES+32; std::vector<uint8_t> sig,vopret; uint8_t msg[32],msg2[CRYPTO_BYTES+32],pk[CRYPTO_PUBLICKEYBYTES],*script;
    // if all dilithium tx -> do multispend/send, else:
    numvouts = tx.vout.size();
    GetOpReturnData(tx.vout[numvouts-1].scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( script[1] == 'R' )
        return(dilithium_Rvalidate(cp,height,eval,tx));
    else if ( script[1] == 'Q' )
        return(dilithium_Qvalidate(cp,height,eval,tx));
    else if ( tx.vout.size() != 2 )
        return eval->Invalid("numvouts != 2");
    else if ( tx.vin.size() != 1 )
        return eval->Invalid("numvins != 1");
    else if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
        return eval->Invalid("illegal normal vin0");
    else if ( myGetTransaction(tx.vin[0].prevout.hash,vintx,hashBlock) != 0 && (numvouts= vintx.vout.size()) > 1 )
    {
        if ( dilithium_sendopretdecode(destpubtxid,vintx.vout[numvouts-1].scriptPubKey) == 'x' )
        {
            if ( dilithium_spendopretdecode(checktxid,sig,tx.vout[tx.vout.size()-1].scriptPubKey) == 'y' )
            {
                if ( destpubtxid == checktxid && sig.size() == smlen )
                {
                    musig_prevoutmsg(msg,tx.vin[0].prevout.hash,tx.vout[0].scriptPubKey);
                    if ( dilithium_bigpubget(handle,destpub33,pk,destpubtxid) < 0 )
                        return eval->Invalid("couldnt get bigpub");
                    else
                    {
                        if ( _dilithium_verify(msg2,&mlen,&sig[0],smlen,pk) < 0 )
                            return eval->Invalid("failed dilithium verify");
                        else if ( mlen != 32 || memcmp(msg,msg2,32) != 0 )
                            return eval->Invalid("failed dilithium msg verify");
                        else return(true);
                    }
                } else return eval->Invalid("destpubtxid or sig size didnt match send opret");
            } else return eval->Invalid("failed decode dilithium spendopret");
        } else return eval->Invalid("couldnt decode send opret");
    } else return eval->Invalid("couldnt find vin0 tx");
}

