/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2002-2004 Yves Renard, Julien Pommier.                    */
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

/**
 * Laplacian (Poisson) problem.
 *
 * This program is used to check that getfem++ is working. This is also 
 * a good example of use of Getfem++.
*/

#include <getfem_assembling.h> /* import assembly methods (and comp. of norms) */
#include <getfem_export.h>   /* export functions (save the solution in a file) */
#include <getfem_regular_meshes.h>
#include <gmm.h>


/* try to enable the SIGFPE if something evaluates to a Not-a-number
 * of infinity during computations
 */
#ifdef GETFEM_HAVE_FEENABLEEXCEPT
#  include <fenv.h>
#endif

/* some Getfem++ types that we will be using */
using bgeot::base_small_vector;  /* special class for small (dim < 16) vectors */
using bgeot::base_node;   /* geometrical nodes (derived from base_small_vector)*/
using bgeot::scalar_type; /* = double */
using bgeot::size_type;   /* = unsigned long */

/* definition of some matrix/vector types. These ones are built
 * using the predefined types in Gmm++
 */
typedef gmm::rsvector<scalar_type> sparse_vector_type;
typedef gmm::row_matrix<sparse_vector_type> sparse_matrix_type;
typedef gmm::col_matrix<sparse_vector_type> col_sparse_matrix_type;

/* Definitions for the exact solution of the Laplacian problem,
 *  i.e. Delta(sol_u) + sol_f = 0
 */

base_small_vector sol_K; /* a coefficient */
/* exact solution */
scalar_type sol_u(const base_node &x) { return sin(gmm::vect_sp(sol_K, x)); }
/* righ hand side */
scalar_type sol_f(const base_node &x)
{ return gmm::vect_sp(sol_K, sol_K) * sin(gmm::vect_sp(sol_K, x)); }
/* gradient of the exact solution */
base_small_vector sol_grad(const base_node &x)
{ return sol_K * cos(gmm::vect_sp(sol_K, x)); }

/*
  structure for the Laplacian problem
*/
struct laplacian_problem {

  enum { DIRICHLET_BOUNDARY_NUM = 0, NEUMANN_BOUNDARY_NUM = 1};
  getfem::getfem_mesh mesh; /* the mesh */
  getfem::mesh_fem mf_u;    /* the main mesh_fem, for the Laplacian solution */
  getfem::mesh_fem mf_rhs;  /* the mesh_fem for the right hand side(f(x),..) */
  getfem::mesh_fem mf_coef; /* the mesh_fem to represent pde coefficients   */

  scalar_type residu;        /* max residu for the iterative solvers */
  size_type est_degree;
  bool gen_dirichlet;

  sparse_matrix_type SM;     /* stiffness matrix.                           */
  std::vector<scalar_type> U, B;      /* main unknown, and right hand side  */

  std::vector<scalar_type> Ud; /* reduced sol. for gen. Dirichlet condition. */
  col_sparse_matrix_type NS; /* Dirichlet NullSpace 
			      * (used if gen_dirichlet is true)
			      */
  std::string datafilename;
  ftool::md_param PARAM;

  void assembly(void);
  bool solve(void);
  void init(void);
  void compute_error();
  laplacian_problem(void) : mf_u(mesh), mf_rhs(mesh), mf_coef(mesh) {}
};

/* Read parameters from the .param file, build the mesh, set finite element
 * and integration methods and selects the boundaries.
 */
