/*===========================================================================

 Copyright (C) 2002-2020 Amandine Cottaz, Yves Renard
 Copyright (C) 2014-2020 Konstantinos Poulios

 This file is a part of GetFEM

 GetFEM  is  free software;  you  can  redistribute  it  and/or modify it
 under  the  terms  of the  GNU  Lesser General Public License as published
 by  the  Free Software Foundation;  either version 3 of the License,  or
 (at your option) any later version along with the GCC Runtime Library
 Exception either version 3.1 or (at your option) any later version.
 This program  is  distributed  in  the  hope  that it will be useful,  but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or  FITNESS  FOR  A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 License and GCC Runtime Library Exception for more details.
 You  should  have received a copy of the GNU Lesser General Public License
 along  with  this program;  if not, write to the Free Software Foundation,
 Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.

===========================================================================*/


#include "getfem/getfem_models.h"
#include "getfem/getfem_plasticity.h"
#include "getfem/getfem_interpolation.h"
#include "getfem/getfem_generic_assembly.h"
#include "gmm/gmm_dense_matrix_functions.h"
#include <iomanip>

namespace getfem {

  //=========================================================================
  //
  //  Specific nonlinear operators of the high-level generic assembly
  //  language, useful for plasticity modeling
  //
  //=========================================================================

  static void ga_init_scalar(bgeot::multi_index &mi) { mi.resize(0); }
  static void ga_init_vector(bgeot::multi_index &mi, size_type N)
    { mi.resize(1); mi[0] = N; }
  static void ga_init_matrix(bgeot::multi_index &mi, size_type M, size_type N)
    { mi.resize(2); mi[0] = M; mi[1] = N; }
  static void ga_init_square_matrix(bgeot::multi_index &mi, size_type N)
    { mi.resize(2); mi[0] = mi[1] = N; }


  inline void matmul(base_matrix &aa,base_matrix &bb,base_matrix &cc)
    {gmm::mult(aa,bb,cc);}

  bool expm(const base_matrix &a_, base_matrix &aexp) {

    const size_type N = gmm::mat_nrows(a_);
    bool success(false);

    // Pade approximation ported from Eigen/Unsupported
    base_matrix a(a_);
    gmm::clear(aexp.as_vector());
    base_matrix tmp(aexp), v(aexp), u(aexp); // Pade approximant is (v+u)/(v-u)
    const scalar_type l1norm = gmm::mat_norminf(a_);
    int e = 0; // squarings
    if (l1norm < 1.495585217958292e-002) { // matrix_exp_pade3(a, u, v)
      const static std::array<scalar_type,4> b{120,60,12,1};
      base_matrix a2(a);
      matmul(a, a, a2);
      gmm::copy(gmm::scaled(a2,b[2]), v);   // v = b2*A2 + b0*I
      gmm::copy(gmm::scaled(a2,b[3]), u);   // u = b3*A2 + b1*I
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }
    } else if (l1norm < 2.539398330063230e-001) { // matrix_exp_pade5(a, u, v)
      const static std::array<scalar_type,6> b{30240,15120,3360,420,30,1};
      base_matrix a2(a), a4(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      gmm::add(gmm::scaled(a4,b[4]),    // v = b4*A4 + b2*A2 + b0*I
               gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a4,b[5]),    // u = b5*A4 + b3*A2 + b1*I
               gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }
    } else if (l1norm < 9.504178996162932e-001) { // matrix_exp_pade7(a, u, v)
      const static std::array<scalar_type,8>
        b{17297280,8648640,1995840,277200,25200,1512,56,1};
      base_matrix a2(a), a4(a), a6(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      matmul(a2, a4, a6);
      gmm::add(gmm::scaled(a6,b[6]),    // v = b6*A6 + b4*A4 + b2*A2 + b0*I
               gmm::scaled(a4,b[4]), v);
      gmm::add(gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a6,b[7]),    // u = b7*A6 + b5*A4 + b3*A2 + b1*I
               gmm::scaled(a4,b[5]), u);
      gmm::add(gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }
    } else if (l1norm < 2.097847961257068e+000) { // matrix_exp_pade9(a, u, v)
      const static std::array<scalar_type,10>
        b{17643225600,8821612800,2075673600,302702400,30270240,2162160,
          110880,3960,90,1};
      base_matrix a2(a), a4(a), a6(a), a8(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      matmul(a2, a4, a6);
      matmul(a4, a4, a8);
      gmm::add(gmm::scaled(a8,b[8]),    // v = b8*A8+b6*A6+b4*A4+b2*A2+b0*I
               gmm::scaled(a6,b[6]), v);
      gmm::add(gmm::scaled(a4,b[4]), v);
      gmm::add(gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a8,b[9]),    // u = b9*A8+b7*A6+b5*A4+b3*A2+b1*I
               gmm::scaled(a6,b[7]), u);
      gmm::add(gmm::scaled(a4,b[5]), u);
      gmm::add(gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }
    } else { // matrix_exp_pade13(a, U, V);
      const scalar_type maxnorm = 5.371920351148152;
      frexp(l1norm / maxnorm, &e);
      if (e <= 0) e = 0;
      else for (auto &&val : a.as_vector()) { val = ldexp(val,-e); }
           // <==> gmm::scale(a, pow(scalar_type(2),-scalar_type(e)));
      const static std::array<scalar_type,14>
        b{64764752532480000,32382376266240000,7771770303897600,
          1187353796428800,129060195264000,10559470521600,670442572800,
          33522128640,1323241920,40840800,960960,16380,182,1};
      base_matrix a2(a), a4(a), a6(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      matmul(a2, a4, a6);
      gmm::add(gmm::scaled(a6,b[12]),
               gmm::scaled(a4,b[10]), tmp);
      gmm::add(gmm::scaled(a2,b[8]), tmp);
      matmul(a6, tmp, v);             // v = b12*A12+b10*A10+b8*A8
      gmm::add(gmm::scaled(a6,b[6]), v); //   + b6*A6+b4*A4+b2*A2+b0*I
      gmm::add(gmm::scaled(a4,b[4]), v);
      gmm::add(gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a6,b[13]),
               gmm::scaled(a4,b[11]), tmp);
      gmm::add(gmm::scaled(a2,b[9]), tmp);
      matmul(a6, tmp, u);             // u = b13*A12+b11*A10+b9*A8
      gmm::add(gmm::scaled(a6,b[7]), u); //   + b7*A6+b5*A4+b3*A2+b1*I
      gmm::add(gmm::scaled(a4,b[5]), u);
      gmm::add(gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }
    }
    std::swap(u, tmp);
    matmul(a, tmp, u);              // u <-- A*u

    gmm::add(v,gmm::scaled(u,-1),tmp); // tmp = denom = v-u
    gmm::lu_inverse(tmp);              // tmp = (v-u)^-1
    gmm::add(u,v);                     // v <-- numer = v+u;
    matmul(tmp,v,aexp);
    success = true;

