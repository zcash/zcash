/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef GADGET_TCC_
#define GADGET_TCC_

namespace libsnark {

template<typename FieldT>
gadget<FieldT>::gadget(protoboard<FieldT> &pb, const std::string &annotation_prefix) :
    pb(pb), annotation_prefix(annotation_prefix)
{
#ifdef DEBUG
    assert(annotation_prefix != "");
#endif
}

} // libsnark
#endif // GADGET_TCC_
