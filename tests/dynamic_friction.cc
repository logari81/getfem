/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2002-2005 Yves Renard, Julien Pommier, Houari Khenous.    */
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
 * Dynamic friction in linear elasticity.
 *
 * This program is used to check that getfem++ is working. This is also 
 * a good example of use of Getfem++.
*/


#include <getfem_assembling.h> /* import assembly methods (and norms comp.) */
#include <getfem_export.h>   /* export functions (save solution in a file)  */
#include <getfem_import.h>
#include <getfem_regular_meshes.h>
#include <getfem_Coulomb_friction.h>
#include <gmm.h>
#include <fstream>
/* try to enable the SIGFPE if something evaluates to a Not-a-number
 * of infinity during computations
 */
#ifdef GETFEM_HAVE_FEENABLEEXCEPT
#  include <fenv.h>
#endif

/* some Getfem++ types that we will be using */
using bgeot::base_small_vector; /* special class for small (dim<16) vectors */
using bgeot::base_node;  /* geometrical nodes(derived from base_small_vector)*/
using bgeot::scalar_type; /* = double */
using bgeot::size_type;   /* = unsigned long */
using bgeot::base_matrix; /* small dense matrix. */

/* definition of some matrix/vector types. 
 * default types of getfem_modeling.h
 */
typedef getfem::modeling_standard_sparse_vector sparse_vector;
typedef getfem::modeling_standard_sparse_matrix sparse_matrix;
typedef getfem::modeling_standard_plain_vector  plain_vector;

/*
 * structure for the friction problem
 */
struct friction_problem {

  enum {
    DIRICHLET_BOUNDARY, CONTACT_BOUNDARY, PERIODIC_BOUNDARY1,
    PERIODIC_BOUNDARY2 
  };
  getfem::getfem_mesh mesh;  /* the mesh */
  getfem::mesh_im  mim;      /* integration methods.                         */
  getfem::mesh_fem mf_u;     /* main mesh_fem, for the friction solution     */
  getfem::mesh_fem mf_rhs;   /* mesh_fem for the right hand side (f(x),..)   */
  getfem::mesh_fem mf_coef;  /* mesh_fem used to represent pde coefficients  */
  scalar_type lambda, mu;    /* Lam� coefficients.                           */
  scalar_type rho, PG;       /* density, and gravity                         */
  scalar_type friction_coef; /* friction coefficient.                        */

  scalar_type residu;        /* max residu for the iterative solvers         */
  
  int scheme;  /* 0 = theta method, 1 = Newmark, 2 = middle point,           */
               /* 3 = middle point with separated contact forces.            */
  size_type N, noisy, nocontact_mass;
  scalar_type beta, theta, gamma, restit;
  scalar_type T, dt, r;
  scalar_type init_vert_pos, init_vert_speed, hspeed, dtexport;
  scalar_type pert_stationary, Dirichlet_ratio;
  bool dt_adapt, periodic, dxexport, Dirichlet, init_stationary;

  std::string datafilename;
  ftool::md_param PARAM;

  void stationary(plain_vector &U0, plain_vector &LN, plain_vector &LT);
  void solve(void);
  void init(void);
  friction_problem(void) : mim(mesh), mf_u(mesh), mf_rhs(mesh), mf_coef(mesh) {}
};

/* Read parameters from the .param file, build the mesh, set finite element
 * and integration methods and selects the boundaries.
 */
