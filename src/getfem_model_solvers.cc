/*===========================================================================

 Copyright (C) 2009-2017 Yves Renard

 This file is a part of GetFEM++

 GetFEM++  is  free software;  you  can  redistribute  it  and/or modify it
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

#include "getfem/getfem_model_solvers.h"
#include "gmm/gmm_inoutput.h"
#include <iomanip>

namespace getfem {


  static rmodel_plsolver_type rdefault_linear_solver(const model &md) {
    return default_linear_solver<model_real_sparse_matrix,
                                 model_real_plain_vector>(md);
  }

  static cmodel_plsolver_type cdefault_linear_solver(const model &md) {
    return default_linear_solver<model_complex_sparse_matrix,
                                 model_complex_plain_vector>(md);
  }

  void default_newton_line_search::init_search(double r, size_t git, double) {
    alpha_min_ratio = 0.9;
    alpha_min = 1e-10;
    alpha_max_ratio = 10.0;
    alpha_mult = 0.25;
    itmax = size_type(-1);
    glob_it = git; if (git <= 1) count_pat = 0;
    conv_alpha = alpha = alpha_old = 1.;
    conv_r = first_res = r; it = 0;
    count = 0;
    max_ratio_reached = false;
  }

  double default_newton_line_search::next_try() {
    alpha_old = alpha; ++it;
    // alpha *= 0.5;
    if (alpha >= 0.4) alpha *= 0.5; else alpha *= alpha_mult;
    return alpha_old;
  }

  bool default_newton_line_search::is_converged(double r, double) {
    // cout << "r = " << r << " alpha = " << alpha_old << " count_pat = " << count_pat << endl;
    if (!max_ratio_reached && r < first_res * alpha_max_ratio) {
      alpha_max_ratio_reached = alpha_old; r_max_ratio_reached = r;
      it_max_ratio_reached = it; max_ratio_reached = true;
    }
    if (max_ratio_reached &&
        r < r_max_ratio_reached * 0.5 &&
        r > first_res * 1.1 && it <= it_max_ratio_reached+1) {
      alpha_max_ratio_reached = alpha_old; r_max_ratio_reached = r;
      it_max_ratio_reached = it;
    }
    if (count == 0 || r < conv_r)
      { conv_r = r; conv_alpha = alpha_old; count = 1; }
    if (conv_r < first_res) ++count;

    if (r < first_res *  alpha_min_ratio)
      { count_pat = 0; return true; }
    if (count>=5 || (alpha < alpha_min && max_ratio_reached) || alpha<1e-15) {
      if (conv_r < first_res * 0.99) count_pat = 0;
      if (/*gmm::random() * 50. < -log(conv_alpha)-4.0 ||*/ count_pat >= 3)
        { conv_r=r_max_ratio_reached; conv_alpha=alpha_max_ratio_reached; }
      if (conv_r >= first_res * 0.999) count_pat++;
      return true;
    }
    return false;
  }


  /* ***************************************************************** */
  /*     Computation of initial values of velocity/acceleration for    */
  /*     time integration schemes.                                     */
  /* ***************************************************************** */

  template <typename MATRIX, typename VECTOR, typename PLSOLVER>
  void compute_init_values(model &md, gmm::iteration &iter,
                           PLSOLVER lsolver,
                           abstract_newton_line_search &ls, const MATRIX &K,
                           const VECTOR &rhs) {

    VECTOR state(md.nb_dof());
    md.from_variables(state);
    md.cancel_init_step();
    md.set_time_integration(2);
    scalar_type dt = md.get_time_step();
    scalar_type ddt = md.get_init_time_step();
    scalar_type t = md.get_time();

    // Solve for ddt
    md.set_time_step(ddt);
    gmm::iteration iter1 = iter;
    standard_solve(md, iter1, lsolver, ls, K, rhs);
    md.copy_init_time_derivative();

    // Restore the model state
    md.set_time_step(dt);
    md.set_time(t);
    md.to_variables(state);
    md.set_time_integration(1);
  }

  /* ***************************************************************** */
  /*     Standard solve.                                               */
  /* ***************************************************************** */

  template <typename MATRIX, typename VECTOR, typename PLSOLVER>
  void standard_solve(model &md, gmm::iteration &iter,
                      PLSOLVER lsolver,
                      abstract_newton_line_search &ls, const MATRIX &K,
                      const VECTOR &rhs) {

    VECTOR state(md.nb_dof());
    md.from_variables(state); // copy the model variables in the state vector

    int time_integration = md.is_time_integration();
    if (time_integration) {
      if (time_integration == 1 && md.is_init_step()) {
        compute_init_values(md, iter, lsolver, ls, K, rhs);
        return;
      }
      md.set_time(md.get_time() + md.get_time_step());
      md.call_init_affine_dependent_variables(time_integration);
    }

    if (md.is_linear()) {
      md.assembly(model::BUILD_ALL);
      (*lsolver)(K, state, rhs, iter);
    }
    else {
      model_pb<MATRIX, VECTOR> mdpb(md, ls, state, rhs, K);
      if (dynamic_cast<newton_search_with_step_control *>(&ls))
        Newton_with_step_control(mdpb, iter, *lsolver);
      else
        classical_Newton(mdpb, iter, *lsolver);
    }
    md.to_variables(state); // copy the state vector into the model variables
  }

  void standard_solve(model &md, gmm::iteration &iter,
                      rmodel_plsolver_type lsolver,
                      abstract_newton_line_search &ls) {
    standard_solve(md, iter, lsolver, ls, md.real_tangent_matrix(),
                   md.real_rhs());
  }

  void standard_solve(model &md, gmm::iteration &iter,
                      cmodel_plsolver_type lsolver,
                      abstract_newton_line_search &ls) {
    standard_solve(md, iter, lsolver, ls, md.complex_tangent_matrix(),
                   md.complex_rhs());
  }


  void standard_solve(model &md, gmm::iteration &iter,
                      rmodel_plsolver_type lsolver) {
    default_newton_line_search ls;
    standard_solve(md, iter, lsolver, ls);
  }

  void standard_solve(model &md, gmm::iteration &iter,
                      cmodel_plsolver_type lsolver) {
    newton_search_with_step_control ls;
    // default_newton_line_search ls;
    standard_solve(md, iter, lsolver, ls);
  }

  void standard_solve(model &md, gmm::iteration &iter) {
    newton_search_with_step_control ls;
    // default_newton_line_search ls;
    if (md.is_complex())
      standard_solve(md, iter, cdefault_linear_solver(md), ls);
    else
      standard_solve(md, iter, rdefault_linear_solver(md), ls);
  }



}  /* end of namespace getfem.                                             */