    for (int i=0; i < e; ++i) { // unscale result
      std::swap(aexp, tmp);
      matmul(tmp, tmp, aexp);
    }
    return success;
  }



  bool expm_deriv(const base_matrix &a_, base_tensor &daexp) {

    size_type N = gmm::mat_nrows(a_);
    base_matrix a(a_), tmp(a_);
    gmm::clear(tmp.as_vector());
    base_matrix aexp(tmp), v(tmp), u(tmp), // Pade approximant is (v+u)/(v-u)
                tmp_(tmp), dv_(tmp), du_(tmp);
    gmm::clear(daexp.as_vector());
    base_tensor dv(daexp), du(daexp);
    const scalar_type l1norm = gmm::mat_norminf(a_);
    int e = 0; // squarings
    if (l1norm < 1.495585217958292e-002) { // matrix_exp_pade3(a, u, v)
      const static std::array<scalar_type,4> b{120,60,12,1};
      base_matrix a2(a);
      matmul(a, a, a2);
      gmm::copy(gmm::scaled(a2,b[2]), v);   // v = b2*A2 + b0*I
      gmm::copy(gmm::scaled(a2,b[3]), u);   // u = b3*A2 + b1*I
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }

      for (size_type l=0; l < N; ++l) // tmp derivative of a2
        for (size_type k=0; k < N; ++k) {
          gmm::clear(dv_); gmm::clear(du_);
          for (size_type ij=0; ij < N; ++ij) {
            const auto &al=a(l,ij), &ak=a(ij,k);
            dv_(k,ij) += b[2]*al;   dv_(ij,l) += b[2]*ak;
            du_(k,ij) += b[3]*al;   du_(ij,l) += b[3]*ak;
          }
          std::swap(du_,tmp); // derivative of u <-- A*u
          matmul(a,tmp,du_);
          for (size_type j=0; j < N; ++j) // i == k
            du_(k,j) += u(l,j);

          std::copy(dv_.begin(),dv_.end(), &dv(0,0,k,l));
          std::copy(du_.begin(),du_.end(), &du(0,0,k,l));
        }
    } else if (l1norm < 2.539398330063230e-001) { // matrix_exp_pade5(a, u, v)
      const static std::array<scalar_type,6> b{30240,15120,3360,420,30,1};
      base_matrix a2(a), a4(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      gmm::add(gmm::scaled(a4,b[4]),    // v = b4*A4 + b2*A2 + b0*I
               gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a4,b[5]),    // u = b5*A4 + b3*A2 + b1*I
               gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }

      base_matrix da2(aexp); // zero init
      for (size_type l=0; l < N; ++l)
        for (size_type k=0; k < N; ++k) {
          gmm::clear(da2); gmm::clear(dv_); gmm::clear(du_);
          for (size_type ij=0; ij < N; ++ij) {
            const auto &al=a(l,ij), &ak=a(ij,k);
            da2(k,ij) += al;        da2(ij,l) += ak;
            dv_(k,ij) += b[2]*al;   dv_(ij,l) += b[2]*ak;
            du_(k,ij) += b[3]*al;   du_(ij,l) += b[3]*ak;
          }
          matmul(a2,da2,tmp);
          matmul(da2,a2,tmp_);
          gmm::add(tmp_,tmp);        // tmp derivative of a4
          gmm::add(gmm::scaled(tmp,b[4]), dv_);
          gmm::add(gmm::scaled(tmp,b[5]), du_);

          std::swap(du_,tmp); // derivative of u <-- A*u
          matmul(a,tmp,du_);
          for (size_type j=0; j < N; ++j) // i == k
            du_(k,j) += u(l,j);

          std::copy(dv_.begin(),dv_.end(), &dv(0,0,k,l));
          std::copy(du_.begin(),du_.end(), &du(0,0,k,l));
        }
    } else if (l1norm < 9.504178996162932e-001) { // matrix_exp_pade7(a, u, v)
      const static std::array<scalar_type,8>
        b{17297280,8648640,1995840,277200,25200,1512,56,1};
      base_matrix a2(a), a4(a), a6(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      matmul(a2, a4, a6);
      gmm::add(gmm::scaled(a6,b[6]),    // v = b6*A6 + b4*A4 + b2*A2 + b0*I
               gmm::scaled(a4,b[4]), v);
      gmm::add(gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a6,b[7]),    // u = b7*A6 + b5*A4 + b3*A2 + b1*I
               gmm::scaled(a4,b[5]), u);
      gmm::add(gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }

      base_matrix da2(aexp); // zero init
      for (size_type l=0; l < N; ++l)
        for (size_type k=0; k < N; ++k) {
          gmm::clear(da2); gmm::clear(dv_); gmm::clear(du_);
          for (size_type ij=0; ij < N; ++ij) {
            const auto &al=a(l,ij), &ak=a(ij,k);
            da2(k,ij) += al;        da2(ij,l) += ak;
            dv_(k,ij) += b[2]*al;   dv_(ij,l) += b[2]*ak;
            du_(k,ij) += b[3]*al;   du_(ij,l) += b[3]*ak;
          }
          matmul(a2,da2,tmp);
          matmul(da2,a2,tmp_);
          gmm::add(tmp_,tmp);        // tmp derivative of a4
          gmm::add(gmm::scaled(tmp,b[4]), dv_);
          gmm::add(gmm::scaled(tmp,b[5]), du_);

          matmul(a2,tmp,tmp_);
          matmul(da2,a4,tmp);
          gmm::add(tmp_,tmp);        // tmp derivative of a6
          gmm::add(gmm::scaled(tmp,b[6]), dv_);
          gmm::add(gmm::scaled(tmp,b[7]), du_);

          std::swap(du_,tmp); // derivative of u <-- A*u
          matmul(a,tmp,du_);
          for (size_type j=0; j < N; ++j) // i == k
            du_(k,j) += u(l,j);

          std::copy(dv_.begin(),dv_.end(), &dv(0,0,k,l));
          std::copy(du_.begin(),du_.end(), &du(0,0,k,l));
        }
    } else if (l1norm < 2.097847961257068e+000) { // matrix_exp_pade9(a, u, v)
      const static std::array<scalar_type,10>
        b{17643225600,8821612800,2075673600,302702400,30270240,2162160,
          110880,3960,90,1};
      base_matrix a2(a), a4(a), a6(a), a8(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      matmul(a2, a4, a6);
      matmul(a4, a4, a8);
      gmm::add(gmm::scaled(a8,b[8]),    // v = b8*A8+b6*A6+b4*A4+b2*A2+b0*I
               gmm::scaled(a6,b[6]), v);
      gmm::add(gmm::scaled(a4,b[4]), v);
      gmm::add(gmm::scaled(a2,b[2]), v);
      gmm::add(gmm::scaled(a8,b[9]),    // u = b9*A8+b7*A6+b5*A4+b3*A2+b1*I
               gmm::scaled(a6,b[7]), u);
      gmm::add(gmm::scaled(a4,b[5]), u);
      gmm::add(gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }

      base_matrix da2(aexp), da4(aexp); // zero init
      for (size_type l=0; l < N; ++l)
        for (size_type k=0; k < N; ++k) {
          gmm::clear(da2); gmm::clear(dv_); gmm::clear(du_);
          for (size_type ij=0; ij < N; ++ij) {
            const auto &al=a(l,ij), &ak=a(ij,k);
            da2(k,ij) += al;        da2(ij,l) += ak;
            dv_(k,ij) += b[2]*al;   dv_(ij,l) += b[2]*ak;
            du_(k,ij) += b[3]*al;   du_(ij,l) += b[3]*ak;
          }
          matmul(a2,da2,tmp);
          matmul(da2,a2,da4);
          gmm::add(tmp,da4);
          gmm::add(gmm::scaled(da4,b[4]), dv_);
          gmm::add(gmm::scaled(da4,b[5]), du_);

          matmul(a2,da4,tmp_);
          matmul(da2,a4,tmp);
          gmm::add(tmp_,tmp);        // tmp derivative of a6
          gmm::add(gmm::scaled(tmp,b[6]), dv_);
          gmm::add(gmm::scaled(tmp,b[7]), du_);

          matmul(a4,da4,tmp);
          matmul(da4,a4,tmp_);
          gmm::add(tmp_,tmp);        // tmp derivative of a8
          gmm::add(gmm::scaled(tmp,b[8]), dv_);
          gmm::add(gmm::scaled(tmp,b[9]), du_);

          std::swap(du_,tmp); // derivative of u <-- A*u
          matmul(a,tmp,du_);
          for (size_type j=0; j < N; ++j) // i == k
            du_(k,j) += u(l,j);

          std::copy(dv_.begin(),dv_.end(), &dv(0,0,k,l));
          std::copy(du_.begin(),du_.end(), &du(0,0,k,l));
        }
    } else { // matrix_exp_pade13(a, U, V);
      const scalar_type maxnorm = 5.371920351148152;
      frexp(l1norm / maxnorm, &e);
      if (e <= 0) e = 0;
      else for (auto &&val : a.as_vector()) { val = ldexp(val,-e); }
           // <==> gmm::scale(a, pow(scalar_type(2),-scalar_type(e)));
      const static std::array<scalar_type,14>
        b{64764752532480000,32382376266240000,7771770303897600,
          1187353796428800,129060195264000,10559470521600,670442572800,
          33522128640,1323241920,40840800,960960,16380,182,1};
      base_matrix a2(a), a4(a), a6(a), v_(a), u_(a);
      matmul(a, a, a2);
      matmul(a2, a2, a4);
      matmul(a2, a4, a6);
      gmm::add(gmm::scaled(a6,b[12]),
               gmm::scaled(a4,b[10]), v_);
      gmm::add(gmm::scaled(a2,b[8]), v_);
      matmul(a6, v_, v);              // v = b12*A12+b10*A10+b8*A8
      gmm::add(gmm::scaled(a6,b[6]), v); //   + b6*A6+b4*A4+b2*A2+b0*I
      gmm::add(gmm::scaled(a4,b[4]), v);
      gmm::add(gmm::scaled(a2,b[2]), v);

      gmm::add(gmm::scaled(a6,b[13]),
               gmm::scaled(a4,b[11]), u_);
      gmm::add(gmm::scaled(a2,b[9]), u_);
      matmul(a6, u_, u);              // u = b13*A12+b11*A10+b9*A8
      gmm::add(gmm::scaled(a6,b[7]), u); //   + b7*A6+b5*A4+b3*A2+b1*I
      gmm::add(gmm::scaled(a4,b[5]), u);
      gmm::add(gmm::scaled(a2,b[3]), u);
      for (size_type ij=0; ij < N; ++ij)
        { v(ij,ij) += b[0]; u(ij,ij) += b[1]; }

      base_matrix da2(aexp), da4(aexp), da6(aexp),
                  dv__(aexp), du__(aexp);
      for (size_type l=0; l < N; ++l)
        for (size_type k=0; k < N; ++k) {
          gmm::clear(da2); gmm::clear(dv_); gmm::clear(du_);
          gmm::clear(dv__); gmm::clear(du__);
          for (size_type ij=0; ij < N; ++ij) {
            const auto &al=a(l,ij), &ak=a(ij,k);
            da2(k,ij) += al;        da2(ij,l) += ak;
            dv_(k,ij) += b[2]*al;   dv_(ij,l) += b[2]*ak;
            du_(k,ij) += b[3]*al;   du_(ij,l) += b[3]*ak;
            dv__(k,ij) += b[8]*al;  dv__(ij,l) += b[8]*ak;
            du__(k,ij) += b[9]*al;  du__(ij,l) += b[9]*ak;
          }
          matmul(a2,da2,da4);
          matmul(da2,a2,tmp); gmm::add(tmp,da4);
          gmm::add(gmm::scaled(da4,b[4]), dv_);
          gmm::add(gmm::scaled(da4,b[5]), du_);
          gmm::add(gmm::scaled(da4,b[10]), dv__);
          gmm::add(gmm::scaled(da4,b[11]), du__);

          matmul(a2,da4,da6);
          matmul(da2,a4,tmp); gmm::add(tmp,da6);
          gmm::add(gmm::scaled(da6,b[6]), dv_);
          gmm::add(gmm::scaled(da6,b[7]), du_);
          gmm::add(gmm::scaled(da6,b[12]), dv__);
          gmm::add(gmm::scaled(da6,b[13]), du__);

          matmul(a6,dv__,tmp); gmm::add(tmp, dv_);
          matmul(da6,v_,tmp);  gmm::add(tmp, dv_);

          matmul(a6,du__,tmp); gmm::add(tmp, du_);
          matmul(da6,u_,tmp);  gmm::add(tmp, du_);

          std::swap(du_,tmp); // derivative of u <-- A*u
          matmul(a,tmp,du_);
          for (size_type j=0; j < N; ++j) // i == k
            du_(k,j) += u(l,j);

          std::copy(dv_.begin(),dv_.end(), &dv(0,0,k,l));
          std::copy(du_.begin(),du_.end(), &du(0,0,k,l));
        }
    }
    std::swap(u, tmp);
    matmul(a, tmp, u);              // u <-- A*u

    base_matrix inv_denom(v);
    gmm::add(gmm::scaled(u,-1),inv_denom); // denom = v-u
    gmm::lu_inverse(inv_denom);

    gmm::add(u,v,tmp);                     // tmp = numer = v+u
    matmul(inv_denom,tmp,aexp);

    for (size_type l=0; l < N; ++l)
      for (size_type k=0; k < N; ++k) { // daexp_kl= D\(dN_kl-dD_kl*aexp)
        std::copy(&dv(0,0,k,l),&dv(0,0,k,l)+N*N, tmp_.begin());
        std::copy(&du(0,0,k,l),&du(0,0,k,l)+N*N, tmp.begin());
        gmm::add(gmm::scaled(tmp_/*dv*/,-1),tmp/*du*/); // tmp = -(dv-du)
        matmul(tmp,aexp,tmp_);
        std::copy(&du(0,0,k,l),&du(0,0,k,l)+N*N, tmp.begin());
        gmm::add(tmp/*du*/, tmp_);
        std::copy(&dv(0,0,k,l),&dv(0,0,k,l)+N*N, tmp.begin());
        gmm::add(tmp/*dv*/, tmp_); // tmp_ = (dv+du)_kl-(dv-du)_kl*aexp 
        matmul(inv_denom, tmp_, tmp);
        std::copy(tmp.begin(), tmp.end(), &daexp(0,0,k,l));
      }
    if (e)
      for (auto &&val : daexp.as_vector()) { val = ldexp(val,-e); }

    for (int i=0; i < e; ++i) { // unscale result
      for (size_type l=0; l < N; ++l)
        for (size_type k=0; k < N; ++k) {
          std::copy(&daexp(0,0,k,l), &daexp(0,0,k,l)+N*N, tmp.begin());
          matmul(tmp, aexp, u); // u,v used a temporaries
          matmul(aexp, tmp, v); //
          gmm::add(u, v, tmp);
          std::copy(tmp.begin(), tmp.end(), &daexp(0,0,k,l));
        }
      std::swap(aexp,tmp);
      matmul(tmp, tmp, aexp);
    }
    return true;
  }

  // numerical differantiation of logm
  // not used because it caused some issues and was slower than
  // simply inverting the derivative of expm
  bool logm_deriv(const base_matrix &a, base_tensor &dalog,
                  base_matrix *palog=NULL) {

    size_type N = gmm::mat_nrows(a);
    base_matrix a1(a), alog1(a), alog(a);
    logm(a, alog);
    scalar_type eps(1e-8);
    for (size_type k=0; k < N; ++k)
      for (size_type l=0; l < N; ++l) {
        gmm::copy(a, a1);
        a1(k,l) += eps;
        gmm::logm(a1, alog1);
        for (size_type i=0; i < N; ++i)
          for (size_type j=0; j < N; ++j)
            dalog(i,j,k,l) = (alog1(i,j) - alog(i,j))/eps;
      }
    if (palog) gmm::copy(alog, *palog);
    return true;
  }


  // Matrix exponential
  struct matrix_exponential_operator : public ga_nonlinear_operator {
    bool result_size(const arg_list &args, bgeot::multi_index &sizes) const {
      if (args.size() != 1 || args[0]->sizes().size() != 2
          || args[0]->sizes()[0] != args[0]->sizes()[1]) return false;
      ga_init_matrix(sizes, args[0]->sizes()[0], args[0]->sizes()[1]);
      return true;
    }

    // Value:
    void value(const arg_list &args, base_tensor &result) const {
      size_type N = args[0]->sizes()[0];
      base_matrix inpmat(N,N), outmat(N,N);
      gmm::copy(args[0]->as_vector(), inpmat.as_vector());
      bool info = expm(inpmat, outmat);
      GMM_ASSERT1(info, "Matrix exponential calculation "
                        "failed to converge");
      gmm::copy(outmat.as_vector(), result.as_vector());
    }

    // Derivative:
    void derivative(const arg_list &args, size_type /*nder*/,
                    base_tensor &result) const {
      size_type N = args[0]->sizes()[0];
      base_matrix inpmat(N,N);
      gmm::copy(args[0]->as_vector(), inpmat.as_vector());
      bool info = expm_deriv(inpmat, result);
      GMM_ASSERT1(info, "Matrix exponential derivative calculation "
                        "failed to converge");
    }

    // Second derivative : not implemented
    void second_derivative(const arg_list &, size_type, size_type,
                           base_tensor &) const {
      GMM_ASSERT1(false, "Sorry, second derivative not implemented");
    }
  };


  // Matrix logarithm
  struct matrix_logarithm_operator : public ga_nonlinear_operator {
    bool result_size(const arg_list &args, bgeot::multi_index &sizes) const {
      if (args.size() != 1 || args[0]->sizes().size() != 2
          || args[0]->sizes()[0] != args[0]->sizes()[1]) return false;
      ga_init_matrix(sizes, args[0]->sizes()[0], args[0]->sizes()[1]);
      return true;
    }

    // Value:
    void value(const arg_list &args, base_tensor &result) const {
      size_type N = args[0]->sizes()[0];
      base_matrix inpmat(N,N), outmat(N,N);
      gmm::copy(args[0]->as_vector(), inpmat.as_vector());
      gmm::logm(inpmat, outmat);
      gmm::copy(outmat.as_vector(), result.as_vector());
    }

    // Derivative:
    void derivative(const arg_list &args, size_type /*nder*/,
                    base_tensor &result) const {
      size_type N = args[0]->sizes()[0];
      base_matrix inpmat(N,N), outmat(N,N), tmpmat(N*N,N*N);
      gmm::copy(args[0]->as_vector(), inpmat.as_vector());
      gmm::logm(inpmat, outmat);
      bool info = expm_deriv(outmat, result);
      if (info) {
        gmm::copy(result.as_vector(), tmpmat.as_vector());
        scalar_type det = gmm::lu_inverse(tmpmat);
        if (det <= 0) gmm::copy(gmm::identity_matrix(), tmpmat);
        gmm::copy(tmpmat.as_vector(), result.as_vector());
      }
      GMM_ASSERT1(info, "Matrix logarithm derivative calculation "
                        "failed to converge");
    }

    // Second derivative : not implemented
    void second_derivative(const arg_list &, size_type, size_type,
                           base_tensor &) const {
      GMM_ASSERT1(false, "Sorry, second derivative not implemented");
    }
  };


  // Normalized vector/matrix operator : Vector/matrix divided by its Frobenius norm
  struct normalized_operator : public ga_nonlinear_operator {
    bool result_size(const arg_list &args, bgeot::multi_index &sizes) const {
      if (args.size() != 1 || args[0]->sizes().size() > 2
          || args[0]->sizes().size() < 1) return false;
      if (args[0]->sizes().size() == 1)
        ga_init_vector(sizes, args[0]->sizes()[0]);
      else
        ga_init_matrix(sizes, args[0]->sizes()[0], args[0]->sizes()[1]);
      return true;
    }

    // Value : u/|u|
    void value(const arg_list &args, base_tensor &result) const {
#   if 1
      const base_tensor &t = *args[0];
      scalar_type eps = 1E-25;
      scalar_type no = ::sqrt(gmm::vect_norm2_sqr(t.as_vector())+gmm::sqr(eps));
      gmm::copy(gmm::scaled(t.as_vector(), scalar_type(1)/no),
                result.as_vector());
#   else
      scalar_type no = gmm::vect_norm2(args[0]->as_vector());
      if (no < 1E-15)
        gmm::clear(result.as_vector());
      else
        gmm::copy(gmm::scaled(args[0]->as_vector(), scalar_type(1)/no),
                  result.as_vector());
#   endif
    }

    // Derivative : (|u|^2 Id - u x u)/|u|^3
    void derivative(const arg_list &args, size_type,
                    base_tensor &result) const {
      const base_tensor &t = *args[0];
      size_type N = t.size();
#   if 1
      scalar_type eps = 1E-25;
      scalar_type no = ::sqrt(gmm::vect_norm2_sqr(t.as_vector())+gmm::sqr(eps));
      scalar_type no3 = no*no*no;

      gmm::clear(result.as_vector());
      for (size_type i = 0; i < N; ++i) {
        result[i*N+i] += scalar_type(1)/no;
        for (size_type j = 0; j < N; ++j)
          result[j*N+i] -= t[i]*t[j] / no3;
      }
#   else
      scalar_type no = gmm::vect_norm2(t.as_vector());

      gmm::clear(result.as_vector());
      if (no >= 1E-15) {
        scalar_type no3 = no*no*no;
        for (size_type i = 0; i < N; ++i) {
          result[i*N+i] += scalar_type(1)/no;
          for (size_type j = 0; j < N; ++j)
            result[j*N+i] -= t[i]*t[j] / no3;
        }
      }
#   endif
    }

    // Second derivative : not implemented
    void second_derivative(const arg_list &/*args*/, size_type, size_type,
                           base_tensor &/*result*/) const {
      GMM_ASSERT1(false, "Sorry, second derivative not implemented");
    }
  };


  // Ball Projection operator.
  struct Ball_projection_operator : public ga_nonlinear_operator {
    bool result_size(const arg_list &args, bgeot::multi_index &sizes) const {
      if (args.size() != 2 || args[0]->sizes().size() > 2
          || (args[0]->sizes().size() < 1 && args[0]->size() != 1)
          || args[1]->size() != 1) return false;
      if (args[0]->sizes().size() < 1)
        ga_init_scalar(sizes);
      else if (args[0]->sizes().size() == 1)
        ga_init_vector(sizes, args[0]->sizes()[0]);
      else
        ga_init_matrix(sizes, args[0]->sizes()[0], args[0]->sizes()[1]);
      return true;
    }

    // Value : ru/|u| if |u| > r, else u 
    void value(const arg_list &args, base_tensor &result) const {
      const base_tensor &t = *args[0];
      scalar_type r = (*args[1])[0];
      scalar_type no = gmm::vect_norm2(t.as_vector());
      if (no > r)
	gmm::copy(gmm::scaled(t.as_vector(), r/no), result.as_vector());
      else
	gmm::copy(t.as_vector(), result.as_vector());
    }

    // Derivative
    void derivative(const arg_list &args, size_type n,
                    base_tensor &result) const {
      const base_tensor &t = *args[0];
      size_type N = t.size();
      scalar_type r = (*args[1])[0];
      scalar_type no = gmm::vect_norm2(t.as_vector()), rno3 = r/(no*no*no);

      gmm::clear(result.as_vector());

      switch(n) {

      case 1 : // derivative with respect to u
	if (r > 0) {
	  if (no <= r) {
	    for (size_type i = 0; i < N; ++i)
	      result[i*N+i] += scalar_type(1);
	  } else {
	    for (size_type i = 0; i < N; ++i) {
	      result[i*N+i] += r/no;
	      for (size_type j = 0; j < N; ++j)
		result[j*N+i] -= t[i]*t[j]*rno3;
	    }
	  }
	}
	break;
      case 2 : // derivative with respect to r
	if (r > 0 && no > r) {
	  for (size_type i = 0; i < N; ++i)
	    result[i] = t[i]/no;
	}
	break;
      default : GMM_ASSERT1(false, "Wrong derivative number");
      }
    }
    
    // Second derivative : not implemented
    void second_derivative(const arg_list &/*args*/, size_type, size_type,
                           base_tensor &/*result*/) const {
      GMM_ASSERT1(false, "Sorry, second derivative not implemented");
    }
  };


  // Normalized_reg vector/matrix operator : Regularized Vector/matrix divided by its Frobenius norm
  struct normalized_reg_operator : public ga_nonlinear_operator {
    bool result_size(const arg_list &args, bgeot::multi_index &sizes) const {
      if (args.size() != 2 || args[0]->sizes().size() > 2
          || args[0]->sizes().size() < 1 || args[1]->size() != 1) return false;
      if (args[0]->sizes().size() == 1)
        ga_init_vector(sizes, args[0]->sizes()[0]);
      else
        ga_init_matrix(sizes, args[0]->sizes()[0], args[0]->sizes()[1]);
      return true;
    }

    // Value : u/(sqrt([u|^2+\eps^2))
    void value(const arg_list &args, base_tensor &result) const {
      const base_tensor &t = *args[0];
      scalar_type eps = (*args[1])[0];
      scalar_type no = ::sqrt(gmm::vect_norm2_sqr(t.as_vector())+gmm::sqr(eps));
      gmm::copy(gmm::scaled(t.as_vector(), scalar_type(1)/no),
                result.as_vector());
    }

    // Derivative / u : ((|u|^2+eps^2) Id - u x u)/(|u|^2+eps^2)^(3/2)
    // Derivative / eps : -eps*u/(|u|^2+eps^2)^(3/2)
    void derivative(const arg_list &args, size_type nder,
                    base_tensor &result) const {
      const base_tensor &t = *args[0];
      scalar_type eps = (*args[1])[0];

      size_type N = t.size();
      scalar_type no = ::sqrt(gmm::vect_norm2_sqr(t.as_vector())+gmm::sqr(eps));
      scalar_type no3 = no*no*no;

      switch (nder) {
      case 1:
        gmm::clear(result.as_vector());
        for (size_type i = 0; i < N; ++i) {
          result[i*N+i] += scalar_type(1)/no;
          for (size_type j = 0; j < N; ++j)
            result[j*N+i] -= t[i]*t[j] / no3;
        }
        break;

      case 2:
        gmm::copy(gmm::scaled(t.as_vector(), -scalar_type(eps)/no3),
                  result.as_vector());
        break;
      }
    }

    // Second derivative : not implemented
    void second_derivative(const arg_list &/*args*/, size_type, size_type,
                           base_tensor &/*result*/) const {
      GMM_ASSERT1(false, "Sorry, second derivative not implemented");
    }
  };


  //=================================================================
  // Von Mises projection
  //=================================================================


  struct Von_Mises_projection_operator : public ga_nonlinear_operator {
    bool result_size(const arg_list &args, bgeot::multi_index &sizes) const {
      if (args.size() != 2 || args[0]->sizes().size() > 2
          || args[1]->size() != 1) return false;
      size_type N = (args[0]->sizes().size() == 2) ?  args[0]->sizes()[0] : 1;
      if (args[0]->sizes().size() == 2 && args[0]->sizes()[1] != N) return false;
      if (args[0]->sizes().size() != 2 && args[0]->size() != 1)  return false;
      if (N > 1) ga_init_square_matrix(sizes, N); else ga_init_scalar(sizes);
      return true;
    }

    // Value:
    void value(const arg_list &args, base_tensor &result) const {
      size_type N = (args[0]->sizes().size() == 2) ? args[0]->sizes()[0] : 1;
      base_matrix tau(N, N), tau_D(N, N);
      gmm::copy(args[0]->as_vector(), tau.as_vector());
      scalar_type s = (*(args[1]))[0];

      scalar_type tau_m = gmm::mat_trace(tau) / scalar_type(N);
      gmm::copy(tau, tau_D);
      for (size_type i = 0; i < N; ++i) tau_D(i,i) -= tau_m;

      scalar_type norm_tau_D = gmm::mat_euclidean_norm(tau_D);

      if (norm_tau_D > s) gmm::scale(tau_D, s / norm_tau_D);

      for (size_type i = 0; i < N; ++i) tau_D(i,i) += tau_m;

      gmm::copy(tau_D.as_vector(), result.as_vector());
    }

    // Derivative:
    void derivative(const arg_list &args, size_type nder,
                    base_tensor &result) const {
      size_type N = (args[0]->sizes().size() == 2) ? args[0]->sizes()[0] : 1;
      base_matrix tau(N, N), tau_D(N, N);
      gmm::copy(args[0]->as_vector(), tau.as_vector());
      scalar_type s = (*(args[1]))[0];
      scalar_type tau_m = gmm::mat_trace(tau) / scalar_type(N);
      gmm::copy(tau, tau_D);
      for (size_type i = 0; i < N; ++i) tau_D(i,i) -= tau_m;
      scalar_type norm_tau_D = gmm::mat_euclidean_norm(tau_D);

      if (norm_tau_D != scalar_type(0))
        gmm::scale(tau_D, scalar_type(1)/norm_tau_D);

      switch(nder) {
      case 1:
        if (norm_tau_D <= s) {
          gmm::clear(result.as_vector());
          for (size_type i = 0; i < N; ++i)
            for (size_type j = 0; j < N; ++j)
              result(i,j,i,j) = scalar_type(1);
        } else {
          for (size_type i = 0; i < N; ++i)
            for (size_type j = 0; j < N; ++j)
              for (size_type m = 0; m < N; ++m)
                for (size_type n = 0; n < N; ++n)
                  result(i,j,m,n)
                    = s * (-tau_D(i,j) * tau_D(m,n)
                           + ((i == m && j == n) ? scalar_type(1) : scalar_type(0))
                           - ((i == j && m == n) ? scalar_type(1)/scalar_type(N)
                              : scalar_type(0))) / norm_tau_D;
          for (size_type i = 0; i < N; ++i)
            for (size_type j = 0; j < N; ++j)
              result(i,i,j,j) += scalar_type(1)/scalar_type(N);
        }
        break;
      case 2:
        if (norm_tau_D < s)
          gmm::clear(result.as_vector());
        else
          gmm::copy(tau_D.as_vector(), result.as_vector());
        break;
      }
    }

    // Second derivative : not implemented
    void second_derivative(const arg_list &, size_type, size_type,
                           base_tensor &) const {
      GMM_ASSERT1(false, "Sorry, second derivative not implemented");
    }
  };

  static bool init_predef_operators(void) {

    ga_predef_operator_tab &PREDEF_OPERATORS
      = dal::singleton<ga_predef_operator_tab>::instance();

    PREDEF_OPERATORS.add_method("Expm",
                                std::make_shared<matrix_exponential_operator>());
    PREDEF_OPERATORS.add_method("Logm",
                                std::make_shared<matrix_logarithm_operator>());
    PREDEF_OPERATORS.add_method("Normalized",
                                std::make_shared<normalized_operator>());
    PREDEF_OPERATORS.add_method("Normalized_reg",
                                std::make_shared<normalized_reg_operator>());
    PREDEF_OPERATORS.add_method("Ball_projection",
                                std::make_shared<Ball_projection_operator>());
    PREDEF_OPERATORS.add_method("Von_Mises_projection",
                                std::make_shared<Von_Mises_projection_operator>());
    return true;
   }

  // declared in getfem_generic_assembly.cc
  bool predef_operators_plasticity_initialized
    = init_predef_operators();


  // ======================================
  //
  // Small strain plasticity brick
  //
  // ======================================


  // Assembly strings for isotropic perfect elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model). With the use of a plastic multiplier
  void build_isotropic_perfect_elastoplasticity_expressions_mult
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &lambda,
   const std::string &mu, const std::string &sigma_y,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &compcond,
   std::string &sigma_after, std::string &von_mises) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(!(md.is_data(xi)) && md.pmesh_fem_of_variable(xi),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined as a fem variable");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*(2*(mu)*(dt)*(Previous_xi))*(Deviator(En)-(Epn)))",
       dict);
    Epnp1 = ga_substitute
      ("((zetan)+(1-1/(1+(theta)*2*(mu)*(dt)*(xi)))*(Deviator(Enp1)-(zetan)))",
       dict);
    dict["Epnp1"] = Epnp1;
    sigma_np1 = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1)))", dict);
    dict["fbound"] = ga_substitute
      ("(2*(mu)*Norm(Deviator(Enp1)-(Epnp1))-sqrt(2/3)*(sigma_y))",  dict);
    dict["sigma_after"] = sigma_after = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn)))", dict);
    compcond = ga_substitute
      ("((mu)*xi-pos_part((mu)*xi+100*(fbound)/(mu)))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*Norm(Deviator(sigma_after))", dict);
  }


  // Assembly strings for isotropic perfect elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model). Without the use of a plastic multiplier
  void build_isotropic_perfect_elastoplasticity_expressions_no_mult
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &lambda,
   const std::string &mu, const std::string &sigma_y,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &xi_np1,
   std::string &sigma_after, std::string &von_mises) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(md.is_data(xi) &&
                (md.pim_data_of_variable(xi) ||
                 md.pmesh_fem_of_variable(xi)),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined either as fem data or as im data");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict) ;


    dict["zetan"] = ga_substitute
      ("(Epn)+(1-(theta))*(2*(mu)*(dt)*(Previous_xi))*(Deviator(En)-(Epn))",
       dict);
    dict["B"] = ga_substitute("Deviator(Enp1)-(zetan)", dict);
    Epnp1 = ga_substitute
      ("(zetan)+pos_part(1-sqrt(2/3)*(sigma_y)/(2*(mu)*Norm(B)+1e-40))*(B)",
       dict);
    dict["Epnp1"] = Epnp1;

    sigma_np1 = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1))", dict);
    dict["sigma_after"] = sigma_after = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn))", dict);
    xi_np1 = ga_substitute
      ("pos_part(sqrt(3/2)*Norm(B)/(sigma_y)-1/(2*(mu)))/((theta)*(dt))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*Norm(Deviator(sigma_after))", dict);
  }

  // Assembly strings for isotropic perfect elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model). With the use of a plastic multiplier
  // and plane strain version.
  void build_isotropic_perfect_elastoplasticity_expressions_mult_ps
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &lambda,
   const std::string &mu, const std::string &sigma_y,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &compcond,
   std::string &sigma_after, std::string &von_mises) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(N == 2, "This plastic law is restricted to 2D");

    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(!(md.is_data(xi)) && md.pmesh_fem_of_variable(xi),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined as a fem variable");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict);
    dict["Dev_En"]= ga_substitute("(En-(Trace(En)/3)*Id(meshdim))", dict);
    dict["Dev_Enp1"]= ga_substitute("(Enp1-(Trace(Enp1)/3)*Id(meshdim))", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*(2*(mu)*(dt)*(Previous_xi))*((Dev_En)-(Epn)))",
       dict);
    Epnp1 = ga_substitute
      ("((zetan)+(1-1/(1+(theta)*2*(mu)*(dt)*(xi)))*((Dev_Enp1)-(zetan)))",
       dict);
    dict["Epnp1"] = Epnp1;
    sigma_np1 = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1)))", dict);
    dict["fbound"] = ga_substitute
      ("(2*(mu)*sqrt(Norm_sqr(Dev_Enp1-(Epnp1))"
       "+sqr(Trace(Enp1)/3-Trace(Epnp1)))-sqrt(2/3)*(sigma_y))", dict);

    sigma_after = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn)))", dict);
    compcond = ga_substitute
      ("((mu)*xi-pos_part((mu)*xi+100*(fbound)/(mu)))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*sqrt(Norm_sqr((2*(mu))*(Dev_En)-(2*(mu))*(Epn))"
       "+sqr(2*(mu)*Trace(En)/3-(2*(mu))*Trace(Epn)))", dict);
  }


  // Assembly strings for isotropic perfect elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model). Without the use of a plastic multiplier
  // and plane strain version.
  void build_isotropic_perfect_elastoplasticity_expressions_no_mult_ps
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &lambda,
   const std::string &mu, const std::string &sigma_y,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1,
   std::string &xi_np1, std::string &sigma_after, std::string &von_mises) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(N == 2, "This plastic law is restricted to 2D");

    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(md.is_data(xi) &&
                (md.pim_data_of_variable(xi) ||
                 md.pmesh_fem_of_variable(xi)),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined either as fem data or as im data");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict) ;
    dict["Dev_En"]= ga_substitute("(En-(Trace(En)/3)*Id(meshdim))", dict);
    dict["Dev_Enp1"]= ga_substitute("(Enp1-(Trace(Enp1)/3)*Id(meshdim))", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*(2*(mu)*(dt)*(Previous_xi))*(Dev_En-(Epn)))",
       dict);
    dict["B"] = ga_substitute("(Dev_Enp1)-(zetan)", dict);
    Epnp1 = ga_substitute
      ("(zetan)+pos_part(1-sqrt(2/3)*(sigma_y)/(2*(mu)*(sqrt(Norm_sqr(B)+"
       "sqr(Trace(Enp1)/3-Trace(zetan))))+1e-25))*(B)", dict);
    dict["Epnp1"] = Epnp1;

    sigma_np1 = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1))", dict);
    sigma_after = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn))", dict);
    xi_np1 = ga_substitute
      ("pos_part(sqrt(3/2)*Norm(B)/(sigma_y)-1/(2*(mu)))/((theta)*(dt))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*sqrt(Norm_sqr((2*(mu))*(Dev_En)-(2*(mu))*(Epn))"
       "+sqr(2*(mu)*Trace(En)/3-(2*(mu))*Trace(Epn)))", dict);
  }

  // Assembly strings for isotropic elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model) and linear isotropic and kinematic
  // hardening. With the use of a plastic multiplier
  void build_isotropic_perfect_elastoplasticity_expressions_hard_mult
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &alpha,
   const std::string &lambda, const std::string &mu,
   const std::string &sigma_y, const std::string &Hk, const std::string &Hi,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &compcond,
   std::string &sigma_after, std::string &von_mises, std::string &alphanp1) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(!(md.is_data(xi)) && md.pmesh_fem_of_variable(xi),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined as a fem variable");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Hk"] = Hk; dict["Hi"] = Hi; dict["alphan"] = alpha;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*((dt)*(Previous_xi))*((2*(mu))*Deviator(En)"
       "-(2*(mu)+2/3*(Hk))*(Epn)))", dict);
    dict["etan"] = ga_substitute
      ("((alphan)+sqrt(2/3)*(1-(theta))*((dt)*(Previous_xi))*"
       "Norm((2*(mu))*Deviator(En)-(2*(mu)+2/3*(Hk))*(Epn)))", dict);
    dict["B"] = ga_substitute
      ("((2*(mu))*Deviator(Enp1)-(2*(mu)+2/3*(Hk))*(zetan))", dict);
    dict["beta"] = ga_substitute
      ("((theta)*(dt)*(xi)/(1+(2*(mu)+2/3*(Hk))*(theta)*(dt)*(xi)))", dict);
    dict["Epnp1"] = Epnp1 = ga_substitute("((zetan)+(beta)*(B))", dict);
    alphanp1 = ga_substitute("((etan)+sqrt(2/3)*(beta)*Norm(B))", dict);
    dict["alphanp1"] = alphanp1;
    sigma_np1 = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1)))", dict);
    dict["fbound"] = ga_substitute
      ("(Norm((2*(mu))*Deviator(Enp1)-(2*(mu)+2/3*(Hk))*(Epnp1))"
       "-sqrt(2/3)*(sigma_y+(Hi)*(alphanp1)))",  dict);
    dict["sigma_after"] = sigma_after = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn)))", dict);
    compcond = ga_substitute
      ("((mu)*xi-pos_part((mu)*xi+100*(fbound)/(mu)))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*Norm(Deviator(sigma_after))", dict);
  }

  // Assembly strings for isotropic elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model) and linear isotropic and kinematic
  // hardening. Without the use of a plastic multiplier
  void build_isotropic_perfect_elastoplasticity_expressions_hard_no_mult
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &alpha,
   const std::string &lambda, const std::string &mu,
   const std::string &sigma_y, const std::string &Hk, const std::string &Hi,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &xi_np1,
   std::string &sigma_after, std::string &von_mises, std::string &alphanp1) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(md.is_data(xi) &&
                (md.pim_data_of_variable(xi) ||
                 md.pmesh_fem_of_variable(xi)),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined either as fem data or as im data");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Hk"] = Hk; dict["Hi"] = Hi; dict["alphan"] = alpha;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*((dt)*(Previous_xi))*((2*(mu))*Deviator(En)"
       "-(2*(mu)+2/3*(Hk))*(Epn)))", dict);
    dict["etan"] = ga_substitute
      ("((alphan)+sqrt(2/3)*(1-(theta))*((dt)*(Previous_xi))*"
       "Norm((2*(mu))*Deviator(En)-(2*(mu)+2/3*(Hk))*(Epn)))", dict);

    dict["B"] = ga_substitute
      ("((2*(mu))*Deviator(Enp1)-(2*(mu)+2/3*(Hk))*(zetan))", dict);
    dict["beta"] =
      ga_substitute("(1/((Norm(B)+1e-40)*(2*(mu)+2/3*(Hk)+(2/3)*(Hi))))"
      "*pos_part(Norm(B)-sqrt(2/3)*((sigma_y)+(Hi)*(etan)))", dict);
    dict["Epnp1"] = Epnp1 = ga_substitute("((zetan)+(beta)*(B))", dict);
    alphanp1 = ga_substitute("((etan)+sqrt(2/3)*(beta)*Norm(B))", dict);
    dict["alphanp1"] = alphanp1;

    sigma_np1 = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1))", dict);
    dict["sigma_after"] = sigma_after = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn))", dict);
    xi_np1 = ga_substitute
      ("(((beta)/(1-(2*(mu)+2/3*(Hk))*(beta)))/((theta)*(dt)))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*Norm(Deviator(sigma_after))", dict);
  }

  // Assembly strings for isotropic elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model) and linear isotropic and kinematic
  // hardening. With the use of a plastic multiplier and plane strain version.
  void build_isotropic_perfect_elastoplasticity_expressions_hard_mult_ps
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &alpha,
   const std::string &lambda, const std::string &mu,
   const std::string &sigma_y, const std::string &Hk, const std::string &Hi,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &compcond,
   std::string &sigma_after, std::string &von_mises, std::string &alphanp1) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(N == 2, "This plastic law is restricted to 2D");

    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(!(md.is_data(xi)) && md.pmesh_fem_of_variable(xi),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined as a fem variable");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Hk"] = Hk; dict["Hi"] = Hi; dict["alphan"] = alpha;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict);
    dict["Dev_En"]= ga_substitute("(En-(Trace(En)/3)*Id(meshdim))", dict);
    dict["Dev_Enp1"]= ga_substitute("(Enp1-(Trace(Enp1)/3)*Id(meshdim))", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*((dt)*(Previous_xi))*((2*(mu))*(Dev_En)"
       "-(2*(mu)+2/3*(Hk))*(Epn)))", dict);
    dict["etan"] = ga_substitute
      ("((alphan)+sqrt(2/3)*(1-(theta))*((dt)*(Previous_xi))*"
       "sqrt(Norm_sqr((2*(mu))*(Dev_En)-(2*(mu)+2/3*(Hk))*(Epn))"
       "+sqr(2*(mu)*Trace(En)/3-(2*(mu)+2/3*(Hk))*Trace(Epn))))", dict);
    dict["B"] = ga_substitute("((2*(mu))*(Dev_Enp1)-(2*(mu)+2/3*(Hk))*(zetan))",
                              dict);
    dict["Norm_B"] = ga_substitute("sqrt(Norm_sqr(B)+sqr(2*(mu)*Trace(Enp1)/3"
                                   "-(2*(mu)+2/3*(Hk))*Trace(zetan)))", dict);

    dict["beta"] = ga_substitute
      ("((theta)*(dt)*(xi)/(1+(2*(mu)+2/3*(Hk))*(theta)*(dt)*(xi)))", dict);
    dict["Epnp1"] = Epnp1 = ga_substitute("((zetan)+(beta)*(B))", dict);
    alphanp1 = ga_substitute("((etan)+sqrt(2/3)*(beta)*(Norm_B))", dict);
    dict["alphanp1"] = alphanp1;
    sigma_np1 = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1)))", dict);
    dict["fbound"] = ga_substitute
      ("(sqrt(Norm_sqr((2*(mu))*(Dev_Enp1)-(2*(mu)+2/3*(Hk))*(Epnp1))"
       "+sqr(2*(mu)*Trace(Enp1)/3-(2*(mu)+2/3*(Hk))*Trace(Epnp1)))"
       "-sqrt(2/3)*(sigma_y+(Hi)*(alphanp1)))",  dict);
    sigma_after = ga_substitute
      ("((lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn)))", dict);
    compcond = ga_substitute
      ("((mu)*xi-pos_part((mu)*xi+100*(fbound)/(mu)))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*sqrt(Norm_sqr((2*(mu))*(Dev_En)-(2*(mu)+2/3*(Hk))*(Epn))"
       "+sqr(2*(mu)*Trace(En)/3-(2*(mu)+2/3*(Hk))*Trace(Epn)))", dict);
  }

  // Assembly strings for isotropic elastoplasticity with Von-Mises
  // criterion (Prandtl-Reuss model) and linear isotropic and kinematic
  // hardening.
  // Without the use of a plastic multiplier and plane strain version.
  void build_isotropic_perfect_elastoplasticity_expressions_hard_no_mult_ps
  (model &md, const std::string &dispname, const std::string &xi,
   const std::string &Previous_Ep, const std::string &alpha,
   const std::string &lambda, const std::string &mu,
   const std::string &sigma_y, const std::string &Hk, const std::string &Hi,
   const std::string &theta, const std::string &dt,
   std::string &sigma_np1, std::string &Epnp1, std::string &xi_np1,
   std::string &sigma_after, std::string &von_mises, std::string &alphanp1) {

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(N == 2, "This plastic law is restricted to 2D");

    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The small strain "
               "elastoplasticity brick can only be applied on a fem "
               "variable of the same dimension as the mesh");

    GMM_ASSERT1(md.is_data(xi) &&
                (md.pim_data_of_variable(xi) ||
                 md.pmesh_fem_of_variable(xi)),
                "The provided name '" << xi << "' for the plastic multiplier, "
                "should be defined either as fem data or as im data");

    GMM_ASSERT1(md.is_data(Previous_Ep) &&
                (md.pim_data_of_variable(Previous_Ep) ||
                 md.pmesh_fem_of_variable(Previous_Ep)),
                "The provided name '" << Previous_Ep << "' for the plastic "
                "strain tensor at the previous timestep, should be defined "
                "either as fem or as im data");

    bgeot::multi_index Ep_size(N, N);
    GMM_ASSERT1((md.pim_data_of_variable(Previous_Ep) &&
                 md.pim_data_of_variable(Previous_Ep)->tensor_size() == Ep_size)
                ||
                (md.pmesh_fem_of_variable(Previous_Ep) &&
                 md.pmesh_fem_of_variable(Previous_Ep)->get_qdims() == Ep_size),
                "Wrong size of " << Previous_Ep);

    std::map<std::string, std::string> dict;
    dict["Hk"] = Hk; dict["Hi"] = Hi; dict["alphan"] = alpha;
    dict["Grad_u"] = "Grad_"+dispname; dict["xi"] = xi;
    dict["Previous_xi"] = "Previous_"+xi;
    dict["Grad_Previous_u"] = "Grad_Previous_"+dispname;
    dict["theta"] = theta; dict["dt"] = dt; dict["Epn"] = Previous_Ep;
    dict["lambda"] = lambda; dict["mu"] = mu; dict["sigma_y"] = sigma_y;

    dict["Enp1"] = ga_substitute("Sym(Grad_u)", dict);
    dict["En"] = ga_substitute("Sym(Grad_Previous_u)", dict);
    dict["Dev_En"]= ga_substitute("(En-(Trace(En)/3)*Id(meshdim))", dict);
    dict["Dev_Enp1"]= ga_substitute("(Enp1-(Trace(Enp1)/3)*Id(meshdim))", dict);
    dict["zetan"] = ga_substitute
      ("((Epn)+(1-(theta))*((dt)*(Previous_xi))*((2*(mu))*(Dev_En)"
       "-(2*(mu)+2/3*(Hk))*(Epn)))", dict);
    dict["etan"] = ga_substitute
      ("((alphan)+sqrt(2/3)*(1-(theta))*((dt)*(Previous_xi))*"
       "sqrt(Norm_sqr((2*(mu))*(Dev_En)-(2*(mu)+2/3*(Hk))*(Epn))"
       "+sqr(2*(mu)*Trace(En)/3-(2*(mu)+2/3*(Hk))*Trace(Epn))))", dict);

    dict["B"] = ga_substitute
      ("((2*(mu))*(Dev_Enp1)-(2*(mu)+2/3*(Hk))*(zetan))", dict);
    dict["Norm_B"] = ga_substitute("sqrt(Norm_sqr(B)+sqr(2*(mu)*Trace(Enp1)/3"
                                   "-(2*(mu)+2/3*(Hk))*Trace(zetan)))", dict);
    dict["beta"] =
      ga_substitute("(1/(((Norm_B)+1e-40)*(2*(mu)+2/3*(Hk)+(2/3)*(Hi))))"
      "*pos_part((Norm_B)-sqrt(2/3)*((sigma_y)+(Hi)*(etan)))", dict);
    dict["Epnp1"] = Epnp1 = ga_substitute("((zetan)+(beta)*(B))", dict);
    alphanp1 = ga_substitute("((etan)+sqrt(2/3)*(beta)*(Norm_B))", dict);
    dict["alphanp1"] = alphanp1;

    sigma_np1 = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epnp1))", dict);
    sigma_after = ga_substitute
      ("(lambda)*Trace(Enp1)*Id(meshdim)+2*(mu)*((Enp1)-(Epn))", dict);
    xi_np1 = ga_substitute
      ("(((beta)/(1-(2*(mu)+2/3*(Hk))*(beta)))/((theta)*(dt)))", dict);
    von_mises = ga_substitute
      ("sqrt(3/2)*sqrt(Norm_sqr((2*(mu))*(Dev_En)-(2*(mu)+2/3*(Hk))*(Epn))"
       "+sqr(2*(mu)*Trace(En)/3-(2*(mu)+2/3*(Hk))*Trace(Epn)))", dict);
  }

  void build_isotropic_perfect_elastoplasticity_expressions_generic
  (model &md, const std::string &lawname,
   plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params,
   std::string &sigma_np1, std::string &Epnp1,
   std::string &compcond, std::string &xi_np1,
   std::string &sigma_after, std::string &von_mises,
   std::string &alphanp1) {

    GMM_ASSERT1(unknowns_type == DISPLACEMENT_ONLY ||
                unknowns_type == DISPLACEMENT_AND_PLASTIC_MULTIPLIER,
                "Not supported type of unknowns");
    bool plastic_multiplier_is_var
      = (unknowns_type == DISPLACEMENT_AND_PLASTIC_MULTIPLIER);

    bool hardening = (lawname.find("_hardening") != std::string::npos);
    size_type nhard = hardening ? 2 : 0;

    GMM_ASSERT1(varnames.size() == (hardening ? 4 : 3),
                "Incorrect number of variables: " << varnames.size());
    GMM_ASSERT1(params.size() >= 3+nhard &&
                params.size() <= 5+nhard,
                "Incorrect number of parameters: " << params.size());
    const std::string &dispname = sup_previous_and_dot_to_varname(varnames[0]);
    const std::string &xi       = sup_previous_and_dot_to_varname(varnames[1]);
    const std::string &Previous_Ep = varnames[2];
    const std::string &alpha       = hardening ? varnames[3] : "";
    const std::string &lambda      = params[0];
    const std::string &mu          = params[1];
    const std::string &sigma_y     = params[2];
    const std::string &Hk          = hardening ? params[3] : "";
    const std::string &Hi          = hardening ? params[4] : "";
    const std::string &theta       = (params.size() >= 4+nhard)
                                   ? params[3+nhard] : "1";
    const std::string &dt          = (params.size() >= 5+nhard)
                                   ? params[4+nhard] : "timestep";

    sigma_np1 = Epnp1 = compcond = xi_np1 = "";;
    sigma_after = von_mises = alphanp1 = "";

    if (lawname.compare("isotropic_perfect_plasticity") == 0 ||
        lawname.compare("prandtl_reuss") == 0) {
      if (plastic_multiplier_is_var) {
        build_isotropic_perfect_elastoplasticity_expressions_mult
          (md, dispname, xi, Previous_Ep, lambda, mu, sigma_y, theta, dt,
           sigma_np1, Epnp1, compcond, sigma_after, von_mises);
      } else {
        build_isotropic_perfect_elastoplasticity_expressions_no_mult
          (md, dispname, xi, Previous_Ep, lambda, mu, sigma_y, theta, dt,
           sigma_np1, Epnp1, xi_np1, sigma_after, von_mises);
      }
    } else if (lawname.compare("plane_strain_isotropic_perfect_plasticity")
               == 0 ||
               lawname.compare("plane_strain_prandtl_reuss") == 0) {
      if (plastic_multiplier_is_var) {
        build_isotropic_perfect_elastoplasticity_expressions_mult_ps
          (md, dispname, xi, Previous_Ep, lambda, mu, sigma_y, theta, dt,
           sigma_np1, Epnp1, compcond, sigma_after, von_mises);
      } else {
        build_isotropic_perfect_elastoplasticity_expressions_no_mult_ps
          (md, dispname, xi, Previous_Ep, lambda, mu, sigma_y, theta, dt,
           sigma_np1, Epnp1, xi_np1, sigma_after, von_mises);
      }
    } else if (lawname.compare("isotropic_plasticity_linear_hardening") == 0 ||
               lawname.compare("prandtl_reuss_linear_hardening") == 0) {
      if (plastic_multiplier_is_var) {
        build_isotropic_perfect_elastoplasticity_expressions_hard_mult
          (md, dispname, xi, Previous_Ep, alpha,
           lambda, mu, sigma_y, Hk, Hi, theta, dt,
           sigma_np1, Epnp1, compcond, sigma_after, von_mises, alphanp1);
      } else {
        build_isotropic_perfect_elastoplasticity_expressions_hard_no_mult
          (md, dispname, xi, Previous_Ep, alpha,
           lambda, mu, sigma_y, Hk, Hi, theta, dt,
           sigma_np1, Epnp1, xi_np1, sigma_after, von_mises, alphanp1);
      }
    } else if
        (lawname.compare("plane_strain_isotropic_plasticity_linear_hardening")
         == 0 ||
         lawname.compare("plane_strain_prandtl_reuss_linear_hardening") == 0) {
      if (plastic_multiplier_is_var) {
        build_isotropic_perfect_elastoplasticity_expressions_hard_mult_ps
          (md, dispname, xi, Previous_Ep, alpha,
           lambda, mu, sigma_y, Hk, Hi, theta, dt,
           sigma_np1, Epnp1, compcond, sigma_after, von_mises, alphanp1);
      } else {
        build_isotropic_perfect_elastoplasticity_expressions_hard_no_mult_ps
          (md, dispname, xi, Previous_Ep, alpha,
           lambda, mu, sigma_y, Hk, Hi, theta, dt,
           sigma_np1, Epnp1, xi_np1, sigma_after, von_mises, alphanp1);
      }
    } else
      GMM_ASSERT1(false, lawname << " is not an implemented elastoplastic law");
  }

  static void filter_lawname(std::string &lawname) {
    for (auto &c : lawname)
      { if (c == ' ') c = '_'; if (c >= 'A' && c <= 'Z') c = char(c+'a'-'A'); }
  }

  size_type add_small_strain_elastoplasticity_brick
  (model &md, const mesh_im &mim,
   std::string lawname, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params, size_type region)  {

    filter_lawname(lawname);
    std::string sigma_np1, compcond;
    {
      std::string dum2, dum4, dum5, dum6, dum7;
      build_isotropic_perfect_elastoplasticity_expressions_generic
        (md, lawname, unknowns_type, varnames, params,
         sigma_np1, dum2, compcond, dum4, dum5, dum6, dum7);
    }

    const std::string dispname=sup_previous_and_dot_to_varname(varnames[0]);
    const std::string xi      =sup_previous_and_dot_to_varname(varnames[1]);

    if (unknowns_type == DISPLACEMENT_AND_PLASTIC_MULTIPLIER) {
      std::string expr = ("("+sigma_np1+"):Grad_Test_"+dispname
                          + "+("+compcond+")*Test_"+xi);
      return add_nonlinear_term
        (md, mim, expr, region, false, false,
         "Small strain isotropic perfect elastoplasticity brick");
    } else {
      return add_nonlinear_term
        (md, mim, "("+sigma_np1+"):Grad_Test_"+dispname, region, true, false,
         "Small strain isotropic perfect elastoplasticity brick");
    }
  }

  void small_strain_elastoplasticity_next_iter
  (model &md, const mesh_im &mim,
   std::string lawname, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params, size_type region)  {

    filter_lawname(lawname);
    std::string Epnp1, xi_np1, alphanp1;
    {
      std::string dum1, dum3, dum5, dum6;
      build_isotropic_perfect_elastoplasticity_expressions_generic
        (md, lawname, unknowns_type, varnames, params,
         dum1, Epnp1, dum3, xi_np1, dum5, dum6, alphanp1);
    }

    std::string disp = sup_previous_and_dot_to_varname(varnames[0]);
    std::string xi   = sup_previous_and_dot_to_varname(varnames[1]);
    std::string Previous_Ep = varnames[2];

    std::string Previous_alpha;
    base_vector tmpv_alpha;
    if (alphanp1.size()) { // Interpolation of the accumulated plastic strain
      Previous_alpha = varnames[3];
      tmpv_alpha.resize(gmm::vect_size(md.real_variable(Previous_alpha)));
      const im_data *pimd = md.pim_data_of_variable(Previous_alpha);
      if (pimd)
        ga_interpolation_im_data(md, alphanp1, *pimd, tmpv_alpha, region);
      else {
        const mesh_fem *pmf = md.pmesh_fem_of_variable(Previous_alpha);
        GMM_ASSERT1(pmf, "Provided data " << Previous_alpha
                    << " should be defined on a im_data or a mesh_fem object");
        ga_local_projection(md, mim, alphanp1, *pmf, tmpv_alpha, region);
      }
    }

    base_vector tmpv_xi;
    if (xi_np1.size()) { // Interpolation of the plastic multiplier for the
      // theta-scheme and the case without multiplier (return mapping)
      // Not really necessary for the Backward Euler scheme.
      tmpv_xi.resize(gmm::vect_size(md.real_variable(xi)));
      const im_data *pimd = md.pim_data_of_variable(xi);
      if (pimd)
        ga_interpolation_im_data(md, xi_np1, *pimd, tmpv_xi, region);
      else {
        const mesh_fem *pmf = md.pmesh_fem_of_variable(xi);
        GMM_ASSERT1(pmf, "Provided data " << xi
                    << " should be defined on a im_data or a mesh_fem object");
        ga_local_projection(md, mim, xi_np1, *pmf, tmpv_xi, region);
      }
    }

    base_vector tmpv_ep(gmm::vect_size(md.real_variable(Previous_Ep)));
    const im_data *pimd = md.pim_data_of_variable(Previous_Ep);
    if (pimd)
      ga_interpolation_im_data(md, Epnp1, *pimd, tmpv_ep, region);
    else {
      const mesh_fem *pmf = md.pmesh_fem_of_variable(Previous_Ep);
      GMM_ASSERT1(pmf, "Provided data " << Previous_Ep
                  << " should be defined on a im_data or a mesh_fem object");
      ga_local_projection(md, mim, Epnp1, *pmf, tmpv_ep, region);
    }

    if (xi_np1.size())
      gmm::copy(tmpv_xi, md.set_real_variable(xi));
    if (alphanp1.size())
      gmm::copy(tmpv_alpha, md.set_real_variable(Previous_alpha));
    gmm::copy(tmpv_ep, md.set_real_variable(Previous_Ep));
    gmm::copy(md.real_variable(disp), md.set_real_variable("Previous_"+disp));
    gmm::copy(md.real_variable(xi), md.set_real_variable("Previous_"+xi));
  }

  // To be called after next_iter, not before
  void compute_small_strain_elastoplasticity_Von_Mises
  (model &md, const mesh_im &mim,
   std::string lawname, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params,
   const mesh_fem &mf_vm, model_real_plain_vector &VM, size_type region) {

    GMM_ASSERT1(mf_vm.get_qdim() == 1,
                "Von mises stress can only be approximated on a scalar fem");
    VM.resize(mf_vm.nb_dof());

    filter_lawname(lawname);

    std::string sigma_after, von_mises;
    {
      std::string dum1, dum2, dum3, dum4, dum7;
      build_isotropic_perfect_elastoplasticity_expressions_generic
        (md, lawname, unknowns_type, varnames, params,
         dum1, dum2, dum3, dum4, sigma_after, von_mises, dum7);
    }

    size_type n_ep = 2; // Index of the plastic strain variable

    const im_data *pimd = md.pim_data_of_variable(varnames[n_ep]);
    if (pimd) {
      ga_local_projection(md, mim, von_mises, mf_vm, VM, region);
    }
    else {
      const mesh_fem *pmf = md.pmesh_fem_of_variable(varnames[n_ep]);
      GMM_ASSERT1(pmf, "Provided data " << varnames[n_ep]
                  << " should be defined on a im_data or a mesh_fem object");
      ga_interpolation_Lagrange_fem(md, von_mises, mf_vm, VM, region);
    }
  }



  // ==============================
  //
  // Finite strain elastoplasticity
  //
  // ==============================

  const std::string _TWOTHIRD_("0.6666666666666666667");
  const std::string _FIVETHIRD_("1.6666666666666666667");
  const std::string _SQRTTHREEHALF_("1.2247448713915889407");

  void ga_define_linear_hardening_function
  (const std::string &name, scalar_type sigma_y0, scalar_type H, bool frobenius)
  {
     if (frobenius) {
       sigma_y0 *= sqrt(2./3.);
       H *= 2./3.;
     }
     std::stringstream expr, der;
     expr << std::setprecision(17) << sigma_y0 << "+" << H << "*t";
     der << std::setprecision(17) << H;
     ga_define_function(name, 1, expr.str(), der.str());
  }

  void ga_define_Ramberg_Osgood_hardening_function
  (const std::string &name,
   scalar_type sigma_ref, scalar_type eps_ref, scalar_type n, bool frobenius)
  {
    scalar_type coef = sigma_ref / pow(eps_ref, 1./n);
    if (frobenius)
      coef *= pow(2./3., 0.5 + 0.5/n); // = sqrt(2/3) * sqrt(2/3)^(1/n)

    std::stringstream expr, der;
    expr << std::setprecision(17) << coef << "*pow(t+1e-12," << 1./n << ")";
    der << std::setprecision(17) << coef/n << "*pow(t+1e-12," << 1./n-1 << ")";
    ga_define_function(name, 1, expr.str(), der.str());
  }

  // Simo-Miehe
  void build_Simo_Miehe_elastoplasticity_expressions
  (model &md, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params,
   std::string &expr, std::string &plaststrain, std::string &invCp, std::string &vm)
  {
    GMM_ASSERT1(unknowns_type == DISPLACEMENT_AND_PLASTIC_MULTIPLIER ||
                unknowns_type == DISPLACEMENT_AND_PLASTIC_MULTIPLIER_AND_PRESSURE,
                "Not supported type of unknowns for this type of plasticity law");
    bool has_pressure_var(unknowns_type ==
                          DISPLACEMENT_AND_PLASTIC_MULTIPLIER_AND_PRESSURE);
    GMM_ASSERT1(varnames.size() == (has_pressure_var ? 5 : 4),
                "Wrong number of variables.");
    GMM_ASSERT1(params.size() == 3, "Wrong number of parameters, "
                                    << params.size() << " != 3.");
    const std::string &dispname     = sup_previous_and_dot_to_varname(varnames[0]);
    const std::string &multname     = sup_previous_and_dot_to_varname(varnames[1]);
    const std::string &pressname    = has_pressure_var ? varnames[2] : "";
    const std::string &plaststrain0 = varnames[has_pressure_var ? 3 : 2];
    const std::string &invCp0       = varnames[has_pressure_var ? 4 : 3];
    const std::string &K            = params[0];
    const std::string &G            = params[1];
    const std::string &sigma_y      = params[2];

    const mesh_fem *mfu = md.pmesh_fem_of_variable(dispname);
    GMM_ASSERT1(mfu, "The provided displacement variable " << dispname <<
                " has to be defined on a mesh_fem");
    size_type N = mfu->linked_mesh().dim();
    GMM_ASSERT1(N >= 2 && N <= 3,
                "Finite strain elastoplasticity brick works only in 2D or 3D");
    GMM_ASSERT1(mfu && mfu->get_qdim() == N, "The finite strain "
                "elastoplasticity brick can only be applied on a fem "
                "variable of the same dimension as the mesh");
    const mesh_fem *mfmult = md.pmesh_fem_of_variable(multname);
    GMM_ASSERT1(mfmult && mfmult->get_qdim() == 1, "The plastic multiplier "
                "for the finite strain elastoplasticity brick has to be a "
                "scalar fem variable");
    bool mixed(!pressname.empty());
    const mesh_fem *mfpress = mixed ? md.pmesh_fem_of_variable(pressname) : 0;
    GMM_ASSERT1(!mixed || (mfpress && mfpress->get_qdim() == 1),
                "The hydrostatic pressure multiplier for the finite strain "
                "elastoplasticity brick has to be a scalar fem variable");

    GMM_ASSERT1(ga_function_exists(sigma_y), "The provided isotropic "
                "hardening function name '" << sigma_y << "' is not defined");

    GMM_ASSERT1(md.is_data(plaststrain0) &&
                (md.pim_data_of_variable(plaststrain0) ||
                 md.pmesh_fem_of_variable(plaststrain0)),
                "The provided name '" << plaststrain0 << "' for the plastic "
                "strain field at the previous timestep, should be defined "
                "either as fem or as im data");
    GMM_ASSERT1((md.pim_data_of_variable(plaststrain0) &&
                 md.pim_data_of_variable(plaststrain0)->nb_tensor_elem() == 1) ||
                (md.pmesh_fem_of_variable(plaststrain0) &&
                 md.pmesh_fem_of_variable(plaststrain0)->get_qdim() == 1),
                "Wrong size of " << plaststrain0);
    GMM_ASSERT1(md.is_data(invCp0) &&
                (md.pim_data_of_variable(invCp0) ||
                 md.pmesh_fem_of_variable(invCp0)),
                "The provided name '" << invCp0 << "' for the inverse of the "
                "plastic right Cauchy-Green tensor field at the previous "
                "timestep, should be defined either as fem or as im data");
    bgeot::multi_index Cp_size(1);
    Cp_size[0] = 4 + (N==3)*2;
    GMM_ASSERT1((md.pim_data_of_variable(invCp0) &&
                 md.pim_data_of_variable(invCp0)->tensor_size() == Cp_size) ||
                (md.pmesh_fem_of_variable(invCp0) &&
                 md.pmesh_fem_of_variable(invCp0)->get_qdims() == Cp_size),
                "Wrong size of " << invCp0);

    const std::string _U_ = sup_previous_and_dot_to_varname(dispname);
    const std::string _KSI_ = sup_previous_and_dot_to_varname(multname);
    const std::string _I_(N == 2 ? "Id(2)" : "Id(3)");
    const std::string _F_("("+_I_+"+Grad_"+_U_+")");
    const std::string _J_("Det"+_F_); // in 2D assumes plane strain

    std::string _P_;
    if (mixed)
      _P_ = "-"+pressname+"*"+_J_;
    else
      _P_ = "("+K+")*log("+_J_+")";

    std::string _INVCP0_, _F3d_, _DEVLOGBETR_, _DEVLOGBETR_3D_;
    if (N == 2) { // plane strain
      _INVCP0_ = "([[[1,0,0],[0,0,0],[0,0,0]],"
                   "[[0,0,0],[0,1,0],[0,0,0]],"
                   "[[0,0,0],[0,0,0],[0,0,1]],"
                   "[[0,1,0],[1,0,0],[0,0,0]]]."+invCp0+")";
      _F3d_ = "(Id(3)+[[1,0,0],[0,1,0]]*Grad_"+_U_+"*[[1,0,0],[0,1,0]]')";
      _DEVLOGBETR_3D_ = "(Deviator(Logm("+_F3d_+"*"+_INVCP0_+"*"+_F3d_+"')))";
      _DEVLOGBETR_ = "([[[[1,0],[0,0]],[[0,1],[0,0]],[[0,0],[0,0]]],"
                       "[[[0,0],[1,0]],[[0,0],[0,1]],[[0,0],[0,0]]],"
                       "[[[0,0],[0,0]],[[0,0],[0,0]],[[0,0],[0,0]]]]"
                       ":"+_DEVLOGBETR_3D_+")";
    } else { // 3D
      _INVCP0_ = "([[[1,0,0],[0,0,0],[0,0,0]],"
                   "[[0,0,0],[0,1,0],[0,0,0]],"
                   "[[0,0,0],[0,0,0],[0,0,1]],"
                   "[[0,1,0],[1,0,0],[0,0,0]],"
                   "[[0,0,1],[0,0,0],[1,0,0]],"
                   "[[0,0,0],[0,0,1],[0,1,0]]]."+invCp0+")";
      _F3d_ = _F_;
      _DEVLOGBETR_3D_ =
      _DEVLOGBETR_ = "(Deviator(Logm("+_F_+"*"+_INVCP0_+"*"+_F_+"')))";
    }
    const std::string _DEVTAUTR_("("+G+"*"+_DEVLOGBETR_+")");
    const std::string _DEVTAUTR_3D_("("+G+"*"+_DEVLOGBETR_3D_+")");
    const std::string _DEVTAU_("((1-2*"+_KSI_+")*"+_DEVTAUTR_+")");
    const std::string _DEVTAU_3D_("((1-2*"+_KSI_+")*"+_DEVTAUTR_3D_+")");
    const std::string _TAU_("("+_P_+"*"+_I_+"+"+_DEVTAU_+")");

    const std::string _PLASTSTRAIN_("("+plaststrain0+"+"+_KSI_+"*Norm"+_DEVLOGBETR_3D_+")");
    const std::string _SIGMA_Y_(sigma_y+"("+_PLASTSTRAIN_+")");

    // results
    expr = _TAU_+":(Grad_Test_"+_U_+"*Inv"+_F_+")";
    if (mixed)
      expr += "+("+pressname+"/("+K+")+log("+_J_+")/"+_J_+")*Test_"+pressname;
    expr += "+(Norm"+_DEVTAU_+
            "-min("+_SIGMA_Y_+",Norm"+_DEVTAUTR_+"+1e-12*"+_KSI_+"))*Test_"+_KSI_;

    plaststrain = _PLASTSTRAIN_;

    if (N==2) invCp = "[[[1,0,0,0.0],[0,0,0,0.5],[0,0,0,0]],"
                       "[[0,0,0,0.5],[0,1,0,0.0],[0,0,0,0]],"
                       "[[0,0,0,0.0],[0,0,0,0.0],[0,0,1,0]]]";
    else invCp = "[[[1.0,0,0,0,0,0],[0,0,0,0.5,0,0],[0,0,0,0,0.5,0]],"
                  "[[0,0,0,0.5,0,0],[0,1.0,0,0,0,0],[0,0,0,0,0,0.5]],"
                  "[[0,0,0,0,0.5,0],[0,0,0,0,0,0.5],[0,0,1.0,0,0,0]]]";
    invCp += ":((Inv"+_F3d_+"*Expm(-"+_KSI_+"*"+_DEVLOGBETR_3D_+")*"+_F3d_+")*"+_INVCP0_+
              "*(Inv"+_F3d_+"*Expm(-"+_KSI_+"*"+_DEVLOGBETR_3D_+")*"+_F3d_+")')";

    vm = _SQRTTHREEHALF_+"*Norm("+_DEVTAU_+")/"+_J_;
  }

  size_type add_finite_strain_elastoplasticity_brick
  (model &md, const mesh_im &mim,
   std::string lawname, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params, size_type region)
  {
    filter_lawname(lawname);
    if (lawname.compare("simo_miehe") == 0 ||
        lawname.compare("eterovic_bathe") == 0) {
      std::string expr, dummy1, dummy2, dummy3;
      build_Simo_Miehe_elastoplasticity_expressions
        (md, unknowns_type, varnames, params, expr, dummy1, dummy2, dummy3);
      return add_nonlinear_term
        (md, mim, expr, region, true, false, "Simo Miehe elastoplasticity brick");
    } else
      GMM_ASSERT1(false, lawname << " is not a known elastoplastic law");
  }

  // Updates any state variables included in params for the given lawname
  void finite_strain_elastoplasticity_next_iter
  (model &md, const mesh_im &mim,
   std::string lawname, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params, size_type region) {

    filter_lawname(lawname);
    if (lawname.compare("simo_miehe") == 0 ||
        lawname.compare("eterovic_bathe") == 0) {
      std::string dummy1, dummy2, plaststrain, invCp;
      build_Simo_Miehe_elastoplasticity_expressions
        (md, unknowns_type, varnames, params, dummy1, plaststrain, invCp, dummy2);

      bool has_pressure_var(unknowns_type ==
                            DISPLACEMENT_AND_PLASTIC_MULTIPLIER_AND_PRESSURE);
      const std::string &multname     = sup_previous_and_dot_to_varname(varnames[1]);
      const std::string &plaststrain0 = varnames[has_pressure_var ? 3 : 2];
      const std::string &invCp0       = varnames[has_pressure_var ? 4 : 3];
      { // update plaststrain0
        model_real_plain_vector tmpvec(gmm::vect_size(md.real_variable(plaststrain0)));
        const im_data *pimd = md.pim_data_of_variable(plaststrain0);
        if (pimd)
          ga_interpolation_im_data(md, plaststrain, *pimd, tmpvec, region);
        else {
          const mesh_fem *pmf = md.pmesh_fem_of_variable(plaststrain0);
          GMM_ASSERT1(pmf, "Provided data " << plaststrain0 << " should be defined "
                           "either on a im_data or a mesh_fem object");
          //ga_interpolation_Lagrange_fem(md, plaststrain, *pmf, tmpvec, region);
          ga_local_projection(md, mim, plaststrain, *pmf, tmpvec, region);
        }
        gmm::copy(tmpvec, md.set_real_variable(plaststrain0));
      }

      { // update invCp0
        model_real_plain_vector tmpvec(gmm::vect_size(md.real_variable(invCp0)));
        const im_data *pimd = md.pim_data_of_variable(invCp0);
        if (pimd)
          ga_interpolation_im_data(md, invCp, *pimd, tmpvec, region);
        else {
          const mesh_fem *pmf = md.pmesh_fem_of_variable(invCp0);
          GMM_ASSERT1(pmf, "Provided data " << invCp0 << " should be defined "
                           "either on a im_data or a mesh_fem object");
          //ga_interpolation_Lagrange_fem(md, invCp, *pmf, tmpvec, region);
          ga_local_projection(md, mim, invCp, *pmf, tmpvec, region);
        }
        gmm::copy(tmpvec, md.set_real_variable(invCp0));
      }

      gmm::clear(md.set_real_variable(multname));

    } else
      GMM_ASSERT1(false, lawname << " is not a known elastoplastic law");
  }

  void compute_finite_strain_elastoplasticity_Von_Mises
  (model &md, const mesh_im &mim,
   std::string lawname, plasticity_unknowns_type unknowns_type,
   const std::vector<std::string> &varnames,
   const std::vector<std::string> &params,
   const mesh_fem &mf_vm, model_real_plain_vector &VM,
   size_type region) {

    filter_lawname(lawname);
    if (lawname.compare("simo_miehe") == 0 ||
        lawname.compare("eterovic_bathe") == 0) {
      std::string dummy1, dummy2, dummy3, von_mises;
      build_Simo_Miehe_elastoplasticity_expressions
        (md, unknowns_type, varnames, params, dummy1, dummy2, dummy3, von_mises);
      VM.resize(mf_vm.nb_dof());

      bool has_pressure_var(unknowns_type ==
                            DISPLACEMENT_AND_PLASTIC_MULTIPLIER_AND_PRESSURE);
      const std::string &plaststrain0 = varnames[has_pressure_var ? 3 : 2];
      const std::string &invCp0       = varnames[has_pressure_var ? 4 : 3];
      bool im_data1 = md.pim_data_of_variable(plaststrain0) != 0;
      bool im_data2 = md.pim_data_of_variable(invCp0) != 0;
      bool fem_data1 = md.pmesh_fem_of_variable(plaststrain0) != 0;
      bool fem_data2 = md.pmesh_fem_of_variable(invCp0) != 0;
      GMM_ASSERT1(im_data1 || fem_data1,
                  "Provided data " << plaststrain0 <<
                  " should be defined on a im_data or a mesh_fem object");
      GMM_ASSERT1(im_data2 || fem_data2,
                  "Provided data " << invCp0 <<
                  " should be defined on a im_data or a mesh_fem object");
      if (im_data1 || im_data2) {
        ga_local_projection(md, mim, von_mises, mf_vm, VM, region);
      } else {
        ga_interpolation_Lagrange_fem(md, von_mises, mf_vm, VM, region);
      }

    } else
      GMM_ASSERT1(false, lawname << " is not a known elastoplastic law");

  }

















  //=================================================================
  //
  //  Old version of an elastoplasticity Brick for isotropic perfect
  //  plasticity with the low level generic assembly.
  //  Particularity of this brick: the flow rule is integrated on
  //  finite element nodes (not on Gauss points).
  //
  //=================================================================

  enum elastoplasticity_nonlinear_term_version { PROJ,
                                                 GRADPROJ,
                                                 PLAST
  };

  /** Compute the projection of D*e + sigma_bar_
      on the dof of sigma. */
  class elastoplasticity_nonlinear_term : public nonlinear_elem_term {

  protected:
    const mesh_im &mim;
    const mesh_fem &mf_u;
    const mesh_fem &mf_sigma;
    const mesh_fem *pmf_data;
    model_real_plain_vector U_n, U_np1;
    model_real_plain_vector Sigma_n;
    model_real_plain_vector threshold, lambda, mu;
    const abstract_constraints_projection &t_proj;
    const size_type option;
    const size_type flag_proj;
    const bool store_sigma;

    bgeot::multi_index sizes_;

    size_type N, size_proj;

    // temporary variables
    base_vector params;
    size_type current_cv;
    model_real_plain_vector convex_coeffs, interpolated_val;

    // storage variables
    model_real_plain_vector cumulated_sigma; // either the projected stress (option==PROJ)
                                             // or the plastic stress (option==PLAST)
    model_real_plain_vector cumulated_count;

    fem_precomp_pool fppool;


    // computes stresses or stress projections on all sigma dofs of a convex
    void compute_convex_coeffs(size_type cv) {

      current_cv = cv;

      pfem pf_sigma = mf_sigma.fem_of_element(cv);
      size_type nbd_sigma = pf_sigma->nb_dof(cv);
      size_type qdim_sigma = mf_sigma.get_qdim();

      gmm::resize(convex_coeffs, size_proj*nbd_sigma);

      base_matrix G;
      bgeot::vectors_to_base_matrix
        (G, mf_u.linked_mesh().points_of_convex(cv));
      bgeot::pgeometric_trans pgt =
        mf_u.linked_mesh().trans_of_convex(cv);

      // if the Lame coefficient are vector fields
      base_vector coeff_data;
      pfem pf_data;
      fem_interpolation_context ctx_data;
      if (pmf_data) {
        pf_data = pmf_data->fem_of_element(cv);
        size_type nbd_data = pf_data->nb_dof(cv);
        coeff_data.resize(nbd_data*3);

        // Definition of the Lame coeff
        mesh_fem::ind_dof_ct::const_iterator itdof
          = pmf_data->ind_basic_dof_of_element(cv).begin();
        for (size_type k = 0; k < nbd_data; ++k, ++itdof) {
          coeff_data[k*3] = lambda[*itdof];
          coeff_data[k*3+1] = mu[*itdof];
          coeff_data[k*3+2] = threshold[*itdof];
        }
        GMM_ASSERT1(pf_data->target_dim() == 1,
                    "won't interpolate on a vector FEM... ");

        pfem_precomp pfp_data = fppool(pf_data, pf_sigma->node_tab(cv));
        ctx_data = fem_interpolation_context
          (pgt, pfp_data, size_type(-1), G, cv, short_type(-1));
      }

      // Definition of the coeff for du = u_n-u_np1 and optionally for u_np1
      size_type cvnbdof_u = mf_u.nb_basic_dof_of_element(cv);
      model_real_plain_vector coeff_du(cvnbdof_u);
      model_real_plain_vector coeff_u_np1(cvnbdof_u);
      mesh_fem::ind_dof_ct::const_iterator itdof
        = mf_u.ind_basic_dof_of_element(cv).begin();
      for (size_type k = 0; k < cvnbdof_u; ++k, ++itdof) {
        coeff_du[k] = U_np1[*itdof] - U_n[*itdof];
        coeff_u_np1[k] = U_np1[*itdof];
      }

      pfem pf_u = mf_u.fem_of_element(cv);
      pfem_precomp pfp_u = fppool(pf_u, pf_sigma->node_tab(cv));
      fem_interpolation_context
        ctx_u(pgt, pfp_u, size_type(-1), G, cv, short_type(-1));

      size_type qdim = mf_u.get_qdim();
      base_matrix G_du(qdim, qdim), G_u_np1(qdim, qdim); // G_du = G_u_np1 - G_u_n

      for (size_type ii = 0; ii < nbd_sigma; ++ii) {

        if (pmf_data) {
          // interpolation of the data on sigma dof
          ctx_data.set_ii(ii);
          pf_data->interpolation(ctx_data, coeff_data, params, 3);
        }

        // interpolation of the gradient of du and u_np1 on sigma dof
        ctx_u.set_ii(ii);
        pf_u->interpolation_grad(ctx_u, coeff_du, G_du, dim_type(qdim));
        if (option == PLAST)
          pf_u->interpolation_grad(ctx_u, coeff_u_np1, G_u_np1, dim_type(qdim));

        // Compute lambda*(tr(eps_np1)-tr(eps_n)) and lambda*tr(eps_np1)
        scalar_type ltrace_deps = params[0]*gmm::mat_trace(G_du);
        scalar_type ltrace_eps_np1 = (option == PLAST) ?
                                     params[0]*gmm::mat_trace(G_u_np1) : 0.;

        // Compute sigma_hat = D*(eps_np1 - eps_n) + sigma_n
        // where D represents the elastic stiffness tensor
        base_matrix sigma_hat(qdim, qdim);
        size_type sigma_dof = mf_sigma.ind_basic_dof_of_element(cv)[ii*qdim_sigma];
        for (dim_type j = 0; j < qdim; ++j) {
          for (dim_type i = 0; i < qdim; ++i)
            sigma_hat(i,j) = Sigma_n[sigma_dof++]
                             + params[1]*(G_du(i,j) + G_du(j,i));
          sigma_hat(j,j) += ltrace_deps;
        }

        // Compute the projection or its grad
        base_matrix proj;
        t_proj.do_projection(sigma_hat, params[2], proj, flag_proj);

        // Compute the plastic part if required
        if (option == PLAST)
          for (dim_type i = 0; i < qdim; ++i) {
            for (dim_type j = 0; j < qdim; ++j)
              proj(i,j) -= params[1]*(G_u_np1(i,j) + G_u_np1(j,i));
            proj(i,i) -= ltrace_eps_np1;
          }

        // Fill in convex_coeffs with sigma or its grad
        std::copy(proj.begin(), proj.end(),
                  convex_coeffs.begin() + proj.size() * ii);

        // Store the projected or plastic sigma
        if (store_sigma) {
          sigma_dof = mf_sigma.ind_basic_dof_of_element(cv)[ii*qdim_sigma];
          for (dim_type j = 0; j < qdim; ++j) {
            for (dim_type i = 0; i < qdim; ++i) {
              cumulated_count[sigma_dof] += 1;
              cumulated_sigma[sigma_dof++] += proj(i,j);
            }
          }
        }

      } // ii = 0:nbd_sigma-1

    }

  public:

    // constructor
    elastoplasticity_nonlinear_term
      (const mesh_im &mim_,
       const mesh_fem &mf_u_,
       const mesh_fem &mf_sigma_,
       const mesh_fem *pmf_data_,
       const model_real_plain_vector &U_n_,
       const model_real_plain_vector &U_np1_,
       const model_real_plain_vector &Sigma_n_,
       const model_real_plain_vector &threshold_,
       const model_real_plain_vector &lambda_,
       const model_real_plain_vector &mu_,
       const abstract_constraints_projection  &t_proj_,
       size_type option_,
       bool store_sigma_) :
      mim(mim_), mf_u(mf_u_), mf_sigma(mf_sigma_), pmf_data(pmf_data_),
      Sigma_n(Sigma_n_), t_proj(t_proj_), option(option_),
      flag_proj(option == GRADPROJ ? 1 : 0),
      store_sigma(option == GRADPROJ ? false : store_sigma_) {

      params.resize(3);
      N = mf_u.linked_mesh().dim();

      sizes_ = (flag_proj == 0 ? bgeot::multi_index(N,N)
                               : bgeot::multi_index(N,N,N,N));

      // size_proj is different if we compute the projection
      // or the gradient of the projection
      size_proj = (flag_proj == 0 ? N*N : N*N*N*N);

      gmm::resize(U_n, mf_u.nb_basic_dof());
      gmm::resize(U_np1, mf_u.nb_basic_dof());
      gmm::resize(Sigma_n, mf_sigma.nb_basic_dof());
      mf_u.extend_vector(gmm::sub_vector(U_n_,
                                         gmm::sub_interval(0,mf_u.nb_dof())),
                         U_n);
      mf_u.extend_vector(gmm::sub_vector(U_np1_,
                                         gmm::sub_interval(0,mf_u.nb_dof())),
                         U_np1);
      mf_sigma.extend_vector(gmm::sub_vector(Sigma_n_,
                                             gmm::sub_interval(0,mf_sigma.nb_dof())),
                             Sigma_n);

      if (pmf_data) {
        gmm::resize(mu, pmf_data->nb_basic_dof());
        gmm::resize(lambda, pmf_data->nb_basic_dof());
        gmm::resize(threshold, pmf_data->nb_basic_dof());
        pmf_data->extend_vector(threshold_, threshold);
        pmf_data->extend_vector(lambda_, lambda);
        pmf_data->extend_vector(mu_, mu);
      } else {
        gmm::resize(mu, 1); mu[0]  =  mu_[0];
        gmm::resize(lambda, 1); lambda[0]  =  lambda_[0];
        gmm::resize(threshold, 1); threshold[0] =  threshold_[0];
        params[0] = lambda[0];
        params[1] = mu[0];
        params[2] = threshold[0];
      }
      GMM_ASSERT1(mf_u.get_qdim() == N,
                  "wrong qdim for the mesh_fem");

      gmm::resize(interpolated_val, size_proj);

      if (store_sigma) {
        cumulated_sigma.resize(mf_sigma.nb_dof());
        cumulated_count.resize(mf_sigma.nb_dof());
      }

      // used to know if the current element is different
      // than the previous one and so if a new computation
      // is necessary or not.
      current_cv = size_type(-1);

    }


    const bgeot::multi_index &sizes(size_type) const { return sizes_; }


    // method from nonlinear_elem_term, gives on output the tensor
    virtual void compute(fem_interpolation_context& ctx,
                         bgeot::base_tensor &t) {
      size_type cv = ctx.convex_num(); //index of current element
      pfem pf_sigma = ctx.pf();
      GMM_ASSERT1(pf_sigma->is_lagrange(),
                  "Sorry, works only for Lagrange fems");

      // if the current element is different than the previous one
      if (cv != current_cv)
        compute_convex_coeffs(cv);

      // interpolation of the sigma or its grad on sigma dof
      pf_sigma->interpolation(ctx, convex_coeffs, interpolated_val, dim_type(size_proj));

      // copy the result into the returned tensor t
      t.adjust_sizes(sizes_);
      std::copy(interpolated_val.begin(), interpolated_val.end(), t.begin());
    }


    // method to get the averaged sigma stored during the assembly
    void get_averaged_sigmas(model_real_plain_vector &sigma) {
       model_real_plain_vector glob_cumulated_count(mf_sigma.nb_dof());
       MPI_SUM_VECTOR(cumulated_sigma, sigma);
       MPI_SUM_VECTOR(cumulated_count, glob_cumulated_count);
       size_type imax = mf_sigma.nb_dof();
       for (size_type i = 0; i < imax; ++i)
         sigma[i] /= glob_cumulated_count[i];
    }

};



  /**
     Right hand side vector for elastoplasticity
      @ingroup asm
  */
  void asm_elastoplasticity_rhs
    (model_real_plain_vector &V,
     model_real_plain_vector *saved_sigma,
     const mesh_im &mim,
     const mesh_fem &mf_u,
     const mesh_fem &mf_sigma,
     const mesh_fem &mf_data,
     const model_real_plain_vector &u_n,
     const model_real_plain_vector &u_np1,
     const model_real_plain_vector &sigma_n,
     const model_real_plain_vector &lambda,
     const model_real_plain_vector &mu,
     const model_real_plain_vector &threshold,
     const abstract_constraints_projection  &t_proj,
     size_type option_sigma,
     const mesh_region &rg = mesh_region::all_convexes()) {

    GMM_ASSERT1(mf_u.get_qdim() == mf_u.linked_mesh().dim(),
                "wrong qdim for the mesh_fem");
    GMM_ASSERT1(option_sigma == PROJ || option_sigma == PLAST,
                "wrong option parameter");

    elastoplasticity_nonlinear_term plast(mim, mf_u, mf_sigma, &mf_data,
                                          u_n, u_np1, sigma_n,
                                          threshold, lambda, mu,
                                          t_proj, option_sigma, (saved_sigma != NULL));

    generic_assembly assem("V(#1) + =comp(NonLin(#2).vGrad(#1))(i,j,:,i,j);");

    assem.push_mi(mim);
    assem.push_mf(mf_u);
    assem.push_mf(mf_sigma);
    assem.push_nonlinear_term(&plast);
    assem.push_vec(V);
    assem.assembly(rg);

    if (saved_sigma)
      plast.get_averaged_sigmas(*saved_sigma);
  }


  /**
      Tangent matrix for elastoplasticity
      @ingroup asm
  */
  void asm_elastoplasticity_tangent_matrix
    (model_real_sparse_matrix &H,
     const mesh_im &mim,
     const mesh_fem &mf_u,
     const mesh_fem &mf_sigma,
     const mesh_fem *pmf_data,
     const model_real_plain_vector &u_n,
     const model_real_plain_vector &u_np1,
     const model_real_plain_vector &sigma_n,
     const model_real_plain_vector &lambda,
     const model_real_plain_vector &mu,
     const model_real_plain_vector &threshold,
     const abstract_constraints_projection &t_proj,
     const mesh_region &rg = mesh_region::all_convexes()) {

    GMM_ASSERT1(mf_u.get_qdim() == mf_u.linked_mesh().dim(),
                "wrong qdim for the mesh_fem");

    elastoplasticity_nonlinear_term gradplast(mim, mf_u, mf_sigma, pmf_data,
                                              u_n, u_np1, sigma_n,
                                              threshold, lambda, mu,
                                              t_proj, GRADPROJ, false);

    generic_assembly assem;

    if (pmf_data)
      assem.set("lambda=data$1(#3); mu=data$2(#3);"
                "t=comp(NonLin(#2).vGrad(#1).vGrad(#1).Base(#3))(i,j,:,:,:,:,:,:,i,j,:);"
                "M(#1,#1)+=  sym(t(k,l,:,l,k,:,m).mu(m)+t(k,l,:,k,l,:,m).mu(m)+t(k,k,:,l,l,:,m).lambda(m))");
    else
      assem.set("lambda=data$1(1); mu=data$2(1);"
                "t=comp(NonLin(#2).vGrad(#1).vGrad(#1))(i,j,:,:,:,:,:,:,i,j);"
                "M(#1,#1)+= sym(t(k,l,:,l,k,:).mu(1)+t(k,l,:,k,l,:).mu(1)+t(k,k,:,l,l,:).lambda(1))");

    assem.push_mi(mim);
    assem.push_mf(mf_u);
    assem.push_mf(mf_sigma);
    if (pmf_data)
      assem.push_mf(*pmf_data);
    assem.push_data(lambda);
    assem.push_data(mu);
    assem.push_nonlinear_term(&gradplast);
    assem.push_mat(H);
    assem.assembly(rg);

  }



  //=================================================================
  //  Elastoplasticity Brick
  //=================================================================

  struct elastoplasticity_brick : public virtual_brick {

    pconstraints_projection  t_proj;

    virtual void asm_real_tangent_terms(const model &md,
                                        size_type /* ib */,
                                        const model::varnamelist &vl,
                                        const model::varnamelist &dl,
                                        const model::mimlist &mims,
                                        model::real_matlist &matl,
                                        model::real_veclist &vecl,
                                        model::real_veclist &,
                                        size_type region,
                                        build_version version)const {

      GMM_ASSERT1(mims.size() == 1,
                  "Elastoplasticity brick need a single mesh_im");
      GMM_ASSERT1(vl.size() == 1,
                  "Elastoplasticity brick need one variable");
      /** vl[0] = u */

      GMM_ASSERT1(dl.size() == 5,
                  "Wrong number of data for elastoplasticity brick, "
                  << dl.size() << " should be 4.");
      GMM_ASSERT1(matl.size() == 1,  "Wrong number of terms for "
                  "elastoplasticity brick");

      const model_real_plain_vector &u_np1 = md.real_variable(vl[0]);
      const model_real_plain_vector &u_n = md.real_variable(dl[4]);
      const mesh_fem &mf_u = *(md.pmesh_fem_of_variable(vl[0]));
      GMM_ASSERT1(&mf_u == md.pmesh_fem_of_variable(dl[4]),
                  "The previous displacement data have to be defined on the "
                  "same mesh_fem as the displacement variable");

      const model_real_plain_vector &lambda = md.real_variable(dl[0]);
      const model_real_plain_vector &mu = md.real_variable(dl[1]);
      const model_real_plain_vector &threshold = md.real_variable(dl[2]);
      const mesh_fem *mf_data = md.pmesh_fem_of_variable(dl[0]);

      const model_real_plain_vector &sigma_n = md.real_variable(dl[3]);
      const mesh_fem &mf_sigma = *(md.pmesh_fem_of_variable(dl[3]));
      GMM_ASSERT1(!(mf_sigma.is_reduced()),
                  "Works only for pure Lagrange fems");

      const mesh_im &mim = *mims[0];
      mesh_region rg(region);
      mim.linked_mesh().intersect_with_mpi_region(rg);

      if (version & model::BUILD_MATRIX) {
        gmm::clear(matl[0]);
        asm_elastoplasticity_tangent_matrix
          (matl[0], mim, mf_u, mf_sigma, mf_data, u_n,
           u_np1, sigma_n, lambda, mu, threshold, *t_proj, rg);
      }

      if (version & model::BUILD_RHS) {
        model_real_plain_vector *dummy = 0;
        asm_elastoplasticity_rhs(vecl[0], dummy,
                                 mim, mf_u, mf_sigma, *mf_data,
                                 u_n, u_np1, sigma_n,
                                 lambda, mu, threshold, *t_proj, PROJ, rg);
        gmm::scale(vecl[0], scalar_type(-1));
      }

    }

    // constructor
    elastoplasticity_brick(const pconstraints_projection &t_proj_)
      : t_proj(t_proj_) {
      set_flags("Elastoplasticity brick", false /* is linear*/,
                true /* is symmetric */, false /* is coercive */,
                true /* is real */, false /* is complex */);
    }

  };


  //=================================================================
  //  Add a elastoplasticity brick
  //=================================================================

  size_type add_elastoplasticity_brick
    (model &md,
     const mesh_im &mim,
     const pconstraints_projection &ACP,
     const std::string &varname,
     const std::string &data_previous_disp,
     const std::string &datalambda,
     const std::string &datamu,
     const std::string &datathreshold,
     const std::string &datasigma,
     size_type region) {

    pbrick pbr = std::make_shared<elastoplasticity_brick>(ACP);

    model::termlist tl{model::term_description(varname, varname, true)};
    model::varnamelist
      dl{datalambda, datamu, datathreshold, datasigma, data_previous_disp},
      vl{varname};

    return md.add_brick(pbr, vl, dl, tl,
                        model::mimlist(1,&mim), region);
  }


  //=================================================================
  //  New stress constraints values computation and saved
  //=================================================================
  //  Update of u and sigma on time iterates :
  //  u_np1 -> u_n         sigma_np1 -> sigma_n

  void elastoplasticity_next_iter(model &md,
                                  const mesh_im &mim,
                                  const std::string &varname,
                                  const std::string &data_previous_disp,
                                  const pconstraints_projection &ACP,
                                  const std::string &datalambda,
                                  const std::string &datamu,
                                  const std::string &datathreshold,
                                  const std::string &datasigma) {

    const model_real_plain_vector &u_np1 = md.real_variable(varname);
    model_real_plain_vector &u_n = md.set_real_variable(data_previous_disp);
    const mesh_fem &mf_u = *(md.pmesh_fem_of_variable(varname));

    const model_real_plain_vector &lambda = md.real_variable(datalambda);
    const model_real_plain_vector &mu = md.real_variable(datamu);
    const model_real_plain_vector &threshold = md.real_variable(datathreshold);
    const mesh_fem *mf_data = md.pmesh_fem_of_variable(datalambda);

    const model_real_plain_vector &sigma_n = md.real_variable(datasigma);
    const mesh_fem &mf_sigma = *(md.pmesh_fem_of_variable(datasigma));

    // dim_type N = mf_sigma.linked_mesh().dim();

    mesh_region rg = mim.linked_mesh().get_mpi_region();

    model_real_plain_vector sigma_np1(mf_sigma.nb_dof());
    model_real_plain_vector dummyV(mf_u.nb_dof());
    asm_elastoplasticity_rhs(dummyV, &sigma_np1,
                             mim, mf_u, mf_sigma, *mf_data,
                             u_n, u_np1, sigma_n,
                             lambda, mu, threshold, *ACP, PROJ, rg);

    // upload sigma and u : u_np1 -> u_n, sigma_np1 -> sigma_n
    // be careful to use this function
    // only if the computation is over
    gmm::copy(sigma_np1, md.set_real_variable(datasigma));
    gmm::copy(u_np1, u_n);

  }



  //=================================================================
  //  Von Mises or Tresca stress computation for elastoplasticity
  //=================================================================

  void compute_elastoplasticity_Von_Mises_or_Tresca
    (model &md,
     const std::string &datasigma,
     const mesh_fem &mf_vm,
     model_real_plain_vector &VM,
     bool tresca) {

    GMM_ASSERT1(gmm::vect_size(VM) == mf_vm.nb_dof(),
                "The vector has not the right size");

    const model_real_plain_vector &sigma_np1 = md.real_variable(datasigma);
    const mesh_fem &mf_sigma = md.mesh_fem_of_variable(datasigma);

    // dimension of the finite element used
    dim_type N = mf_sigma.linked_mesh().dim();

    GMM_ASSERT1(mf_vm.get_qdim() == 1,
                "Target dimension of mf_vm should be 1");

    base_matrix sigma(N, N), Id(N, N);
    base_vector eig(N);
    base_vector sigma_vm(mf_vm.nb_dof()*N*N);

    gmm::copy(gmm::identity_matrix(), Id);

    interpolation(mf_sigma, mf_vm, sigma_np1, sigma_vm);

    // for each dof we compute the Von Mises or Tresca stress
    for (size_type ii = 0; ii < mf_vm.nb_dof(); ++ii) {

      /* we retrieve the matrix sigma_vm on this dof */
      std::copy(sigma_vm.begin()+ii*N*N, sigma_vm.begin()+(ii+1)*N*N,
                sigma.begin());

      if (!tresca) {
        /* von mises: norm(deviator(sigma)) */
        gmm::add(gmm::scaled(Id, -gmm::mat_trace(sigma) / N), sigma);

        /* von mises stress=sqrt(3/2)* norm(sigma) */
        VM[ii] = sqrt(3.0/2.)*gmm::mat_euclidean_norm(sigma);
      } else {
        /* else compute the tresca criterion */
        gmm::symmetric_qr_algorithm(sigma, eig);
        std::sort(eig.begin(), eig.end());
        VM[ii] = eig.back() - eig.front();
      }
    }
  }



  //=================================================================
  //  Compute the plastic part
  //=================================================================

  void compute_plastic_part(model &md,
                            const mesh_im &mim,
                            const mesh_fem &mf_pl,
                            const std::string &varname,
                            const std::string &data_previous_disp,
                            const pconstraints_projection &ACP,
                            const std::string &datalambda,
                            const std::string &datamu,
                            const std::string &datathreshold,
                            const std::string &datasigma,
                            model_real_plain_vector &plast) {

    const model_real_plain_vector &u_np1 = md.real_variable(varname);
    const model_real_plain_vector &u_n = md.real_variable(data_previous_disp);
    const mesh_fem &mf_u = *(md.pmesh_fem_of_variable(varname));

    const model_real_plain_vector &lambda = md.real_variable(datalambda);
    const model_real_plain_vector &mu = md.real_variable(datamu);
    const model_real_plain_vector &threshold = md.real_variable(datathreshold);
    const mesh_fem *pmf_data = md.pmesh_fem_of_variable(datalambda);

    const model_real_plain_vector &sigma_n = md.real_variable(datasigma);
    const mesh_fem &mf_sigma = *(md.pmesh_fem_of_variable(datasigma));

    dim_type N = mf_sigma.linked_mesh().dim();

    mesh_region rg = mim.linked_mesh().get_mpi_region();

    model_real_plain_vector dummyV(mf_u.nb_dof());
    model_real_plain_vector saved_plast(mf_sigma.nb_dof());
    asm_elastoplasticity_rhs(dummyV, &saved_plast,
                             mim, mf_u, mf_sigma, *pmf_data,
                             u_n, u_np1, sigma_n,
                             lambda, mu, threshold, *ACP, PLAST, rg);

    /* Retrieve and save the plastic part */
    GMM_ASSERT1(gmm::vect_size(plast) == mf_pl.nb_dof(),
                "The vector has not the right size");
    GMM_ASSERT1(mf_pl.get_qdim() == 1,
                "Target dimension of mf_pl should be 1");

    base_vector saved_pl(mf_pl.nb_dof()*N*N);
    interpolation(mf_sigma, mf_pl, saved_plast, saved_pl);

    // for each dof we compute the norm of the plastic part
    base_matrix plast_tmp(N, N);
    for (size_type ii = 0; ii < mf_pl.nb_dof(); ++ii) {

      /* we retrieve the matrix sigma_pl on this dof */
      std::copy(saved_pl.begin()+ii*N*N, saved_pl.begin()+(ii+1)*N*N,
                plast_tmp.begin());

      plast[ii] = gmm::mat_euclidean_norm(plast_tmp);

    }

  }





}  /* end of namespace getfem.  */