void friction_problem::init(void) {
  const char *MESH_TYPE = PARAM.string_value("MESH_TYPE","Mesh type ");
  const char *FEM_TYPE  = PARAM.string_value("FEM_TYPE","FEM name");
  const char *INTEGRATION = PARAM.string_value("INTEGRATION",
					       "Name of integration method");
  cout << "MESH_TYPE=" << MESH_TYPE << "\n";
  cout << "FEM_TYPE="  << FEM_TYPE << "\n";
  cout << "INTEGRATION=" << INTEGRATION << "\n";

  std::string meshname
    (PARAM.string_value("MESHNAME", "Nom du fichier de maillage"));

  /* First step : build the mesh */
  bgeot::pgeometric_trans pgt = 
    bgeot::geometric_trans_descriptor(MESH_TYPE);
  N = pgt->dim();
  if (meshname.compare(0,5, "splx:")==0) {
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
    mesh.transformation(M);
  }
  else getfem::import_mesh(meshname, mesh);
  mesh.optimize_structure();

  datafilename = PARAM.string_value("ROOTFILENAME","Base name of data files.");
  residu = PARAM.real_value("RESIDU"); if (residu == 0.) residu = 1e-10;

  mu = PARAM.real_value("MU", "Lam� coefficient mu");
  lambda = PARAM.real_value("LAMBDA", "Lam� coefficient lambda");
  rho = PARAM.real_value("RHO", "Density");
  PG = PARAM.real_value("PG", "Gravity constant");
  friction_coef = PARAM.real_value("FRICTION_COEF", "Friction coefficient");

  scheme = PARAM.int_value("SCHEME", "Time scheme");
  theta = PARAM.real_value("THETA", "Parameter for the theta-method"); 
  beta = PARAM.real_value("BETA", "Parameter beta for Newmark");
  gamma = PARAM.real_value("GAMMA", "Parameter gamma for Newmark");
  restit = PARAM.real_value("RESTIT", "Resitution parameter");

  Dirichlet = PARAM.int_value("DIRICHLET","Dirichlet condition or not");
  Dirichlet_ratio = PARAM.real_value("DIRICHLET_RATIO",
				     "parameter for Dirichlet condition");
  T = PARAM.real_value("T", "from [0,T] the time interval");
  dt = PARAM.real_value("DT", "time step");
  dt_adapt = (PARAM.int_value("DT_ADAPT", "time step adaptation") != 0);
  periodic = (PARAM.int_value("PERIODICITY", "peridiodic condition or not")
	      != 0);
  dxexport = (PARAM.int_value("DX_EXPORT", "Exporting on OpenDX format")
	      != 0);
  dtexport = PARAM.real_value("DT_EXPORT", "time step for the export");
  dtexport = dt * double(int(dtexport / dt + 0.5));
  nocontact_mass = PARAM.int_value("NOCONTACT_MASS", "Suppress the mass "
				   "of contact nodes");
  r = PARAM.real_value("R", "augmentation parameter");
  noisy = (PARAM.int_value("NOISY", "verbosity of iterative methods") != 0);
  init_vert_pos = PARAM.real_value("INIT_VERT_POS", "initial position");
  init_vert_speed = PARAM.real_value("INIT_VERT_SPEED","initial speed");
  hspeed = PARAM.real_value("FOUNDATION_HSPEED","initial speed");
  init_stationary = (PARAM.int_value("STATIONARY",
				     "initial condition is stationary") != 0);
  pert_stationary = PARAM.real_value("PERT_STATIONARY",
				     "Perturbation on the initial velocity");
  mf_u.set_qdim(N);

  /* set the finite element on the mf_u */
  getfem::pfem pf_u = getfem::fem_descriptor(FEM_TYPE);
  getfem::pintegration_method ppi = 
    getfem::int_method_descriptor(INTEGRATION);

  mim.set_integration_method(mesh.convex_index(), ppi);
  mf_u.set_finite_element(mesh.convex_index(), pf_u);

  /* set the finite element on mf_rhs (same as mf_u is DATA_FEM_TYPE is
     not used in the .param file */
  const char *data_fem_name = PARAM.string_value("DATA_FEM_TYPE");
  if (data_fem_name == 0) {
    if (!pf_u->is_lagrange()) {
      DAL_THROW(dal::failure_error, "You are using a non-lagrange FEM. "
		<< "In that case you need to set "
		<< "DATA_FEM_TYPE in the .param file");
    }
    mf_rhs.set_finite_element(mesh.convex_index(), pf_u);
  } else {
    mf_rhs.set_finite_element(mesh.convex_index(), 
			      getfem::fem_descriptor(data_fem_name));
  }
  
  mf_coef.set_finite_element(mesh.convex_index(),
			     getfem::classical_fem(pgt,0));

  /* set boundary conditions */
  base_node center(0.,0.,20.);
  std::cout << "Reperage des bord de contact et Dirichlet\n";  
  for (dal::bv_visitor cv(mesh.convex_index()); !cv.finished(); ++cv) {
    size_type nf = mesh.structure_of_convex(cv)->nb_faces();
    for (size_type f = 0; f < nf; f++) {
      if (bgeot::neighbour_of_convex(mesh, cv, f).empty()) {
	base_small_vector un = mesh.normal_of_face_of_convex(cv, f);
	un /= gmm::vect_norm2(un);	
	base_node pt = mesh.points_of_face_of_convex(cv,f)[0];
	if (un[N-1] < -0.000001 && (N != 3 || (bgeot::vect_dist2(pt, center)
			   > .99*sqrt(25. + 15*15) && pt[N-1] < 20.1)))
	  mesh.add_face_to_set(CONTACT_BOUNDARY, cv, f); 
	if (un[0] > 0.98) mesh.add_face_to_set(PERIODIC_BOUNDARY1, cv, f); 
	if (un[0] < -0.98) mesh.add_face_to_set(PERIODIC_BOUNDARY2, cv, f); 
	if (un[N-1] > 0.1 && Dirichlet)
	  mesh.add_face_to_set(DIRICHLET_BOUNDARY, cv, f);
      }
    }
  }
}

