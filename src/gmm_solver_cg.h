/* -*- c++ -*- (enables emacs c++ mode)                                    */
//=======================================================================
// Copyright (C) 1997-2001
// Authors: Andrew Lumsdaine <lums@osl.iu.edu> 
//          Lie-Quan Lee     <llee@osl.iu.edu>
//
// This file is part of the Iterative Template Library
//
// You should have received a copy of the License Agreement for the
// Iterative Template Library along with the software;  see the
// file LICENSE.  
//
// Permission to modify the code and to distribute modified code is
// granted, provided the text of this NOTICE is retained, a notice that
// the code was modified is included with the above COPYRIGHT NOTICE and
// with the COPYRIGHT NOTICE in the LICENSE file, and that the LICENSE
// file is distributed with the modified code.
//
// LICENSOR MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.
// By way of example, but not limitation, Licensor MAKES NO
// REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED SOFTWARE COMPONENTS
// OR DOCUMENTATION WILL NOT INFRINGE ANY PATENTS, COPYRIGHTS, TRADEMARKS
// OR OTHER RIGHTS.
//=======================================================================
/* *********************************************************************** */
/*                                                                         */
/* Library :  Generic Matrix Methods  (gmm)                                */
/* File    :  gmm_solver_cg.h : conjugated gradient.                       */
/*            Modified version of I.T.L. conjugated gradient.              */
/*     									   */
/* Date : October 13, 2002.                                                */
/* Author : Yves Renard, Yves.Renard@gmm.insa-tlse.fr                      */
/*                                                                         */
/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2002  Yves Renard.                                        */
/*                                                                         */
/* This file is a part of GETFEM++                                         */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify    */
/* it under the terms of the GNU Lesser General Public License as          */
/* published by the Free Software Foundation; version 2.1 of the License.  */
/*                                                                         */
/* This program is distributed in the hope that it will be useful,         */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/* GNU Lesser General Public License for more details.                     */
/*                                                                         */
/* You should have received a copy of the GNU Lesser General Public        */
/* License along with this program; if not, write to the Free Software     */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,  */
/* USA.                                                                    */
/*                                                                         */
/* *********************************************************************** */


#ifndef __GMM_SOLVER_CG_H
#define __GMM_SOLVER_CG_H

namespace gmm {

  /* ******************************************************************** */
  /*		conjugate gradient                           		  */
  /* (preconditionned, with parametrable scalar product)        	  */
  /* ******************************************************************** */

  template <class Matrix, class Matps, class Precond, 
            class Vector1, class Vector2>
  int cg(const Matrix& A, Vector1& x, const Vector2& b, const Matps& PS,
	 const Precond &P, int itemax, double residu, int noisy) {

    typedef typename temporary_plain_vector<Vector1>::vector_type temp_vector;
    typedef typename linalg_traits<Vector1>::value_type value_type;

    value_type rho(0), rho_1(0), a(0), beta(0), 
      norm_b = sqrt(vect_sp(PS, b, b));
    temp_vector p(vect_size(x)), q(vect_size(x)), r(vect_size(x)),
      z(vect_size(x));
    int iter = 0;

    if (norm_b == value_type(0))
      clear(x);
    else {
      mult(A, scaled(x, -1.0), b, r);
      mult(P, r, z);
      rho = vect_sp(PS, r, z);
      
      while (sqrt(dal::abs(rho)) > residu * norm_b) {
	
	if (iter == 0) copy(r, p);		  
	else { beta = rho / rho_1; add(r, scaled(p, beta), p); }
	
	mult(A, p, q);
	a = rho / vect_sp(PS, p, q);
	
	add(scaled(p, a), x);
	add(scaled(q, -a), r);
	
	mult(P, r, z);
	rho_1 = rho;
	rho = vect_sp(PS, r, z);
	
	if (++iter >= itemax) return -1;
	if (noisy > 0)  cout << "iter " << iter << " residu "
			     << sqrt(dal::abs(rho)) / norm_b << endl;
      }
    }
    return iter;
  }

  template <class Matrix, class Matps, class Precond, 
            class Vector1, class Vector2> inline 
  int cg(const Matrix& A, const Vector1& x, const Vector2& b, const Matps& PS,
	 const Precond &P, int itemax, double residu, int noisy)
  { return cg(A, linalg_const_cast(x), b, PS, P, itemax, residu, noisy); }

  template <class Matrix, class Precond, 
            class Vector1, class Vector2> inline
  int cg(const Matrix& A, Vector1& x, const Vector2& b,
	 const Precond &P, int itemax, double residu, int noisy)
  { return cg(A, x , b, identity_matrix(), P, itemax, residu, noisy); }

  template <class Matrix, class Precond, 
            class Vector1, class Vector2> inline
  int cg(const Matrix& A, const Vector1& x, const Vector2& b,
	 const Precond &P, int itemax, double residu, int noisy)
  { return cg(A, x , b , identity_matrix(), P , itemax, residu, noisy); }
  
}


#endif //  __GMM_SOLVER_CG_H
