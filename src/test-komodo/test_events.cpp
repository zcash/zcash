#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <komodo_structs.h>

int32_t komodo_faststateinit(struct komodo_state *sp,char *fname,char *symbol,char *dest);
struct komodo_state *komodo_stateptrget(char *base);
extern int32_t KOMODO_EXTERNAL_NOTARIES;

namespace TestEvents {

void write_p_record(std::FILE* fp)
{
    // a pubkey record with 2 keys
    char data[5+1+(33*2)] = {'P', 1, 0, 0, 0, 2}; // public key record identifier with height of 1 plus number of keys plus keys themselves
    memset(&data[6], 1, 33); // 1st key is all 1s
    memset(&data[39], 2, 33); // 2nd key is all 2s
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_n_record(std::FILE* fp)
{
    // a notarized record has
    // 5 byte header 
    // 4 bytes notarized_height
    // 32 bytes notarized hash( blockhash)
    // 32 bytes desttxid
    char data[73] = {'N', 1, 0, 0, 0, 2, 0, 0, 0};
    memset(&data[9], 1, 32); // notarized hash
    memset(&data[41], 2, 32); // desttxid
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_m_record(std::FILE* fp)
{
    // a notarized M record has
    // 5 byte header 
    // 4 bytes notarized_height
    // 32 bytes notarized hash( blockhash)
    // 32 bytes desttxid
    // 32 bytes MoM
    // 4 bytes MoMdepth
    char data[109] = {'M', 1, 0, 0, 0, 3, 0, 0, 0};
    memset(&data[9], 1, 32); // notarized hash
    memset(&data[41], 2, 32); // desttxid
    memset(&data[73], 3, 32); // MoM
    memset(&data[105], 4, 4); // MoMdepth
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_u_record(std::FILE* fp)
{
    // a U record has
    // 5 byte header 
    // 1 byte n and 1 byte nid
    // 8 bytes mask
    // 32 bytes hash
    char data[47] = {'U', 1, 0, 0, 0, 'N', 'I'};
    memset(&data[7], 1, 8); // mask
    memset(&data[15], 2, 32); // hash
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_k_record(std::FILE* fp)
{
    // a K record has
    // 5 byte header 
    // 4 bytes height
    char data[9] = {'K', 1, 0, 0, 0 };
    memset(&data[5], 1, 4); // height
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_t_record(std::FILE* fp)
{
    // a T record has
    // 5 byte header 
    // 4 bytes height
    // 4 bytes timestamp
    char data[13] = {'T', 1, 0, 0, 0 };
    memset(&data[5], 1, 4); // height
    memset(&data[9], 2, 4); // timestamp
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_r_record(std::FILE* fp)
{
    // an R record has
    // 5 byte header 
    // 32 byte txid
    // 2 byte v
    // 8 byte ovalue
    // 2 byte olen
    // olen bytes of data
    char data[65583] = {'R', 1, 0, 0, 0 };
    memset(&data[5], 1, 32); // txid
    memset(&data[37], 2, 2); // v
    memset(&data[39], 3, 8); // ovalue
    unsigned char olen[2] = {254, 255}; // 65534
    memcpy(&data[47], olen, 2);
    memset(&data[49], 4, 65534);
    std::fwrite(data, sizeof(data), 1, fp);
}
void write_v_record(std::FILE* fp)
{
    // a V record has
    // 5 byte header 
    // 1 byte counter
    // counter * 4 bytes of data
    char data[146] = {'V', 1, 0, 0, 0};
    memset(&data[5], 35, 1); // counter
    memset(&data[6], 1, 140); // data
    std::fwrite(data, sizeof(data), 1, fp);
}

/****
 * The main purpose of this test is to verify that
 * state files created continue to be readable despite logic
 * changes. Files need to be readable. The record format should 
 * not change without hardfork protection
 */
TEST(TestEvents, komodo_faststateinit_test)
{
    strcpy(ASSETCHAINS_SYMBOL, "TST");
    KOMODO_EXTERNAL_NOTARIES = 1;

    boost::filesystem::path temp = boost::filesystem::unique_path();
    boost::filesystem::create_directories(temp);
    try
    {
        // NOTE: header contains a 5 byte header that is make up of
        // an 8 bit identifier (i.e. 'P', 'N', etc.)
        // plus a 32 bit height. Everything else is record specific
        // pubkey record
        {
            // create a binary file that should be readable by komodo
            const std::string temp_filename = temp.native() + "/kstate.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_p_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = nullptr;
            // attempt to read the file
            int32_t result = komodo_faststateinit( state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            // The first and second event should be pub keys
            ASSERT_EQ(state->Komodo_numevents, 1);
            komodo_event* ev1 = state->Komodo_events[0];
            ASSERT_EQ(ev1->height, 1);
            ASSERT_EQ(ev1->type, 'P');
        }
        // notarized record
        {
            const std::string temp_filename = temp.native() + "/notarized.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_n_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit( state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 2);
            komodo_event* ev = state->Komodo_events[1];
            ASSERT_EQ(ev->height, 1);
            ASSERT_EQ(ev->type, 'N');
        }
        // notarized M record
        {
            const std::string temp_filename = temp.native() + "/notarized.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_m_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit(state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 3);
            komodo_event* ev = state->Komodo_events[2];
            ASSERT_EQ(ev->height, 1);
            ASSERT_EQ(ev->type, 'N'); // code converts "M" to "N"
        }
        // record type "U" (deprecated)
        {
            const std::string temp_filename = temp.native() + "/type_u.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_u_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit(state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 3); // does not get added to state
        }
        // record type K (KMD height)
        {
            const std::string temp_filename = temp.native() + "/kmdtype.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_k_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit(state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 4);
            komodo_event* ev = state->Komodo_events[3];
            ASSERT_EQ(ev->height, 1);
            ASSERT_EQ(ev->type, 'K');            
        }
        // record type T (KMD height with timestamp)
        {
            const std::string temp_filename = temp.native() + "/kmdtypet.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_t_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit(state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 5);
            komodo_event* ev = state->Komodo_events[4];
            ASSERT_EQ(ev->height, 1);
            ASSERT_EQ(ev->type, 'K'); // changed from T to K
        }
        // record type R (opreturn)
        {
            const std::string temp_filename = temp.native() + "/kmdtypet.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_r_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit(state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 6);
            komodo_event* ev = state->Komodo_events[5];
            ASSERT_EQ(ev->height, 1);
            ASSERT_EQ(ev->type, 'R');
        }
        // record type V
        {
            const std::string temp_filename = temp.native() + "/kmdtypet.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_v_record(fp);
            std::fclose(fp);
            // verify file still exists
            ASSERT_TRUE(boost::filesystem::exists(full_filename));
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit(state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 7);
            komodo_event* ev = state->Komodo_events[6];
            ASSERT_EQ(ev->height, 1);
            ASSERT_EQ(ev->type, 'V');
        }
        // all together in 1 file
        {
            const std::string temp_filename = temp.native() + "/combined_state.tmp";
            char full_filename[temp_filename.size()+1];
            strcpy(full_filename, temp_filename.c_str());
            std::FILE* fp = std::fopen(full_filename, "wb+");
            ASSERT_NE(fp, nullptr);
            write_p_record(fp);
            write_n_record(fp);
            write_m_record(fp);
            write_u_record(fp);
            write_k_record(fp);
            write_t_record(fp);
            write_r_record(fp);
            write_v_record(fp);
            std::fclose(fp);
            // attempt to read the file
            char symbol[] = "TST";
            komodo_state* state = komodo_stateptrget((char*)symbol);
            ASSERT_NE(state, nullptr);
            char* dest = (char*)"123456789012345";
            // attempt to read the file
            int32_t result = komodo_faststateinit( state, full_filename, symbol, dest);
            // compare results
            ASSERT_EQ(result, 1);
            ASSERT_EQ(state->Komodo_numevents, 14);
            komodo_event* ev1 = state->Komodo_events[7];
            ASSERT_EQ(ev1->height, 1);
            ASSERT_EQ(ev1->type, 'P');
        }
    } 
    catch(...)
    {
        FAIL() << "Exception thrown";
    }
    boost::filesystem::remove_all(temp);
}

} // namespace TestEvents