/**************************************************************************/
/*  Computation of the stationary solution.                               */
/**************************************************************************/

void friction_problem::stationary(plain_vector &U0, plain_vector &LN,
				  plain_vector &LT) {
  size_type nb_dof_rhs = mf_rhs.nb_dof();
  N = mesh.dim();

  // Linearized elasticity brick.
  getfem::mdbrick_isotropic_linearized_elasticity<>
    ELAS(mim, mf_u, mf_coef, lambda, mu, true);

  // Defining the volumic source term.
  plain_vector F(nb_dof_rhs * N);
  plain_vector f(N); f[N-1] = -rho*PG;
  for (size_type i = 0; i < nb_dof_rhs; ++i)
      gmm::copy(f,gmm::sub_vector(F, gmm::sub_interval(i*N, N)));
  
  // Volumic source term brick.
  getfem::mdbrick_source_term<> VOL_F(ELAS, mf_rhs, F);

  // Dirichlet condition brick.
  gmm::clear(F);
  for (size_type i = 0; i < nb_dof_rhs; ++i)
    F[(i+1)*N-1] = Dirichlet_ratio * mf_rhs.point_of_dof(i)[N-1];
  getfem::mdbrick_Dirichlet<> DIRICHLET(VOL_F, mf_rhs, F, DIRICHLET_BOUNDARY);

  // contact condition for Lagrange elements
  dal::bit_vector cn = mf_u.dof_on_set(CONTACT_BOUNDARY);
  if (periodic) cn.setminus(mf_u.dof_on_set(PERIODIC_BOUNDARY1));
  sparse_matrix BN(cn.card()/N, mf_u.nb_dof());
  sparse_matrix BT((N-1)*cn.card()/N, mf_u.nb_dof());
  plain_vector gap(cn.card()/N);
  size_type jj = 0;
  for (dal::bv_visitor i(cn); !i.finished(); ++i)
    if (i % N == 0) {
      BN(jj, i+N-1) = -1.;
      gap[jj] = mf_u.point_of_dof(i)[N-1];
      for (size_type k = 0; k < N-1; ++k) BT((N-1)*jj+k, i+k) = 1.;
      ++jj;
    }

  getfem::mdbrick_Coulomb_friction<>
    FRICTION(DIRICHLET, BN, gap, friction_coef, BT);

  // Eventual periodic condition (lagrange elements only).
  sparse_matrix BP(0,mf_u.nb_dof());
  if (periodic) {
    dal::bit_vector b1 = mf_u.dof_on_set(PERIODIC_BOUNDARY1);
    dal::bit_vector b2 = mf_u.dof_on_set(PERIODIC_BOUNDARY2);
    dal::bit_vector bd = mf_u.dof_on_set(DIRICHLET_BOUNDARY);
    b1.setminus(bd); b2.setminus(bd);
    gmm::resize(BP, b1.card(), mf_u.nb_dof());
    size_type k =0;
    for (dal::bv_visitor i(b1); !i.finished(); ++i, ++k)
      if (i % N == 0) {
	for (dal::bv_visitor j(b2); !j.finished(); ++j) 
	  if (j % N == 0) {
	    base_node pt = mf_u.point_of_dof(i) - mf_u.point_of_dof(j);
	    pt[0] = 0.;
	    if (gmm::vect_norm2(pt) < 1E-4) {
	      for (size_type l = 0; l < N; ++l)
		{ BP(k+l, i+l) = 1.; BP(k+l, j+l) = -1.; }
	      break; 
	    }
	  }
      }
  }
  gmm::resize(F, gmm::mat_nrows(BP)); gmm::clear(F);
  getfem::mdbrick_constraint<> PERIODIC(FRICTION, BP, F);
  getfem::standard_model_state MS(PERIODIC);
  
  FRICTION.set_r(r); 
   
  plain_vector WT(mf_u.nb_dof()), HSPEED(mf_u.nb_dof());
  for (size_type i=0; i < mf_u.nb_dof(); ++i)
    if ((i % N) == 0) HSPEED[i] = -hspeed;
  FRICTION.set_WT(HSPEED);
  FRICTION.set_stationary(true);

  cout << "Computation of the stationary problem\n";
  gmm::iteration iter(residu, noisy, 40000);
  getfem::standard_solve(MS, PERIODIC, iter);

  gmm::copy(ELAS.get_solution(MS), U0);
  gmm::copy(FRICTION.get_LN(MS), LN);
  gmm::copy(FRICTION.get_LT(MS), LT);

}

