// -*- c++ -*- (enables emacs c++ mode)
//===========================================================================
//
// Copyright (C) 2011-2011 Yves Renard, Tomas Ligursky.
//
// This file is a part of GETFEM++
//
// Getfem++  is  free software;  you  can  redistribute  it  and/or modify it
// under  the  terms  of the  GNU  Lesser General Public License as published
// by  the  Free Software Foundation;  either version 2.1 of the License,  or
// (at your option) any later version.
// This program  is  distributed  in  the  hope  that it will be useful,  but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or  FITNESS  FOR  A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.
// You  should  have received a copy of the GNU Lesser General Public License
// along  with  this program;  if not, write to the Free Software Foundation,
// Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
//
//===========================================================================

/**
   @file test_continuation.cc
   
   Continuation of solutions to the following state problem parameterized by
   lambda: u'' +  u = lambda * exp(u) in (0, 1), u'(0) = u'(1) = 0.

   This program is used to check that getfem++ is working. This is also 
   a good example of use of Getfem++.
*/

#include "getfem/getfem_regular_meshes.h"
#include "getfem/getfem_model_solvers.h"
#include "getfem/getfem_continuation.h" /* import continuation method */

/* some Getfem++ types that we will be using */
using bgeot::scalar_type; /* = double */
using bgeot::size_type;   /* = unsigned long */

/* definition of a vector type; this one is built using the predefined types
   in Gmm++ */
typedef getfem::modeling_standard_plain_vector plain_vector;

/*
  structure for the state problem
*/
struct state_problem {

  getfem::mesh mesh;         /* the mesh */
  getfem::mesh_fem mf_u;     /* the FEM-method */
  getfem::mesh_im mim;       /* the integration method */
  scalar_type lambda;

  std::string datafilename;
  bgeot::md_param PARAM;

  bool cont(plain_vector &U);
  void init(void);
  state_problem(void) : mf_u(mesh), mim(mesh) {}
};


/* Read parameters from the .param file, build the mesh, set finite element
 * and integration methods.
 */
void state_problem::init(void) {
  std::string FEM_TYPE  = PARAM.string_value("FEM_TYPE","FEM name");
  std::string INTEGRATION = PARAM.string_value("INTEGRATION",
					       "Name of integration method");
  cout << "FEM_TYPE="  << FEM_TYPE << "\n";
  cout << "INTEGRATION=" << INTEGRATION << "\n";
  
  /* First step: build the mesh */
  std::vector<getfem::size_type> nsubdiv(1);
  nsubdiv[0] = PARAM.int_value("NX", "Number of the space steps ");
  regular_unit_mesh(mesh, nsubdiv, bgeot::parallelepiped_geotrans(1, 1));
  
  datafilename = PARAM.string_value("ROOTFILENAME",
				    "Base name of data files.");

  /* set the finite element on the mf_u */
  getfem::pfem pf_u = getfem::fem_descriptor(FEM_TYPE);
  getfem::pintegration_method ppi = 
    getfem::int_method_descriptor(INTEGRATION);

  mf_u.set_finite_element(pf_u);
  mim.set_integration_method(ppi);

}

/**************************************************************************/
/*  Model.                                                                */
/**************************************************************************/

bool state_problem::cont(plain_vector &U) {
  
  getfem::model model;

  // Main unknown of the problem
  model.add_fem_variable("u", mf_u);

  // The Helmholtz term on u.
  plain_vector K(1); K[0] = 1.;
  model.add_initialized_fixed_size_data("k", K);
  add_Helmholtz_brick(model, mim, "u", "k");

  // The term lambda * exp(u).
  std::string f = "-lambda*exp(u)", dfdu = "-lambda*exp(u)";
  lambda = PARAM.real_value("LAMBDA0");
  model.add_initialized_scalar_data("lambda", lambda);
  add_basic_nonlinear_brick(model, mim, "u", f, dfdu,
			    size_type(-1), "lambda");

  // Initialize the continuation
  getfem::rmodel_plsolver_type ls =
    getfem::default_linear_solver<getfem::model_real_sparse_matrix,
                                  getfem::model_real_plain_vector>(model);
  size_type nb_step = int(PARAM.int_value("NBSTEP")),
    maxit = PARAM.int_value("MAXITER"),
    thrit = PARAM.int_value("THR_ITER"),
    nb_dof = mf_u.nb_dof() + 1;
  scalar_type maxres = PARAM.real_value("RESIDUAL"),
    maxdiff = PARAM.real_value("DIFFERENCE"),
    minang = PARAM.real_value("ANGLE"),
    h_init = PARAM.real_value("H_INIT") * nb_dof,
    h_max = PARAM.real_value("H_MAX") * nb_dof,
    h_min = PARAM.real_value("H_MIN") * nb_dof,
    h_inc = PARAM.real_value("H_INC"),
    h_dec = PARAM.real_value("H_DEC"),
    eps = PARAM.real_value("EPSILON");
  int noisy = PARAM.int_value("NOISY");
  getfem::S_getfem_model s(model, "lambda", ls, maxit, thrit, maxres,
			   maxdiff, minang, h_init, h_max, h_min, h_inc,
			   h_dec, eps, noisy);

  if (noisy > 0) cout << "computing initial point" << endl;
  gmm::iteration iter(PARAM.real_value("RESIDUAL_INIT"), noisy, 40000);
  getfem::standard_solve(model, iter);

  gmm::resize(U, mf_u.nb_dof());
  gmm::copy(model.real_variable("u"), U);

  cout << "U = " << U << endl;
  cout << "lambda - u * exp(-u) = " << lambda - U[0] * exp(-U[0]) << endl;

  plain_vector T_U(U);
  scalar_type T_lambda = PARAM.real_value("DIRECTION"), h;
  init_continuation(s, U, lambda, T_U, T_lambda, h);

  // Continuation
  bool converged = true;
  for (size_type step = 0; step < nb_step; ++step) {
    cout << endl << "beginning of step " << step + 1 << endl;
    
    converged = Moore_Penrose_continuation(s, U, lambda, T_U, T_lambda, h);
    
    cout << "U = " << U << endl;
    cout << "lambda = " << lambda << endl;
    cout << "lambda - U[0] * exp(-U[0]) = "
	 << lambda - U[0] * exp(-U[0]) << endl;

    cout << "end of Step nº " << step+1 << " / " << nb_step << endl;
  }

  return (converged);
}
  
/**************************************************************************/
/*  main program.                                                         */
/**************************************************************************/

int main(int argc, char *argv[]) {

  GMM_SET_EXCEPTION_DEBUG; // Exceptions make a memory fault, to debug.
  FE_ENABLE_EXCEPT;        // Enable floating point exception for Nan.

  try {    
    state_problem p;
    p.PARAM.read_command_line(argc, argv);
    p.init();
    plain_vector U(p.mf_u.nb_dof());
    if (!p.cont(U)) GMM_ASSERT1(false, "Solve has failed");
  }
  GMM_STANDARD_CATCH_ERROR;

  return 0; 
}