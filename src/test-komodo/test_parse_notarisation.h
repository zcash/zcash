#include "komodo_structs.h"
#include <vector>

/***
 * @brief read a binary file to get a long list of notarized_checkpoints for testing
 * @param filename the file to read
 * @returns a big vector
 */
std::vector<notarized_checkpoint> get_test_checkpoints_from_file(const std::string& filename);
