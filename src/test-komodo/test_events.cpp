#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <boost/filesystem.hpp>
//#define KOMODO_ZCASH
//#include <komodo.h>

struct komodo_state;
int32_t komodo_faststateinit(struct komodo_state *sp,char *fname,char *symbol,char *dest);
struct komodo_state *komodo_stateptrget(char *base);

namespace TestEvents {

TEST(TestEvents, komodo_faststateinit_test)
{
    // create a binary file that should be readable by komodo
    boost::filesystem::path temp = boost::filesystem::unique_path();
    boost::filesystem::create_directories(temp);
    const std::string temp_filename = temp.native() + "/kstate.tmp";
    char full_filename[temp_filename.size()+1];
    strcpy(full_filename, temp_filename.c_str());
    std::FILE* fp = std::fopen(full_filename, "wb+");
    ASSERT_NE(fp, nullptr);
    // a pubkey record with 2 keys
    char data[1+4+1+(33*2)] = {'P', 1, 0, 0, 0, 2}; // public key record identifier with height of 1 plus number of keys plus keys themselves
    memset(&data[6], 1, 33); // 1st key is all 1s
    memset(&data[39], 2, 33); // 2nd key is all 2s
    std::fwrite(data, sizeof(data), 1, fp);
    std::fclose(fp);
    // verify file still exists
    ASSERT_TRUE(boost::filesystem::exists(full_filename));
    // attempt to read the file
    char symbol[] = "KMD";
    komodo_state* state = komodo_stateptrget((char*)symbol);
    ASSERT_NE(state, nullptr);
    char* dest = nullptr;
    // attempt to read the file
    int32_t result = komodo_faststateinit( state, full_filename, symbol, dest);
    // compare results
    ASSERT_EQ(result, 1);
}

} // namespace TestEvents