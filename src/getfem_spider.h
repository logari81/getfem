/* -*- c++ -*- (enables emacs c++ mode)                                    */
/* *********************************************************************** */
/*                                                                         */
/* Library : GEneric Tool for Finite Element Methods (getfem)              */
/* File    : getfem_interpolated_fem.h : definition of a finite element    */
/*           method which interpolates a fem on a different mesh.          */
/*                                                                         */
/* Date : October 29, 2004.                                                */
/* Author : Yves Renard, Yves.Renard@gmm.insa-tlse.fr                      */
/*                                                                         */
/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2004  Yves Renard.                                        */
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



#ifndef GETFEM_SPIDER_H__
#define GETFEM_SPIDER_H__

#include <getfem_interpolated_fem.h>
#include <getfem_regular_mesh.h>

namespace getfem {




  struct Xfem_sqrtr : public virtual_Xfem_func {
    virtual scalar_type val(const Xfem_func_context &c) { ::sqrt(c.xreal[0]); }
    virtual base_small_vector grad(const Xfem_func_context &c)
    { base_small_vector V(2); V[0] = 1. / (2.* ::sqrt(c.xreal[0]));}
    virtual base_matrix hess(const Xfem_func_context &c)
    { base_matrix m(2,2); m(0,0) = -1. / (4.* ::sqrt(c.xreal[0])*c.xreal[0]) }
  };

  struct interpolated_transformation : public virtual_interpolated_func {
    base_small_vector trans;
    scalar_type theta0;
     
    virtual void val(const interpolated_func_context&c, base_small_vector &v) {
      base_small_vector w =  c.xreal - trans;
      v[0] = gmm::vect_norm2(w);
      v[1] = atan2(w[0], w[1]) - theta0;
    }
    virtual void grad(const interpolated_func_context&, base_matrix &m) {
      base_small_vector w =  c.xreal - trans;
      scalar_type r = gmm::vect_norm2(w);
      m(0,0) = w[0] / r; m(0,1) = w[1] / r;
      m(1,0) = -c.real[1] / gmm::sqr(r);
      m(1,0) = c.real[0] / gmm::sqr(r);
    }
    virtual void hess(const interpolated_func_context&, base_matrix &)
    { DAL_THROW(dal::failure_error,"this interpolated_func has no hessian"); }
    
    
    virtual ~virtual_interpolated_func() {}
  };


  class spider {

    protected :

      mesh cartesian;
      mesh_fem cartesian_fem;
      pfem Qk;
      Xfem enriched_Qk;
      scalar_type R;
      unsigned Nr, Ntheta, K;
      Xfem_sqrtr Sqrtr;
      interpolated_fem *final_fem;
      interpolated_transformation it;

    public :
      
      ~spider() { if (final_fem) delete final_fem; }
      
      spider(scalar_type R_, mesh_fem &target_fem, unsigned Nr_, unsigned Ntheta_, unsigned K_,
	     base_small_vector translation, scalar_type theta0)
        : R(R_), Nr(Nr_), Ntheta(Ntheta_), K(K_), final_fem(0) {
        
	it.trans = translation;
	it.theta0 = theta0;

        /* make the cartesian mesh */
        bgeot::pgeometric_trans pgt = 
	  bgeot::geometric_trans_descriptor("GT_LINEAR_QK(2)");
        std::vector<size_type> nsubdiv(2);
	nsubdiv[0] = Nr; nsubdiv[1] = Ntheta;
        getfem::regular_unit_mesh(cartesian, nsubdiv, pgt, false);
	bgeot::base_matrix M(2,2);
	M(0,0) = R;
	M(1,1) = 2. * M_PI;
	cartesian.transformation(M);
	bgeot::base_small_vector V(2);
	V[1] = -M_PI;
	cartesian.translation(V);
	

	std::stringstream Qkname;
	Qkname << "FEM_QK(2," << K << ")";
	Qk = fem_descriptor(Qkname.str());
	enriched_Qk.add_func(Qk, Sqrtr);
	enriched_Qk.valid();

	
	std::stringstream ppiname;
	cartesian_fem.set_finite_element(cartesian.convex_index(), enriched_Qk,
					 getfem::int_method_descriptor("IM_GAUSS_PARALLELEPIPED(2,20)"));  
      
	final_fem = new interpolated_fem(cartesian_fem, target_fem, &it);
      }






  };



}  /* end of namespace getfem.                                            */

#endif