void laplacian_problem::init(void)
{
  const char *MESH_TYPE = PARAM.string_value("MESH_TYPE","Mesh type ");
  const char *FEM_TYPE  = PARAM.string_value("FEM_TYPE","FEM name");
  const char *INTEGRATION = PARAM.string_value("INTEGRATION",
					       "Name of integration method");
  cout << "MESH_TYPE=" << MESH_TYPE << "\n";
  cout << "FEM_TYPE="  << FEM_TYPE << "\n";
  cout << "INTEGRATION=" << INTEGRATION << "\n";

  /* First step : build the mesh */
  bgeot::pgeometric_trans pgt = 
    bgeot::geometric_trans_descriptor(MESH_TYPE);
  size_type N = pgt->dim();
  std::vector<size_type> nsubdiv(N);
  std::fill(nsubdiv.begin(),nsubdiv.end(),
	    PARAM.int_value("NX", "Nomber of space steps "));
  getfem::regular_unit_mesh(mesh, nsubdiv, pgt,
			    PARAM.int_value("MESH_NOISED") != 0);
  
  bgeot::base_matrix M(N,N);
  for (size_type i=0; i < N; ++i) {
    static const char *t[] = {"LX","LY","LZ"};
    M(i,i) = (i<3) ? PARAM.real_value(t[i],t[i]) : 1.0;
  }
  if (N>1) { M(0,1) = PARAM.real_value("INCLINE") * PARAM.real_value("LY"); }

  /* scale the unit mesh to [LX,LY,..] and incline it */
  mesh.transformation(M);

  datafilename = PARAM.string_value("ROOTFILENAME","Base name of data files.");
  scalar_type FT = PARAM.real_value("FT", "parameter for exact solution");
  residu = PARAM.real_value("RESIDU"); if (residu == 0.) residu = 1e-10;
  sol_K.resize(N);
  for (size_type j = 0; j < N; j++)
    sol_K[j] = ((j & 1) == 0) ? FT : -FT;

  /* set the finite element on the mf_u */
  getfem::pfem pf_u = getfem::fem_descriptor(FEM_TYPE);
  est_degree = pf_u->estimated_degree();
  getfem::pintegration_method ppi = getfem::int_method_descriptor(INTEGRATION);

  mf_u.set_finite_element(mesh.convex_index(), pf_u, ppi);

  /* set the finite element on mf_rhs (same as mf_u is DATA_FEM_TYPE is
     not used in the .param file */
  const char *data_fem_name = PARAM.string_value("DATA_FEM_TYPE");
  if (data_fem_name == 0) {
    if (!pf_u->is_lagrange()) {
      DAL_THROW(dal::failure_error, "You are using a non-lagrange FEM. "
		<< "In that case you need to set "
		<< "DATA_FEM_TYPE in the .param file");
    }
    mf_rhs.set_finite_element(mesh.convex_index(), pf_u, ppi);
  } else {
    mf_rhs.set_finite_element(mesh.convex_index(), 
			      getfem::fem_descriptor(data_fem_name), ppi);
  }
  
  /* set the finite element on mf_coef. Here we use a very simple element
   *  since the only function that need to be interpolated on the mesh_fem 
   * is f(x)=1 ... */
  mf_coef.set_finite_element(mesh.convex_index(),
			     getfem::classical_fem(pgt,0), ppi);

  /* set boundary conditions
   * (Neuman on the upper face, Dirichlet elsewhere) */
  gen_dirichlet = PARAM.int_value("GENERIC_DIRICHLET");
  cout << "Selecting Neumann and Dirichlet boundaries\n";
  getfem::convex_face_ct border_faces;
  getfem::outer_faces_of_mesh(mesh, border_faces);
  for (getfem::convex_face_ct::const_iterator it = border_faces.begin();
       it != border_faces.end(); ++it) {
    assert(it->f != size_type(-1));
    base_node un = mesh.normal_of_face_of_convex(it->cv, it->f);
    un /= gmm::vect_norm2(un);
    if (dal::abs(un[N-1] - 1.0) < 1.0E-7) { // new Neumann face
      mesh.add_face_to_set(NEUMANN_BOUNDARY_NUM, it->cv, it->f);
    } else {
      mesh.add_face_to_set(DIRICHLET_BOUNDARY_NUM, it->cv, it->f);
    }
  }
}

void laplacian_problem::assembly(void)
{
  size_type nb_dof = mf_u.nb_dof();
  size_type nb_dof_rhs = mf_rhs.nb_dof();

  gmm::resize(B, nb_dof); gmm::clear(B);
  gmm::resize(U, nb_dof); gmm::clear(U); 
  gmm::resize(SM, nb_dof, nb_dof); gmm::clear(SM);
  
  cout << "Number of dof : " << nb_dof << endl;
  cout << "Assembling stiffness matrix" << endl;
  getfem::asm_stiffness_matrix_for_laplacian(SM, mf_u, mf_coef, 
     std::vector<scalar_type>(mf_coef.nb_dof(), 1.0));
  
  cout << "Assembling source term" << endl;
  std::vector<scalar_type> F(nb_dof_rhs);
  for (size_type i = 0; i < nb_dof_rhs; ++i)
    F[i] = sol_f(mf_rhs.point_of_dof(i));
  getfem::asm_source_term(B, mf_u, mf_rhs, F);
  
  cout << "Assembling Neumann condition" << endl;
  /* Fill F with Grad(sol_u).n .. a bit complicated */
  for (dal::bv_visitor cv(mesh.convexes_in_set(NEUMANN_BOUNDARY_NUM));
       !cv.finished(); ++cv) {
    getfem::pfem pf = mf_rhs.fem_of_element(cv);
    getfem::mesh_cvf_set::face_bitset fb = mesh.faces_of_convex_in_set(cv,
						      NEUMANN_BOUNDARY_NUM);
    for (unsigned f = 0; f < MAX_FACES_PER_CV; ++f) if (fb[f]) {
      for (size_type l = 0; l< pf->structure(cv)->nb_points_of_face(f); ++l) {
	size_type n = pf->structure(cv)->ind_points_of_face(f)[l];
	base_small_vector un
	  = mesh.normal_of_face_of_convex(cv, f, pf->node_of_dof(cv, n));
	un /= gmm::vect_norm2(un);
	size_type dof = mf_rhs.ind_dof_of_element(cv)[n];
	F[dof] = gmm::vect_sp(sol_grad(mf_rhs.point_of_dof(dof)), un);
      }
    }
  }
  getfem::asm_source_term(B, mf_u, mf_rhs, F, NEUMANN_BOUNDARY_NUM);

  cout << "take Dirichlet condition into account" << endl;  
  if (!gen_dirichlet) {    
    std::vector<scalar_type> D(nb_dof);
    for (size_type i = 0; i < nb_dof; ++i)
      D[i] = sol_u(mf_u.point_of_dof(i));
    getfem::assembling_Dirichlet_condition(SM, B, mf_u, 
					   DIRICHLET_BOUNDARY_NUM, D);
  } else {
    for (size_type i = 0; i < nb_dof_rhs; ++i)
      F[i] = sol_u(mf_rhs.point_of_dof(i));
    
    gmm::resize(Ud, nb_dof);
    gmm::resize(NS, nb_dof, nb_dof);
    col_sparse_matrix_type H(nb_dof, nb_dof);
    std::vector<scalar_type> R(nb_dof), RHaux(nb_dof);

    /* build H and R such that U mush satisfy H*U = R */
    getfem::asm_dirichlet_constraints(H, R, mf_u,
				      mf_rhs, F, DIRICHLET_BOUNDARY_NUM);    
    gmm::clean(H, 1e-15);
    int nbcols = getfem::Dirichlet_nullspace(H, NS, R, Ud);
    // cout << "Number of irreductible unknowns : " << nbcols << endl;
    gmm::resize(NS,gmm::mat_ncols(H),nbcols);

    gmm::mult(SM, Ud, gmm::scaled(B, -1.0), RHaux);
    gmm::resize(B, nbcols);
    gmm::resize(U, nbcols);
    gmm::mult(gmm::transposed(NS), gmm::scaled(RHaux, -1.0), B);
    sparse_matrix_type SMaux(nbcols, nb_dof);
    gmm::mult(gmm::transposed(NS), SM, SMaux);
    gmm::resize(SM, nbcols, nbcols);
    /* NSaux = NS, but is stored by rows instead of by columns */
    sparse_matrix_type NSaux(nb_dof, nbcols); gmm::copy(NS, NSaux);
    gmm::mult(SMaux, NSaux, SM);
  }
}

