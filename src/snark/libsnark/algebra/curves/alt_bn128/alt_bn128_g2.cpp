/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_g2.hpp"

namespace libsnark {

#ifdef PROFILE_OP_COUNTS
int64_t alt_bn128_G2::add_cnt = 0;
int64_t alt_bn128_G2::dbl_cnt = 0;
#endif

std::vector<size_t> alt_bn128_G2::wnaf_window_table;
std::vector<size_t> alt_bn128_G2::fixed_base_exp_window_table;
alt_bn128_G2 alt_bn128_G2::G2_zero;
alt_bn128_G2 alt_bn128_G2::G2_one;

alt_bn128_G2::alt_bn128_G2()
{
    this->X = G2_zero.X;
    this->Y = G2_zero.Y;
    this->Z = G2_zero.Z;
}

alt_bn128_Fq2 alt_bn128_G2::mul_by_b(const alt_bn128_Fq2 &elt)
{
    return alt_bn128_Fq2(alt_bn128_twist_mul_by_b_c0 * elt.c0, alt_bn128_twist_mul_by_b_c1 * elt.c1);
}

void alt_bn128_G2::print() const
{
    if (this->is_zero())
    {
        printf("O\n");
    }
    else
    {
        alt_bn128_G2 copy(*this);
        copy.to_affine_coordinates();
        gmp_printf("(%Nd*z + %Nd , %Nd*z + %Nd)\n",
                   copy.X.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.X.c0.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.Y.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.Y.c0.as_bigint().data, alt_bn128_Fq::num_limbs);
    }
}

void alt_bn128_G2::print_coordinates() const
{
    if (this->is_zero())
    {
        printf("O\n");
    }
    else
    {
        gmp_printf("(%Nd*z + %Nd : %Nd*z + %Nd : %Nd*z + %Nd)\n",
                   this->X.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->X.c0.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->Y.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->Y.c0.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->Z.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->Z.c0.as_bigint().data, alt_bn128_Fq::num_limbs);
    }
}

void alt_bn128_G2::to_affine_coordinates()
{
    if (this->is_zero())
    {
        this->X = alt_bn128_Fq2::zero();
        this->Y = alt_bn128_Fq2::one();
        this->Z = alt_bn128_Fq2::zero();
    }
    else
    {
        alt_bn128_Fq2 Z_inv = Z.inverse();
        alt_bn128_Fq2 Z2_inv = Z_inv.squared();
        alt_bn128_Fq2 Z3_inv = Z2_inv * Z_inv;
        this->X = this->X * Z2_inv;
        this->Y = this->Y * Z3_inv;
        this->Z = alt_bn128_Fq2::one();
    }
}

void alt_bn128_G2::to_special()
{
    this->to_affine_coordinates();
}

bool alt_bn128_G2::is_special() const
{
    return (this->is_zero() || this->Z == alt_bn128_Fq2::one());
}

bool alt_bn128_G2::is_zero() const
{
    return (this->Z.is_zero());
}

bool alt_bn128_G2::operator==(const alt_bn128_G2 &other) const
{
    if (this->is_zero())
    {
        return other.is_zero();
    }

    if (other.is_zero())
    {
        return false;
    }

    /* now neither is O */

    // using Jacobian coordinates so:
    // (X1:Y1:Z1) = (X2:Y2:Z2)
    // iff
    // X1/Z1^2 == X2/Z2^2 and Y1/Z1^3 == Y2/Z2^3
    // iff
    // X1 * Z2^2 == X2 * Z1^2 and Y1 * Z2^3 == Y2 * Z1^3

    alt_bn128_Fq2 Z1_squared = (this->Z).squared();
    alt_bn128_Fq2 Z2_squared = (other.Z).squared();

    if ((this->X * Z2_squared) != (other.X * Z1_squared))
    {
        return false;
    }

    alt_bn128_Fq2 Z1_cubed = (this->Z) * Z1_squared;
    alt_bn128_Fq2 Z2_cubed = (other.Z) * Z2_squared;

    if ((this->Y * Z2_cubed) != (other.Y * Z1_cubed))
    {
        return false;
    }

    return true;
}

bool alt_bn128_G2::operator!=(const alt_bn128_G2& other) const
{
    return !(operator==(other));
}

alt_bn128_G2 alt_bn128_G2::operator+(const alt_bn128_G2 &other) const
{
    // handle special cases having to do with O
    if (this->is_zero())
    {
        return other;
    }

    if (other.is_zero())
    {
        return *this;
    }

    // no need to handle points of order 2,4
    // (they cannot exist in a prime-order subgroup)

    // check for doubling case

    // using Jacobian coordinates so:
    // (X1:Y1:Z1) = (X2:Y2:Z2)
    // iff
    // X1/Z1^2 == X2/Z2^2 and Y1/Z1^3 == Y2/Z2^3
    // iff
    // X1 * Z2^2 == X2 * Z1^2 and Y1 * Z2^3 == Y2 * Z1^3

    alt_bn128_Fq2 Z1Z1 = (this->Z).squared();
    alt_bn128_Fq2 Z2Z2 = (other.Z).squared();

    alt_bn128_Fq2 U1 = this->X * Z2Z2;
    alt_bn128_Fq2 U2 = other.X * Z1Z1;

    alt_bn128_Fq2 Z1_cubed = (this->Z) * Z1Z1;
    alt_bn128_Fq2 Z2_cubed = (other.Z) * Z2Z2;

    alt_bn128_Fq2 S1 = (this->Y) * Z2_cubed;      // S1 = Y1 * Z2 * Z2Z2
    alt_bn128_Fq2 S2 = (other.Y) * Z1_cubed;      // S2 = Y2 * Z1 * Z1Z1

    if (U1 == U2 && S1 == S2)
    {
        // dbl case; nothing of above can be reused
        return this->dbl();
    }

    // rest of add case
    alt_bn128_Fq2 H = U2 - U1;                            // H = U2-U1
    alt_bn128_Fq2 S2_minus_S1 = S2-S1;
    alt_bn128_Fq2 I = (H+H).squared();                    // I = (2 * H)^2
    alt_bn128_Fq2 J = H * I;                              // J = H * I
    alt_bn128_Fq2 r = S2_minus_S1 + S2_minus_S1;          // r = 2 * (S2-S1)
    alt_bn128_Fq2 V = U1 * I;                             // V = U1 * I
    alt_bn128_Fq2 X3 = r.squared() - J - (V+V);           // X3 = r^2 - J - 2 * V
    alt_bn128_Fq2 S1_J = S1 * J;
    alt_bn128_Fq2 Y3 = r * (V-X3) - (S1_J+S1_J);          // Y3 = r * (V-X3)-2 S1 J
    alt_bn128_Fq2 Z3 = ((this->Z+other.Z).squared()-Z1Z1-Z2Z2) * H; // Z3 = ((Z1+Z2)^2-Z1Z1-Z2Z2) * H

    return alt_bn128_G2(X3, Y3, Z3);
}

alt_bn128_G2 alt_bn128_G2::operator-() const
{
    return alt_bn128_G2(this->X, -(this->Y), this->Z);
}


alt_bn128_G2 alt_bn128_G2::operator-(const alt_bn128_G2 &other) const
{
    return (*this) + (-other);
}

alt_bn128_G2 alt_bn128_G2::add(const alt_bn128_G2 &other) const
{
    // handle special cases having to do with O
    if (this->is_zero())
    {
        return other;
    }

    if (other.is_zero())
    {
        return *this;
    }

    // no need to handle points of order 2,4
    // (they cannot exist in a prime-order subgroup)

    // handle double case
    if (this->operator==(other))
    {
        return this->dbl();
    }

#ifdef PROFILE_OP_COUNTS
    this->add_cnt++;
#endif
    // NOTE: does not handle O and pts of order 2,4
    // http://www.hyperelliptic.org/EFD/g1p/auto-shortw-projective.html#addition-add-1998-cmo-2

    alt_bn128_Fq2 Z1Z1 = (this->Z).squared();             // Z1Z1 = Z1^2
    alt_bn128_Fq2 Z2Z2 = (other.Z).squared();             // Z2Z2 = Z2^2
    alt_bn128_Fq2 U1 = (this->X) * Z2Z2;                  // U1 = X1 * Z2Z2
    alt_bn128_Fq2 U2 = (other.X) * Z1Z1;                  // U2 = X2 * Z1Z1
    alt_bn128_Fq2 S1 = (this->Y) * (other.Z) * Z2Z2;      // S1 = Y1 * Z2 * Z2Z2
    alt_bn128_Fq2 S2 = (other.Y) * (this->Z) * Z1Z1;      // S2 = Y2 * Z1 * Z1Z1
    alt_bn128_Fq2 H = U2 - U1;                            // H = U2-U1
    alt_bn128_Fq2 S2_minus_S1 = S2-S1;
    alt_bn128_Fq2 I = (H+H).squared();                    // I = (2 * H)^2
    alt_bn128_Fq2 J = H * I;                              // J = H * I
    alt_bn128_Fq2 r = S2_minus_S1 + S2_minus_S1;          // r = 2 * (S2-S1)
    alt_bn128_Fq2 V = U1 * I;                             // V = U1 * I
    alt_bn128_Fq2 X3 = r.squared() - J - (V+V);           // X3 = r^2 - J - 2 * V
    alt_bn128_Fq2 S1_J = S1 * J;
    alt_bn128_Fq2 Y3 = r * (V-X3) - (S1_J+S1_J);          // Y3 = r * (V-X3)-2 S1 J
    alt_bn128_Fq2 Z3 = ((this->Z+other.Z).squared()-Z1Z1-Z2Z2) * H; // Z3 = ((Z1+Z2)^2-Z1Z1-Z2Z2) * H

    return alt_bn128_G2(X3, Y3, Z3);
}

alt_bn128_G2 alt_bn128_G2::mixed_add(const alt_bn128_G2 &other) const
{
#ifdef DEBUG
    assert(other.is_special());
#endif

    // handle special cases having to do with O
    if (this->is_zero())
    {
        return other;
    }

    if (other.is_zero())
    {
        return *this;
    }

    // no need to handle points of order 2,4
    // (they cannot exist in a prime-order subgroup)

    // check for doubling case

    // using Jacobian coordinates so:
    // (X1:Y1:Z1) = (X2:Y2:Z2)
    // iff
    // X1/Z1^2 == X2/Z2^2 and Y1/Z1^3 == Y2/Z2^3
    // iff
    // X1 * Z2^2 == X2 * Z1^2 and Y1 * Z2^3 == Y2 * Z1^3

    // we know that Z2 = 1

    const alt_bn128_Fq2 Z1Z1 = (this->Z).squared();

    const alt_bn128_Fq2 &U1 = this->X;
    const alt_bn128_Fq2 U2 = other.X * Z1Z1;

    const alt_bn128_Fq2 Z1_cubed = (this->Z) * Z1Z1;

    const alt_bn128_Fq2 &S1 = (this->Y);                // S1 = Y1 * Z2 * Z2Z2
    const alt_bn128_Fq2 S2 = (other.Y) * Z1_cubed;      // S2 = Y2 * Z1 * Z1Z1

    if (U1 == U2 && S1 == S2)
    {
        // dbl case; nothing of above can be reused
        return this->dbl();
    }

#ifdef PROFILE_OP_COUNTS
    this->add_cnt++;
#endif

    // NOTE: does not handle O and pts of order 2,4
    // http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#addition-madd-2007-bl
    alt_bn128_Fq2 H = U2-(this->X);                         // H = U2-X1
    alt_bn128_Fq2 HH = H.squared() ;                        // HH = H&2
    alt_bn128_Fq2 I = HH+HH;                                // I = 4*HH
    I = I + I;
    alt_bn128_Fq2 J = H*I;                                  // J = H*I
    alt_bn128_Fq2 r = S2-(this->Y);                         // r = 2*(S2-Y1)
    r = r + r;
    alt_bn128_Fq2 V = (this->X) * I ;                       // V = X1*I
    alt_bn128_Fq2 X3 = r.squared()-J-V-V;                   // X3 = r^2-J-2*V
    alt_bn128_Fq2 Y3 = (this->Y)*J;                         // Y3 = r*(V-X3)-2*Y1*J
    Y3 = r*(V-X3) - Y3 - Y3;
    alt_bn128_Fq2 Z3 = ((this->Z)+H).squared() - Z1Z1 - HH; // Z3 = (Z1+H)^2-Z1Z1-HH

    return alt_bn128_G2(X3, Y3, Z3);
}

alt_bn128_G2 alt_bn128_G2::dbl() const
{
#ifdef PROFILE_OP_COUNTS
    this->dbl_cnt++;
#endif
    // handle point at infinity
    if (this->is_zero())
    {
        return (*this);
    }

    // NOTE: does not handle O and pts of order 2,4
    // http://www.hyperelliptic.org/EFD/g1p/auto-shortw-projective.html#doubling-dbl-2007-bl

    alt_bn128_Fq2 A = (this->X).squared();         // A = X1^2
    alt_bn128_Fq2 B = (this->Y).squared();        // B = Y1^2
    alt_bn128_Fq2 C = B.squared();                // C = B^2
    alt_bn128_Fq2 D = (this->X + B).squared() - A - C;
    D = D+D;                        // D = 2 * ((X1 + B)^2 - A - C)
    alt_bn128_Fq2 E = A + A + A;                  // E = 3 * A
    alt_bn128_Fq2 F = E.squared();                // F = E^2
    alt_bn128_Fq2 X3 = F - (D+D);                 // X3 = F - 2 D
    alt_bn128_Fq2 eightC = C+C;
    eightC = eightC + eightC;
    eightC = eightC + eightC;
    alt_bn128_Fq2 Y3 = E * (D - X3) - eightC;     // Y3 = E * (D - X3) - 8 * C
    alt_bn128_Fq2 Y1Z1 = (this->Y)*(this->Z);
    alt_bn128_Fq2 Z3 = Y1Z1 + Y1Z1;               // Z3 = 2 * Y1 * Z1

    return alt_bn128_G2(X3, Y3, Z3);
}

alt_bn128_G2 alt_bn128_G2::mul_by_q() const
{
    return alt_bn128_G2(alt_bn128_twist_mul_by_q_X * (this->X).Frobenius_map(1),
                      alt_bn128_twist_mul_by_q_Y * (this->Y).Frobenius_map(1),
                      (this->Z).Frobenius_map(1));
}

bool alt_bn128_G2::is_well_formed() const
{
    if (this->is_zero())
    {
        return true;
    }
    else
    {
        /*
          y^2 = x^3 + b

          We are using Jacobian coordinates, so equation we need to check is actually

          (y/z^3)^2 = (x/z^2)^3 + b
          y^2 / z^6 = x^3 / z^6 + b
          y^2 = x^3 + b z^6
        */
        alt_bn128_Fq2 X2 = this->X.squared();
        alt_bn128_Fq2 Y2 = this->Y.squared();
        alt_bn128_Fq2 Z2 = this->Z.squared();

        alt_bn128_Fq2 X3 = this->X * X2;
        alt_bn128_Fq2 Z3 = this->Z * Z2;
        alt_bn128_Fq2 Z6 = Z3.squared();

        return (Y2 == X3 + alt_bn128_twist_coeff_b * Z6);
    }
}

alt_bn128_G2 alt_bn128_G2::zero()
{
    return G2_zero;
}

alt_bn128_G2 alt_bn128_G2::one()
{
    return G2_one;
}

alt_bn128_G2 alt_bn128_G2::random_element()
{
    return (alt_bn128_Fr::random_element().as_bigint()) * G2_one;
}

std::ostream& operator<<(std::ostream &out, const alt_bn128_G2 &g)
{
    alt_bn128_G2 copy(g);
    copy.to_affine_coordinates();
    out << (copy.is_zero() ? 1 : 0) << OUTPUT_SEPARATOR;
#ifdef NO_PT_COMPRESSION
    out << copy.X << OUTPUT_SEPARATOR << copy.Y;
#else
    /* storing LSB of Y */
    out << copy.X << OUTPUT_SEPARATOR << (copy.Y.c0.as_bigint().data[0] & 1);
#endif

    return out;
}

std::istream& operator>>(std::istream &in, alt_bn128_G2 &g)
{
    char is_zero;
    alt_bn128_Fq2 tX, tY;

#ifdef NO_PT_COMPRESSION
    in >> is_zero >> tX >> tY;
    is_zero -= '0';
#else
    in.read((char*)&is_zero, 1); // this reads is_zero;
    is_zero -= '0';
    consume_OUTPUT_SEPARATOR(in);

    unsigned char Y_lsb;
    in >> tX;
    consume_OUTPUT_SEPARATOR(in);
    in.read((char*)&Y_lsb, 1);
    Y_lsb -= '0';

    // y = +/- sqrt(x^3 + b)
    if (!is_zero)
    {
        alt_bn128_Fq2 tX2 = tX.squared();
        alt_bn128_Fq2 tY2 = tX2 * tX + alt_bn128_twist_coeff_b;
        tY = tY2.sqrt();

        if ((tY.c0.as_bigint().data[0] & 1) != Y_lsb)
        {
            tY = -tY;
        }
    }
#endif
    // using projective coordinates
    if (!is_zero)
    {
        g.X = tX;
        g.Y = tY;
        g.Z = alt_bn128_Fq2::one();
    }
    else
    {
        g = alt_bn128_G2::zero();
    }

    return in;
}

template<>
void batch_to_special_all_non_zeros<alt_bn128_G2>(std::vector<alt_bn128_G2> &vec)
{
    std::vector<alt_bn128_Fq2> Z_vec;
    Z_vec.reserve(vec.size());

    for (auto &el: vec)
    {
        Z_vec.emplace_back(el.Z);
    }
    batch_invert<alt_bn128_Fq2>(Z_vec);

    const alt_bn128_Fq2 one = alt_bn128_Fq2::one();

    for (size_t i = 0; i < vec.size(); ++i)
    {
        alt_bn128_Fq2 Z2 = Z_vec[i].squared();
        alt_bn128_Fq2 Z3 = Z_vec[i] * Z2;

        vec[i].X = vec[i].X * Z2;
        vec[i].Y = vec[i].Y * Z3;
        vec[i].Z = one;
    }
}

} // libsnark
