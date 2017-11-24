/** @file
 *****************************************************************************

 Declaration of interfaces for profiling constraints.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef CONSTRAINT_PROFILING_HPP_
#define CONSTRAINT_PROFILING_HPP_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace libsnark {

extern size_t constraint_profiling_indent;

struct constraint_profiling_entry {
    size_t indent;
    std::string annotation;
    size_t count;
};

extern std::vector<constraint_profiling_entry> constraint_profiling_table;

#define PROFILE_CONSTRAINTS(pb, annotation)                             \
    for (size_t _num_constraints_before = pb.num_constraints(), _iter = (++constraint_profiling_indent, 0), _cp_pos = constraint_profiling_table.size(); \
         _iter == 0;                                                    \
         constraint_profiling_table.insert(constraint_profiling_table.begin() + _cp_pos, constraint_profiling_entry{--constraint_profiling_indent, annotation, pb.num_constraints() - _num_constraints_before}), \
         _iter = 1)

size_t PRINT_CONSTRAINT_PROFILING(); // returns # of top level constraints

} // libsnark

#endif // CONSTRAINT_PROFILING_HPP_