bool laplacian_problem::solve(void) {
  cout << "Compute preconditionner\n";
  double time = ftool::uclock_sec();
  gmm::iteration iter(residu, 1, 40000);
  // gmm::identity_matrix P;
  // gmm::diagonal_precond<sparse_matrix_type> P(SM);
  // gmm::mr_approx_inverse_precond<sparse_matrix_type> P(SM, 10, 10E-17);
  // gmm::ildlt_precond<sparse_matrix_type> P(SM);
  // gmm::ildltt_precond<sparse_matrix_type> P(SM, 50, 1E-9);
  gmm::ilut_precond<sparse_matrix_type> P(SM, 50, 1E-9);
  // gmm::ilutp_precond<sparse_matrix_type> P(SM, 50, 1E-9);
  // gmm::ilu_precond<sparse_matrix_type> P(SM);
  cout << "Time to compute preconditionner : "
       << ftool::uclock_sec() - time << " seconds\n";

  gmm::cg(SM, U, B, P, iter);
  // gmm::gmres(SM, U, B, P, 50, iter);
  
  cout << "Total time to solve : "
       << ftool::uclock_sec() - time << " seconds\n";

  if (gen_dirichlet) {
    std::vector<scalar_type> Uaux(mf_u.nb_dof());
    gmm::mult(NS, U, Ud, Uaux);
    gmm::resize(U, mf_u.nb_dof());
    gmm::copy(Uaux, U);
  }

  return (iter.converged());
}

/* compute the error with respect to the exact solution */
void laplacian_problem::compute_error() {
  std::vector<scalar_type> V(mf_rhs.nb_dof());
  getfem::interpolation(mf_u, mf_rhs, U, V);
  for (size_type i = 0; i < mf_rhs.nb_dof(); ++i)
    V[i] -= sol_u(mf_rhs.point_of_dof(i));
  cout.precision(16);
  cout << "L2 error = " << getfem::asm_L2_norm(mf_rhs, V) << endl
       << "H1 error = " << getfem::asm_H1_norm(mf_rhs, V) << endl
       << "Linfty error = " << gmm::vect_norminf(V) << endl;     
}

/**************************************************************************/
/*  main program.                                                         */
/**************************************************************************/

int main(int argc, char *argv[]) {
  dal::exception_callback_debug cb;
  dal::exception_callback::set_exception_callback(&cb); // In order to debug ...

#ifdef GETFEM_HAVE_FEENABLEEXCEPT /* trap SIGFPE */
  feenableexcept(FE_DIVBYZERO | FE_INVALID);
#endif

  try {    
    laplacian_problem p;
    p.PARAM.read_command_line(argc, argv);
    p.init();
    p.mesh.write_to_file(p.datafilename + ".mesh");
    p.assembly();
    if (!p.solve()) DAL_THROW(dal::failure_error, "Solve procedure has failed");
    p.compute_error();
    getfem::save_solution(p.datafilename + ".dataelt", p.mf_u, p.U,
			  p.est_degree);
  }
  DAL_STANDARD_CATCH_ERROR;

  return 0; 
}
