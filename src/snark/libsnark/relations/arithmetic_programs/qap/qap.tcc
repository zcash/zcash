/** @file
*****************************************************************************

Implementation of interfaces for a QAP ("Quadratic Arithmetic Program").

See qap.hpp .

*****************************************************************************
* @author     This file is part of libsnark, developed by SCIPR Lab
*             and contributors (see AUTHORS).
* @copyright  MIT license (see LICENSE file)
*****************************************************************************/

#ifndef QAP_TCC_
#define QAP_TCC_

#include "common/profiling.hpp"
#include "common/utils.hpp"
#include "algebra/evaluation_domain/evaluation_domain.hpp"
#include "algebra/scalar_multiplication/multiexp.hpp"

namespace libsnark {

template<typename FieldT>
qap_instance<FieldT>::qap_instance(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                   const size_t num_variables,
                                   const size_t degree,
                                   const size_t num_inputs,
                                   const std::vector<std::map<size_t, FieldT> > &A_in_Lagrange_basis,
                                   const std::vector<std::map<size_t, FieldT> > &B_in_Lagrange_basis,
                                   const std::vector<std::map<size_t, FieldT> > &C_in_Lagrange_basis) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    A_in_Lagrange_basis(A_in_Lagrange_basis),
    B_in_Lagrange_basis(B_in_Lagrange_basis),
    C_in_Lagrange_basis(C_in_Lagrange_basis)
{
}

template<typename FieldT>
qap_instance<FieldT>::qap_instance(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                   const size_t num_variables,
                                   const size_t degree,
                                   const size_t num_inputs,
                                   std::vector<std::map<size_t, FieldT> > &&A_in_Lagrange_basis,
                                   std::vector<std::map<size_t, FieldT> > &&B_in_Lagrange_basis,
                                   std::vector<std::map<size_t, FieldT> > &&C_in_Lagrange_basis) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    A_in_Lagrange_basis(std::move(A_in_Lagrange_basis)),
    B_in_Lagrange_basis(std::move(B_in_Lagrange_basis)),
    C_in_Lagrange_basis(std::move(C_in_Lagrange_basis))
{
}

template<typename FieldT>
size_t qap_instance<FieldT>::num_variables() const
{
    return num_variables_;
}

template<typename FieldT>
size_t qap_instance<FieldT>::degree() const
{
    return degree_;
}

template<typename FieldT>
size_t qap_instance<FieldT>::num_inputs() const
{
    return num_inputs_;
}

template<typename FieldT>
bool qap_instance<FieldT>::is_satisfied(const qap_witness<FieldT> &witness) const
{
    const FieldT t = FieldT::random_element();

    std::vector<FieldT> At(this->num_variables()+1, FieldT::zero());
    std::vector<FieldT> Bt(this->num_variables()+1, FieldT::zero());
    std::vector<FieldT> Ct(this->num_variables()+1, FieldT::zero());
    std::vector<FieldT> Ht(this->degree()+1);

    const FieldT Zt = this->domain->compute_Z(t);

    const std::vector<FieldT> u = this->domain->lagrange_coeffs(t);

    for (size_t i = 0; i < this->num_variables()+1; ++i)
    {
        for (auto &el : A_in_Lagrange_basis[i])
        {
            At[i] += u[el.first] * el.second;
        }

        for (auto &el : B_in_Lagrange_basis[i])
        {
            Bt[i] += u[el.first] * el.second;
        }

        for (auto &el : C_in_Lagrange_basis[i])
        {
            Ct[i] += u[el.first] * el.second;
        }
    }

    FieldT ti = FieldT::one();
    for (size_t i = 0; i < this->degree()+1; ++i)
    {
        Ht[i] = ti;
        ti *= t;
    }

    const qap_instance_evaluation<FieldT> eval_qap_inst(this->domain,
                                                        this->num_variables(),
                                                        this->degree(),
                                                        this->num_inputs(),
                                                        t,
                                                        std::move(At),
                                                        std::move(Bt),
                                                        std::move(Ct),
                                                        std::move(Ht),
                                                        Zt);
    return eval_qap_inst.is_satisfied(witness);
}

template<typename FieldT>
qap_instance_evaluation<FieldT>::qap_instance_evaluation(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                                         const size_t num_variables,
                                                         const size_t degree,
                                                         const size_t num_inputs,
                                                         const FieldT &t,
                                                         const std::vector<FieldT> &At,
                                                         const std::vector<FieldT> &Bt,
                                                         const std::vector<FieldT> &Ct,
                                                         const std::vector<FieldT> &Ht,
                                                         const FieldT &Zt) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    t(t),
    At(At),
    Bt(Bt),
    Ct(Ct),
    Ht(Ht),
    Zt(Zt)
{
}

template<typename FieldT>
qap_instance_evaluation<FieldT>::qap_instance_evaluation(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                                         const size_t num_variables,
                                                         const size_t degree,
                                                         const size_t num_inputs,
                                                         const FieldT &t,
                                                         std::vector<FieldT> &&At,
                                                         std::vector<FieldT> &&Bt,
                                                         std::vector<FieldT> &&Ct,
                                                         std::vector<FieldT> &&Ht,
                                                         const FieldT &Zt) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    t(t),
    At(std::move(At)),
    Bt(std::move(Bt)),
    Ct(std::move(Ct)),
    Ht(std::move(Ht)),
    Zt(Zt)
{
}

template<typename FieldT>
size_t qap_instance_evaluation<FieldT>::num_variables() const
{
    return num_variables_;
}

template<typename FieldT>
size_t qap_instance_evaluation<FieldT>::degree() const
{
    return degree_;
}

template<typename FieldT>
size_t qap_instance_evaluation<FieldT>::num_inputs() const
{
    return num_inputs_;
}

template<typename FieldT>
bool qap_instance_evaluation<FieldT>::is_satisfied(const qap_witness<FieldT> &witness) const
{

    if (this->num_variables() != witness.num_variables())
    {
        return false;
    }

    if (this->degree() != witness.degree())
    {
        return false;
    }

    if (this->num_inputs() != witness.num_inputs())
    {
        return false;
    }

    if (this->num_variables() != witness.coefficients_for_ABCs.size())
    {
        return false;
    }

    if (this->degree()+1 != witness.coefficients_for_H.size())
    {
        return false;
    }

    if (this->At.size() != this->num_variables()+1 || this->Bt.size() != this->num_variables()+1 || this->Ct.size() != this->num_variables()+1)
    {
        return false;
    }

    if (this->Ht.size() != this->degree()+1)
    {
        return false;
    }

    if (this->Zt != this->domain->compute_Z(this->t))
    {
        return false;
    }

    FieldT ans_A = this->At[0] + witness.d1*this->Zt;
    FieldT ans_B = this->Bt[0] + witness.d2*this->Zt;
    FieldT ans_C = this->Ct[0] + witness.d3*this->Zt;
    FieldT ans_H = FieldT::zero();

    ans_A = ans_A + naive_plain_exp<FieldT, FieldT>(this->At.begin()+1, this->At.begin()+1+this->num_variables(),
                                                    witness.coefficients_for_ABCs.begin(), witness.coefficients_for_ABCs.begin()+this->num_variables());
    ans_B = ans_B + naive_plain_exp<FieldT, FieldT>(this->Bt.begin()+1, this->Bt.begin()+1+this->num_variables(),
                                                    witness.coefficients_for_ABCs.begin(), witness.coefficients_for_ABCs.begin()+this->num_variables());
    ans_C = ans_C + naive_plain_exp<FieldT, FieldT>(this->Ct.begin()+1, this->Ct.begin()+1+this->num_variables(),
                                                    witness.coefficients_for_ABCs.begin(), witness.coefficients_for_ABCs.begin()+this->num_variables());
    ans_H = ans_H + naive_plain_exp<FieldT, FieldT>(this->Ht.begin(), this->Ht.begin()+this->degree()+1,
                                                    witness.coefficients_for_H.begin(), witness.coefficients_for_H.begin()+this->degree()+1);

    if (ans_A * ans_B - ans_C != ans_H * this->Zt)
    {
        return false;
    }

    return true;
}

template<typename FieldT>
qap_witness<FieldT>::qap_witness(const size_t num_variables,
                                 const size_t degree,
                                 const size_t num_inputs,
                                 const FieldT &d1,
                                 const FieldT &d2,
                                 const FieldT &d3,
                                 const std::vector<FieldT> &coefficients_for_ABCs,
                                 const std::vector<FieldT> &coefficients_for_H) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    d1(d1),
    d2(d2),
    d3(d3),
    coefficients_for_ABCs(coefficients_for_ABCs),
    coefficients_for_H(coefficients_for_H)
{
}

template<typename FieldT>
qap_witness<FieldT>::qap_witness(const size_t num_variables,
                                 const size_t degree,
                                 const size_t num_inputs,
                                 const FieldT &d1,
                                 const FieldT &d2,
                                 const FieldT &d3,
                                 const std::vector<FieldT> &coefficients_for_ABCs,
                                 std::vector<FieldT> &&coefficients_for_H) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    d1(d1),
    d2(d2),
    d3(d3),
    coefficients_for_ABCs(coefficients_for_ABCs),
    coefficients_for_H(std::move(coefficients_for_H))
{
}


template<typename FieldT>
size_t qap_witness<FieldT>::num_variables() const
{
    return num_variables_;
}

template<typename FieldT>
size_t qap_witness<FieldT>::degree() const
{
    return degree_;
}

template<typename FieldT>
size_t qap_witness<FieldT>::num_inputs() const
{
    return num_inputs_;
}


} // libsnark

#endif // QAP_TCC_
