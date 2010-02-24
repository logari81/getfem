// -*- c++ -*- (enables emacs c++ mode)
//===========================================================================
//
// Copyright (C) 2006-2010 Julien Pommier.
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
// $Id$
#include <getfemint.h>
#include <getfemint_mesh_levelset.h>
#include <getfemint_levelset.h>
#include <getfemint_workspace.h>

using namespace getfemint;

/*@GFDOC
  General function for modification of @tmesh_levelset objects.
@*/




// Object for the declaration of a new sub-command.

struct sub_gf_lset_set : virtual public dal::static_stored_object {
  int arg_in_min, arg_in_max, arg_out_min, arg_out_max;
  virtual void run(getfemint::mexargs_in& in,
		   getfemint::mexargs_out& out,
		   getfemint_mesh_levelset *gmls,
		   getfem::mesh_level_set &mls) = 0;
};

typedef boost::intrusive_ptr<sub_gf_lset_set> psub_command;

// Function to avoid warning in macro with unused arguments.
template <typename T> static inline void dummy_func(T &) {}

#define sub_command(name, arginmin, arginmax, argoutmin, argoutmax, code) { \
    struct subc : public sub_gf_lset_set {				\
      virtual void run(getfemint::mexargs_in& in,			\
		       getfemint::mexargs_out& out,			\
		       getfemint_mesh_levelset *gmls,			\
		       getfem::mesh_level_set &mls)			\
      { dummy_func(in); dummy_func(out);  dummy_func(gmls); code }	\
    };									\
    psub_command psubc = new subc;					\
    psubc->arg_in_min = arginmin; psubc->arg_in_max = arginmax;		\
    psubc->arg_out_min = argoutmin; psubc->arg_out_max = argoutmax;	\
    subc_tab[cmd_normalize(name)] = psubc;				\
  }                           




void gf_mesh_levelset_set(getfemint::mexargs_in& m_in,
                          getfemint::mexargs_out& m_out) {
  typedef std::map<std::string, psub_command > SUBC_TAB;
  static SUBC_TAB subc_tab;

  if (subc_tab.size() == 0) {

    /*@SET ('add', @tls ls)
    Add a link to the @tls `ls`.

    Only a reference is kept, no copy is done. In order to indicate
    that the linked @tmesh is cut by a @tls one has to call this
    method, where `ls` is an @tls object. An arbitrary number of
    @tls can be added.

    **WARNING**

    The @tmesh of `ls` and the linked @tmesh must be the same.@*/
    sub_command
      ("add", 1, 1, 0, 0,
       getfemint_levelset *gls = in.pop().to_getfemint_levelset();
       if (&mls.linked_mesh() != &gls->levelset().get_mesh_fem().linked_mesh())
	 THROW_BADARG("The meshes of the levelset and the mesh_levelset "
		      "are not the same!");
       mls.add_level_set(gls->levelset());
       workspace().set_dependance(gmls, gls);
       );


    /*@SET ('sup', @tls ls)
    Remove a link to the @tls `ls`.@*/
    sub_command
      ("sup", 1, 1, 0, 0,
       getfemint_levelset *gls = in.pop().to_getfemint_levelset();
       mls.sup_level_set(gls->levelset());
       workspace().sup_dependance(gmls, gls);
       );


    /*@SET ('adapt')
    Do all the work (cut the convexes with the levelsets).

    To initialice the @tmls object or to actualize it when the
    value of any levelset function is modified, one has to call
    this method.@*/
    sub_command
      ("adapt", 0, 0, 0, 0,
       mls.adapt();
       );

  }


  if (m_in.narg() < 2)  THROW_BADARG( "Wrong number of input arguments");
  
  getfemint_mesh_levelset *gmls = m_in.pop().to_getfemint_mesh_levelset(true);
  getfem::mesh_level_set &mls = gmls->mesh_levelset();
 
  std::string init_cmd   = m_in.pop().to_string();
  std::string cmd        = cmd_normalize(init_cmd);
  
  SUBC_TAB::iterator it = subc_tab.find(cmd);
  if (it != subc_tab.end()) {
    check_cmd(cmd, it->first.c_str(), m_in, m_out, it->second->arg_in_min,
	      it->second->arg_in_max, it->second->arg_out_min,
	      it->second->arg_out_max);
    it->second->run(m_in, m_out, gmls, mls);
  }
  else bad_cmd(init_cmd);



}
