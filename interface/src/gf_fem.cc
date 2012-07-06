/*===========================================================================
 
 Copyright (C) 2006-2012 Yves Renard, Julien Pommier.
 
 This file is a part of GETFEM++
 
 Getfem++  is  free software;  you  can  redistribute  it  and/or modify it
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
// $Id$
#include <getfemint_misc.h>
#include <getfemint_pfem.h>
#include <getfemint_mesh_fem.h>
#include <getfemint_mesh_im.h>
#include <getfem/getfem_interpolated_fem.h>
#include <getfemint_workspace.h>

using namespace getfemint;
/*@GFDOC
    This object represents a finite element method on a reference element.
@*/



void gf_fem(getfemint::mexargs_in& in, getfemint::mexargs_out& out) {
  if (in.narg() < 1) {
    THROW_BADARG( "Wrong number of input arguments");
  }
  std::string cmd = in.pop().to_string();
  getfemint_pfem *gfi_pf = 0;
  if (check_cmd(cmd, "interpolated fem", in, out, 2, 3, 0, 1)) {
    /*@INIT F = ('interpolated_fem', @tmf mf, @tmim mim, [@ivec blocked_dof])
    Build a special @tfem which is interpolated from another @tmf.

    Using this special finite element, it is possible to interpolate a given
    @tmf `mf` on another mesh, given the integration method `mim` that will
    be used on this mesh.

    Note that this finite element may be quite slow, and eats much
    memory.@*/
    getfemint_mesh_fem *gfi_mf  = in.pop().to_getfemint_mesh_fem();
    getfemint_mesh_im  *gfi_mim = in.pop().to_getfemint_mesh_im();
    dal::bit_vector blocked_dof;
    if (in.remaining())
      blocked_dof = in.pop().to_bit_vector();
    getfem::pfem pf =
      getfem::new_interpolated_fem(gfi_mf->mesh_fem(),
				   gfi_mim->mesh_im(),
				   0, blocked_dof);
    gfi_pf = getfemint_pfem::get_from(pf);
    gfi_pf->nbdof_need_convex_number() = true;
    workspace().set_dependance(gfi_pf, gfi_mim);
    workspace().set_dependance(gfi_pf, gfi_mf);
  } else {
    /*@INIT F = ('.list', @str fem_name)
      The `fem_name` should contain a description of the finite element
      method. Please refer to the getfem++ manual (especially the
      description of finite element and integration methods) for a complete
      reference. Here is a list of some of them:

      - FEM_PK(n,k) :
        classical Lagrange element Pk on a simplex of dimension `n`.
      - FEM_PK_DISCONTINUOUS(n,k[,alpha]) :
        discontinuous Lagrange element Pk on a simplex of dimension `n`.
      - FEM_QK(n,k) :
        classical Lagrange element Qk on quadrangles, hexahedrons etc.
      - FEM_QK_DISCONTINUOUS(n,k[,alpha]) :
        discontinuous Lagrange element Qk on quadrangles, hexahedrons etc.
      - FEM_Q2_INCOMPLETE :
        incomplete 2D Q2 element with 8 dof (serendipity Quad 8 element).
      - FEM_PK_PRISM(n,k) :
        classical Lagrange element Pk on a prism of dimension `n`.
      - FEM_PK_PRISM_DISCONTINUOUS(n,k[,alpha]) :
        classical discontinuous Lagrange element Pk on a prism.
      - FEM_PK_WITH_CUBIC_BUBBLE(n,k) :
        classical Lagrange element Pk on a simplex with an additional
        volumic bubble function.
      - FEM_P1_NONCONFORMING :
        non-conforming P1 method on a triangle.
      - FEM_P1_BUBBLE_FACE(n) :
        P1 method on a simplex with an additional bubble function on face 0.
      - FEM_P1_BUBBLE_FACE_LAG :
        P1 method on a simplex with an additional lagrange dof on face 0.
      - FEM_PK_HIERARCHICAL(n,k) :
        PK element with a hierarchical basis.
      - FEM_QK_HIERARCHICAL(n,k) :
        QK element with a hierarchical basis
      - FEM_PK_PRISM_HIERARCHICAL(n,k) :
        PK element on a prism with a hierarchical basis.
      - FEM_STRUCTURED_COMPOSITE(@tfem f,k) :
        Composite @tfem `f` on a grid with `k` divisions.
      - FEM_PK_HIERARCHICAL_COMPOSITE(n,k,s) :
        Pk composite element on a grid with `s` subdivisions and with a
        hierarchical basis.
      - FEM_PK_FULL_HIERARCHICAL_COMPOSITE(n,k,s) :
        Pk composite element with `s` subdivisions and a hierarchical basis
        on both degree and subdivision.
      - FEM_PRODUCT(A,B) :
        tensorial product of two polynomial elements.
      - FEM_HERMITE(n) :
        Hermite element P3 on a simplex of dimension `n = 1, 2, 3`.
      - FEM_ARGYRIS :
        Argyris element P5 on the triangle.
      - FEM_HCT_TRIANGLE :
        Hsieh-Clough-Tocher element on the triangle (composite P3 element
        which is C1), should be used with IM_HCT_COMPOSITE() integration
        method.
      - FEM_QUADC1_COMPOSITE :
        Quadrilateral element, composite P3 element and C1 (16 dof).
      - FEM_REDUCED_QUADC1_COMPOSITE :
        Quadrilateral element, composite P3 element and C1 (12 dof).
      - FEM_RT0(n) :
        Raviart-Thomas element of order 0 on a simplex of dimension `n`.
      - FEM_NEDELEC(n) :
        Nedelec edge element of order 0 on a simplex of dimension `n`.

      Of course, you have to ensure that the selected fem is compatible with
      the geometric transformation: a Pk fem has no meaning on a quadrangle.
      @*/
    getfem::pfem pf = getfem::fem_descriptor(cmd);
    gfi_pf = getfemint_pfem::get_from(pf, STATIC_OBJ | CONST_OBJ);
  }
  out.pop().from_object_id(gfi_pf->get_id(), FEM_CLASS_ID);
}