/**************************************************************************/
/*  Model.                                                                */
/**************************************************************************/

void friction_problem::solve(void) {
  size_type nb_dof_rhs = mf_rhs.nb_dof();
  N = mesh.dim();
  cout << "Number of dof for u: " << mf_u.nb_dof() << endl;

  // Linearized elasticity brick.
  getfem::mdbrick_isotropic_linearized_elasticity<>
    ELAS(mim, mf_u, mf_coef, lambda, mu, true);

  // Defining the volumic source term.
  plain_vector F(nb_dof_rhs * N);
  plain_vector f(N); f[N-1] = -rho*PG;
  for (size_type i = 0; i < nb_dof_rhs; ++i)
      gmm::copy(f,gmm::sub_vector(F, gmm::sub_interval(i*N, N)));
  
  // Volumic source term brick.
  getfem::mdbrick_source_term<> VOL_F(ELAS, mf_rhs, F);

  // Dirichlet condition brick.
  gmm::clear(F);
  for (size_type i = 0; i < nb_dof_rhs; ++i)
    F[(i+1)*N-1] = Dirichlet_ratio * mf_rhs.point_of_dof(i)[N-1];
  getfem::mdbrick_Dirichlet<> DIRICHLET(VOL_F, mf_rhs, F, DIRICHLET_BOUNDARY);
  
  // contact condition for Lagrange elements
  dal::bit_vector cn = mf_u.dof_on_set(CONTACT_BOUNDARY);
  if (periodic) cn.setminus(mf_u.dof_on_set(PERIODIC_BOUNDARY1));
  sparse_matrix BN(cn.card()/N, mf_u.nb_dof());
  sparse_matrix BT((N-1)*cn.card()/N, mf_u.nb_dof());
  plain_vector gap(cn.card()/N);
  size_type jj = 0;
  for (dal::bv_visitor i(cn); !i.finished(); ++i)
    if (i % N == 0) {
      BN(jj, i+N-1) = -1.;
      gap[jj] = mf_u.point_of_dof(i)[N-1];
      for (size_type k = 0; k < N-1; ++k) BT((N-1)*jj+k, i+k) = 1.;
      ++jj;
    }

  getfem::mdbrick_Coulomb_friction<>
    FRICTION(DIRICHLET, BN, gap,
	     friction_coef * ((scheme == 3) ? (1./theta) : 1.), BT);

  // Dynamic brick.
  getfem::mdbrick_dynamic<> DYNAMIC(FRICTION, mf_coef, rho);
  if (nocontact_mass) DYNAMIC.no_mass_on_boundary(CONTACT_BOUNDARY);

  // Eventual periodic condition (lagrange element only).
  sparse_matrix BP(0,mf_u.nb_dof());
  if (periodic) {
    dal::bit_vector b1 = mf_u.dof_on_set(PERIODIC_BOUNDARY1);
    dal::bit_vector b2 = mf_u.dof_on_set(PERIODIC_BOUNDARY2);
    dal::bit_vector bd = mf_u.dof_on_set(DIRICHLET_BOUNDARY);
    b1.setminus(bd); b2.setminus(bd);
    gmm::resize(BP, b1.card(), mf_u.nb_dof());
    size_type k =0;
    for (dal::bv_visitor i(b1); !i.finished(); ++i, ++k)
      if (i % N == 0) {
	for (dal::bv_visitor j(b2); !j.finished(); ++j) 
	  if (j % N == 0) {
	    base_node pt = mf_u.point_of_dof(i) - mf_u.point_of_dof(j);
	    pt[0] = 0.;
	    if (gmm::vect_norm2(pt) < 1E-4) {
	      for (size_type l = 0; l < N; ++l)
		{ BP(k+l, i+l) = 1.; BP(k+l, j+l) = -1.; }
	      break; 
	    }
	  }
      }
  }
  gmm::resize(F, gmm::mat_nrows(BP)); gmm::clear(F);
  getfem::mdbrick_constraint<> PERIODIC(DYNAMIC, BP, F);
  
  cout << "Total number of variables: " << PERIODIC.nb_dof() << endl;
  getfem::standard_model_state MS(PERIODIC);

  plain_vector WT(mf_u.nb_dof()), DF(mf_u.nb_dof()), HSPEED(mf_u.nb_dof());
  plain_vector U2(mf_u.nb_dof());
  plain_vector U0(mf_u.nb_dof()), V0(mf_u.nb_dof()), A0(mf_u.nb_dof());
  plain_vector U1(mf_u.nb_dof()), V1(mf_u.nb_dof()), A1(mf_u.nb_dof());
  plain_vector Vdemi(mf_u.nb_dof()), V_demi(mf_u.nb_dof()), Vmi(mf_u.nb_dof());
  plain_vector UU1(N), VV1(N), LLN1(N), ACC(mf_u.nb_dof());
  plain_vector LT0(gmm::mat_nrows(BT)), LN0(gmm::mat_nrows(BN));
  plain_vector LT1(gmm::mat_nrows(BT)), LN1(gmm::mat_nrows(BN));
  scalar_type a(1), b(1), dt0 = dt, t(0), t_export(dtexport), alpha(0);
  scalar_type J_friction0(0), J_friction1(0);
  plain_vector one(mf_u.nb_dof());
  std::fill(one.begin(), one.end(), 1.0);

  sparse_matrix BBT(gmm::mat_nrows(BN), gmm::mat_nrows(BN));
  if (scheme == 4 || scheme == 5) gmm::mult(BN, gmm::transposed(BN), BBT);


  // Initial conditions (U0, V0, M A0 = F)
  gmm::clear(U0); gmm::clear(V0); gmm::clear(LT0);
  for (size_type i=0; i < mf_u.nb_dof(); ++i)
    if ((i % N) == 0) { 
      U0[i+N-1] = Dirichlet ? (Dirichlet_ratio * mf_u.point_of_dof(i)[N-1])
	: init_vert_pos;
      V0[i+N-1] = Dirichlet ? 0.0 : init_vert_speed;
      HSPEED[i] = hspeed;
    }
  
  if (init_stationary) {
    stationary(U0, LN0, LT0);
    gmm::fill_random(V0); gmm::scale(V0, pert_stationary);
  }
 
  gmm::clear(A0);
  gmm::iteration iter(residu, 0, 40000);
  if ((scheme == 0 || scheme == 1) && !nocontact_mass && !init_stationary) {
    plain_vector FA(mf_u.nb_dof());
    gmm::mult(ELAS.stiffness_matrix(), gmm::scaled(U0, -1.0),
 	      VOL_F.source_term(), FA);
    gmm::mult_add(gmm::transposed(BN), LN0, FA);
    gmm::mult_add(gmm::transposed(BT), LT0, FA);
    gmm::cg(DYNAMIC.mass_matrix(), A0, FA, gmm::identity_matrix(), iter);
  }
  iter.set_noisy(noisy);

  scalar_type J0 = 0.5*gmm::vect_sp(ELAS.stiffness_matrix(), U0, U0)
    + 0.5 * gmm::vect_sp(DYNAMIC.mass_matrix(), V0, V0)
    - gmm::vect_sp(VOL_F.source_term(), U0);

  std::auto_ptr<getfem::dx_export> exp;
  getfem::stored_mesh_slice sl;
  if (dxexport) {
    exp.reset(new getfem::dx_export(datafilename + ".dx", false));
    if (N <= 2)
      sl.build(mesh, getfem::slicer_none(),4);
    else
      sl.build(mesh, getfem::slicer_boundary(mesh),4);
    exp->exporting(sl,true);
    exp->exporting_mesh_edges();
    exp->write_point_data(mf_u, U0, "stepinit"); 
    exp->serie_add_object("deformationsteps");
  }
  
  std::ofstream Houari1("iter", std::ios::out);   
  std::ofstream Houari2("nrj", std::ios::out);
  std::ofstream Houari3("vts", std::ios::out);
  std::ofstream Houari4("FN0", std::ios::out);
  std::ofstream Houari5("depl", std::ios::out);
  
  

  while (t <= T) {

    switch (scheme) { // complementary left hand side and velocity complement
    case 0 :
      a = 1./(dt*dt*theta*theta); b = 1.; alpha = 1./(theta*dt);
      gmm::add(gmm::scaled(U0, a), gmm::scaled(V0, dt*a), U1);
      gmm::add(gmm::scaled(A0, (1.-theta)/theta), U1);
      gmm::mult(DYNAMIC.mass_matrix(), U1, DF);
      gmm::add(gmm::scaled(U0, -1.), gmm::scaled(V0, -dt*(1.-theta)), WT);
      break;
    case 1 :
      a = 2./(dt*dt*beta); b = 1.; alpha = 2.*gamma/(beta*dt);
      gmm::add(gmm::scaled(U0, a), gmm::scaled(V0, a*dt), U1);
      gmm::add(gmm::scaled(A0, (1.-beta)/beta), U1);
      gmm::mult(DYNAMIC.mass_matrix(), U1, DF);
      gmm::add(gmm::scaled(U0, -1.),
	       gmm::scaled(V0, dt*(beta*0.5/gamma -1.)), WT);
      gmm::add(gmm::scaled(A0, dt*dt*0.5*(beta-gamma)/gamma), WT);
      break;
    case 2 :
      a = 4./(dt*dt); b = 1.; alpha = 2./dt;
      gmm::add(gmm::scaled(U0, a), gmm::scaled(V0, 2./dt), U1);
      gmm::mult(DYNAMIC.mass_matrix(), U1, DF);
      gmm::copy(gmm::scaled(U0, -1.), WT);
      break;
    case 3 : // for the friction, it should be better to take the average 
      // for the contact forces to define the friction threshold
      a = 2./(dt*dt); b = 1.;  alpha = 1./dt;
      gmm::add(gmm::scaled(U0, a), gmm::scaled(V0, 2./dt), U1);
      gmm::mult(DYNAMIC.mass_matrix(), U1, DF);
      gmm::mult_add(gmm::transposed(BT), gmm::scaled(LN0, (1.-theta)), DF);
      gmm::copy(gmm::scaled(U0, -1.), WT);
      break;
    case 4 : // Paoli-Schatzman scheme DF --> F^n
      gmm::mult(ELAS.stiffness_matrix(), gmm::scaled(U1, -1.),  VOL_F.source_term(), A1);
      iter.init();
      gmm::cg(DYNAMIC.mass_matrix(), DF, A1, gmm::identity_matrix(), iter);
      break;
    case 5:
     
      
      break;
    }


    if (scheme  != 4 && scheme  != 5) {
      gmm::add(gmm::scaled(HSPEED, -1./alpha), WT);
      
      FRICTION.set_WT(WT); FRICTION.set_r(r); FRICTION.set_alpha(alpha); 
      DYNAMIC.set_dynamic_coeff(a, b);
      DYNAMIC.set_DF(DF);
      
      iter.init();
      getfem::standard_solve(MS, PERIODIC, iter);
      gmm::copy(ELAS.get_solution(MS), U1); 
      gmm::copy(FRICTION.get_LN(MS), LN1);
      gmm::copy(FRICTION.get_LT(MS), LT1);
    }
    std::vector<scalar_type> BU(gmm::mat_nrows(BN)), AA(gmm::mat_nrows(BN));

    switch (scheme) { // computation of U^{n+1}, V^{n+1}, A^{n+1}, J_friction1
    case 0 :
      gmm::add(gmm::scaled(U1, 1./dt), gmm::scaled(U0, -1./dt), V1);
      J_friction1 = J_friction0 + dt * theta * gmm::vect_sp(BT, V1, LT1) 
	+ dt * (1.-theta) * gmm::vect_sp(BT, V1, LT0);
      gmm::add(gmm::scaled(V0, -(1.-theta)), V1);
      gmm::scale(V1, 1./theta);
      gmm::add(gmm::scaled(V1, 1./dt), gmm::scaled(V0, -1./dt), A1);
      gmm::add(gmm::scaled(A0, -(1.-theta)), A1);
      gmm::scale(A1, 1./theta);
      break;
    case 1 :
      gmm::add(gmm::scaled(U1, 1.), gmm::scaled(U0, -1.), A1);
      J_friction1 = J_friction0 + (1.-gamma)*gmm::vect_sp(BT, A1, LT0)
	+ theta * gmm::vect_sp(BT, A1, LT1);
      gmm::scale(A1, 2./(beta*dt*dt));
      gmm::add(gmm::scaled(V0, -2./(beta*dt)), A1);
      gmm::add(gmm::scaled(A0, -(1. - beta)/beta), A1);
      gmm::add(gmm::scaled(A0, (1.-gamma)*dt), gmm::scaled(A1, gamma*dt), V1);
      gmm::add(V0, V1);
      break;
    case 2 :
      gmm::copy(U1, V1);
      gmm::add(gmm::scaled(V1, 2.), gmm::scaled(U0, -1.), U1);
      gmm::add(gmm::scaled(U1, 2./dt), gmm::scaled(U0, -2./dt), V1);
      J_friction1 = J_friction0 + dt * 0.5 * gmm::vect_sp(BT, V1, LT1);
      gmm::add(gmm::scaled(V0, -1), V1);
      break;
    case 3 :
      gmm::scale(LN1, 1./theta);
      gmm::add(gmm::scaled(U1, 2./dt), gmm::scaled(U0, -2./dt), V1);
      J_friction1 = J_friction0 + dt * 0.5 * gmm::vect_sp(BT,V1, LT1);
      gmm::add(gmm::scaled(V0, -1), V1);
      break;
    case 4 :
      if (t == 0)
	gmm::add(gmm::scaled(V0, dt), U0, U1); // + terme d'orde sup�rieur ...
      
      /* calcul de la force normal */
      gmm::add(U0,gmm::scaled(U1, -2.), ACC);
      gmm::add(U2, ACC);
      gmm::add(DF, gmm::scaled(ACC, -1./(dt*dt)), ACC);
      gmm::mult(DYNAMIC.mass_matrix(), ACC, ACC);
      gmm::mult(BN, ACC, LN1);

      gmm::add(gmm::scaled(DF, dt*dt), gmm::scaled(U0, restit-1.), U2);
      gmm::add(gmm::scaled(U1, 2.), U2);
      gmm::scale(U2, 1./(1.+restit));
      gmm::mult(BN, U2, BU);
      for (size_type i = 0; i < gmm::mat_nrows(BN); ++i)
	AA[i] = (std::min(BU[i], gap[i]) - BU[i]) / BBT(i,i);
      gmm::mult(gmm::transposed(BN), AA, U2, U2);
      gmm::scale(U2, 1.+restit);
      gmm::add(gmm::scaled(U0, -restit), U2);
      gmm::add(gmm::scaled(U2, 1./(2.*dt)), gmm::scaled(U0, -1./(2.*dt)), V1);
      
      break;
    case 5:
      gmm::add(gmm::scaled(U1, 2.), U0, A0);
      gmm::add(gmm::scaled(U1, -2.), U0, A1);
      gmm::mult(DYNAMIC.mass_matrix(), A1, A1);
      gmm::mult(ELAS.stiffness_matrix(), A0, A0);
      gmm::mult(gmm::transposed(BN), LN0, DF);
      gmm::mult(gmm::transposed(BT), LT0, DF, DF);
      gmm::add(VOL_F.source_term(), DF, DF);
      gmm::add(gmm::scaled(A0, 1./(dt*dt)), DF, DF);
      gmm::add(gmm::scaled(A1, 0.25), DF, DF);
      
      gmm::add(U2, gmm::scaled(U1, -1.), Vdemi);
      gmm::copy(gmm::scaled(Vdemi, 1./(2*dt*dt)), Vdemi);
      gmm::add(U1, gmm::scaled(U0, -1.), V_demi);
      gmm::copy(gmm::scaled(V_demi, 1./(2*dt*dt)), V_demi);
      gmm::add(Vdemi, V_demi, Vmi);

      
     

      if (t == 0)
	gmm::add(gmm::scaled(V0, dt), U0, U1); // + terme d'orde sup�rieur ...
      sparse_matrix DMM(mf_u.nb_dof(), mf_u.nb_dof());
      iter.init();
      gmm::add(gmm::scaled(DYNAMIC.mass_matrix(), 1./(dt*dt)), 
	       gmm::scaled(ELAS.stiffness_matrix(), 0.25), DMM); 
      gmm::cg(DMM, U2, DF, gmm::identity_matrix(), iter);
      
      gmm::add(gmm::scaled(U0, restit), U2);
      gmm::scale(U2, 1./(1.+restit));
      gmm::mult(BN, U2, BU);
      gmm::add(gmm::scaled(BU, (1.+restit)/(dt*dt)), LN0, BU);
      
      for (size_type i = 0; i < gmm::mat_nrows(BN); ++i)
	AA[i] = (std::min(BU[i], gap[i]) - BU[i]) / BBT(i,i);
      gmm::add(AA, LN0);
      
      r = gmm::vect_norm2(LN0);
      if (r <= 0){
	for (size_type i = 0; i < gmm::mat_nrows(BN); ++i) 
	  LT0[i] = 0;
      }
      else{
       	gmm::mult(BN, gmm::scaled(Vmi, r/gmm::vect_norm2(Vmi)), LT0);
      }
      break;
    }
    
//     {
//       plain_vector w(gmm::mat_nrows(BN));
//       gmm::mult(BN, U1, gmm::scaled(gap, -1.), w);
//       cout << "Normal dep : " << w << endl;
//       cout << "Contact pressure : " << LN1 << endl;
//     }

    scalar_type J1 = 0.5*gmm::vect_sp(ELAS.stiffness_matrix(), U1, U1)
      + 0.5 * gmm::vect_sp(DYNAMIC.mass_matrix(), V1, V1)
      - gmm::vect_sp(VOL_F.source_term(), U1);

    if (dt_adapt && gmm::abs(J0-J1) > 1E-4 && dt > 1E-5) {
      dt /= 2.;
      gmm::clear(MS.state());
      cout << "Trying with dt = " << dt << endl;
    }
    else {
      t += dt;

      scalar_type LTtot = gmm::vect_sp(BT,one, LT1);
      scalar_type LNtot = -gmm::vect_sp(BN,one, LN1);
      scalar_type Friction_coef_ap = (LNtot >= 0.) ? 0.
	: gmm::abs(LTtot / LNtot);
      
      size_type nbsl= 0, nbst = 0;
      for (size_type i = 0; i < gmm::mat_nrows(BN); ++i) {
	if (LN1[i] < -1E-12) {
	  if (gmm::vect_norm2(gmm::sub_vector(LT1,
					      gmm::sub_interval(i*(N-1), N-1)))
	      < -friction_coef * double(LN1[i]) * 0.99999)
	    nbst++; else nbsl++;
	}
      }
      

      cout << "t = " << t << " energy : " << J1
	   << " friction energy : " << J_friction1
	   << " app. fric. coef : " << Friction_coef_ap
	   << " (st " << nbst << ", sl " << nbsl << ")" << endl;
      dt = std::min(2.*dt, dt0);

      gmm::copy(gmm::sub_vector(U1, gmm::sub_interval(0,N)), UU1);
      gmm::copy(gmm::sub_vector(V1, gmm::sub_interval(0,N)), VV1);
      gmm::copy(gmm::sub_vector(LN1, gmm::sub_interval(0,N)), LLN1);
      
      scalar_type FN0 = gmm::vect_norm2(LLN1);
      scalar_type depl = gmm::vect_norm2(UU1);
      scalar_type Vts = gmm::vect_norm2(VV1);
      scalar_type time_iteration = t/dt;
      
      
      Houari1 << time_iteration << "\n";
      Houari2 << J1 << "\n";
      Houari3 << Vts << "\n";
      Houari4 << FN0 << "\n";
      Houari5 << depl << "\n";
      
      gmm::copy(U1, U0); gmm::copy(V1, V0); gmm::copy(A1, A0); J0 = J1;
      gmm::copy(LN1, LN0); gmm::copy(LT1, LT0); J_friction0 = J_friction1;
      if (scheme == 4 || scheme == 5) gmm::copy(U2, U1);
      if (dxexport && t >= t_export-dt/20.0) {
	exp->write_point_data(mf_u, U0);
	exp->serie_add_object("deformationsteps");
	t_export += dtexport;
      }
    }
    
  }
}
  
/**************************************************************************/
/*  main program.                                                         */
/**************************************************************************/

int main(int argc, char *argv[]) {
  dal::exception_callback_debug cb;
  dal::exception_callback::set_exception_callback(&cb); // in order to debug

#ifdef GETFEM_HAVE_FEENABLEEXCEPT /* trap SIGFPE */
  feenableexcept(FE_DIVBYZERO | FE_INVALID);
#endif

  try {    
    friction_problem p;
    p.PARAM.read_command_line(argc, argv);
    p.init();
    p.solve();
  }
  DAL_STANDARD_CATCH_ERROR;

  cout << "To see the simulation, you have to set DX_EXPORT to 1 in "
    "dynamic_friction.param and DT_EXPORT to a suitable value (for "
    "instance equal to DT). Then you can use Open_DX (type just \"dx\" "
    "if it is installed on your system) with the Visual Program "
    "dynamic_friction.net (use for instance \"Edit Visual Programs ...\" "
    "with dynamic_friction.net, then \"execute once\" in Execute menu and "
    "use the sequencer to start the animation).\n";

  return 0; 
}
