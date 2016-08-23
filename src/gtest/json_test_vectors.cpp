#include "json_test_vectors.h"

Array
read_json(const std::string& jsondata)
{
    Value v;

    if (!read_string(jsondata, v) || v.type() != array_type)
    {
        ADD_FAILURE();
        return Array();
    }
    return v.get_array();
}
