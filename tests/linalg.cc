/* *********************************************************************** */
/*                                                                         */
/* Copyright (C) 2002-2003  Yves Renard.                                   */
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
#include <gmm.h>
#include <gmm_inoutput.h>


using gmm::size_type;

void test_gauss_det() {
  gmm::dense_matrix<double> m(5,5);
  // vander(1:5)
  for (unsigned i=0; i < m.nrows(); ++i)
    for (unsigned j=0; j < m.ncols(); ++j) {
      m(i,j) = 1; for (unsigned k=j; k < m.ncols()-1; ++k) m(i,j) *= (i+1);
    }
  cout << "m=" << m << endl;
  double det = gmm::lu_inverse(m);
  cout << "det = " << det << "\n m=" << m << endl;
}

template <class M1, class M2> 
void const_ref_test(const M1 &m1, const M2 &m2) {

  cout << "/***********************************************************/\n";
  cout << "/*      Const references test.                             */\n";
  cout << "/***********************************************************/\n";

  cout << gmm::transposed(gmm::sub_matrix(m1, gmm::sub_interval(0, 2),
					  gmm::sub_interval(2, 2))) << endl;
  cout << gmm::transposed(gmm::sub_matrix(m2,gmm::sub_interval(2, 3),
 					   gmm::sub_slice(1, 2, 3))) << endl;
}

template <class MAT, class T> void print_for_matlab(const MAT &m, T) { 
  cout.precision(16);
  cout << "[ ";
  for (size_type i = 0; i < gmm::mat_nrows(m); ++i) {
    for (size_type j = 0; j < gmm::mat_ncols(m); ++j) cout << " " << m(i,j);
    if (i != gmm::mat_nrows(m)-1) cout << " ; \n";
  }
  cout << " ]" << endl;
}

template <class MAT, class T> void print_for_matlab(const MAT &m,
						    std::complex<T>) { 
  cout.precision(16);
  cout << "[ ";
  for (size_type i = 0; i < gmm::mat_nrows(m); ++i) {
    for (size_type j = 0; j < gmm::mat_ncols(m); ++j)
      cout << " (" << m(i,j).real() << "+" << m(i,j).imag() << "*i)" ;
    if (i != gmm::mat_nrows(m)-1) cout << " ; \n";
  }
  cout << " ]" << endl;
}

template <class MAT> inline void print_for_matlab(const MAT &m)
{ print_for_matlab(m, gmm::linalg_traits<MAT>::value_type()); }

template <class T> inline T real_or_complex(double a, double b, T)
{ return a; }
template <class T> inline std::complex<T>
real_or_complex(double a, double b, std::complex<T>)
{ return std::complex<T>(a, b); }


template <class MAT>  void test_qr(const MAT &m) {
  typedef typename gmm::linalg_traits<MAT>::value_type value_type;
  typedef typename gmm::number_traits<value_type>::magnitude_type
    magnitude_type;
  double tol = gmm::default_tol(value_type());

  cout   << "/***********************************************************/\n";
  if (gmm::is_complex(value_type()))
    cout << "/*                   Test of QR algorithms (complex)       */\n";
  else
    cout << "/*                   Test of QR algorithms (real)          */\n";
  cout   << "/***********************************************************/\n";
 
  size_type nn = gmm::mat_nrows(m);
  MAT cm(nn,nn), cq(nn,nn), cr(nn,nn), ca(nn,nn);
  std::vector<value_type> cv(nn);
  std::vector<std::complex<magnitude_type> > eigc(nn);

  /* Test on QR factorisation.                                             */
  gmm::fill_random(cm);
  // print_for_matlab(cm);
  gmm::qr_factor(cm, cq, cr);
  gmm::mult(cq, cr, ca);
  gmm::add(gmm::scaled(cm, -1.0), ca);
  cout << "difference on QR factorization : " << gmm::mat_euclidean_norm(ca) << endl;
  if (gmm::mat_euclidean_norm(ca) > sqrt(tol)) 
    DAL_THROW(dal::failure_error, "Error on QR factorisation");

  /* Test on LU.                                                           */
  gmm::fill_random(cm);
  gmm::copy(cm, cq);
  // cout << "cm = " << cm << endl;
  double exectime = dal::uclock_sec();
  gmm::lu_inverse(cq);
  cout << "time to compute LU inverse : "
       << dal::uclock_sec()-exectime << "\n\n";
  // cout << "cm^{-1} = " << cq << endl;
  gmm::mult(cm, cq, ca);
  // cout << "ca = " << ca << endl;
  gmm::copy(gmm::identity_matrix(), cq);
  gmm::add(gmm::scaled(cq, -1.0), ca);
  cout << "difference on LU factorization : " << gmm::mat_euclidean_norm(ca) << endl;
  if (gmm::mat_euclidean_norm(ca) > sqrt(tol))
    DAL_THROW(dal::failure_error, "Error on LU factorisation.");

  exectime = dal::uclock_sec();
  // cout.precision(6);
  // rudimentary_qr_algorithm(cm, eigc, cq);
  // cout << "time to compute rudimentary QR : "<<dal::uclock_sec()-exectime;
  // cout << "\neigenvalues : " << eigc << endl;
  // cout << "eigenvectors : " << cq << endl;
  MAT cm_conj(nn, nn); /* to avoid a Warning in the mult. */
  gmm::copy(gmm::conjugated(cm), cm_conj);

  gmm::mult(cm, cm_conj, cq); gmm::copy(cq, cm);
  // print_for_matlab(cm);
  exectime = dal::uclock_sec();
  symmetric_qr_algorithm(cm, eigc, cq);
  cout << "time to compute symmetric implicit QR : "
       << dal::uclock_sec()-exectime;
  cout.precision(6);
  cout << "\neigenvalues : " << eigc << endl;
  // cout << "eigenvectors : " << cq << endl;
  implicit_qr_algorithm(cm, eigc, cq);
  cout << "\neigenvalues : " << eigc << endl;
  
  gmm::fill_random(cq);
  gmm::copy(cq, cr);
  gmm::lu_inverse(cr);
  gmm::copy(gmm::identity_matrix(), cm);
  std::fill(cv.begin(), cv.end(), 1.0);
  if (nn >  0) cv[ 0] = cm( 0, 0) = real_or_complex(     0.0,  0.0, cv[0]);
  if (nn >  1) cv[ 1] = cm( 1, 1) = real_or_complex(     0.0,  0.0, cv[0]);
  if (nn >  2) cv[ 2] = cm( 2, 2) = real_or_complex(     0.01,-0.1, cv[0]);
  if (nn >  3) cv[ 3] = cm( 3, 3) = real_or_complex(     0.01, 0.1, cv[0]);
  if (nn >  4) cv[ 4] = cm( 4, 4) = real_or_complex(    -2.0,  3.0, cv[0]);
  if (nn >  5) cv[ 5] = cm( 5, 5) = real_or_complex(    -2.0,  3.0, cv[0]);
  if (nn >  6) cv[ 6] = cm( 6, 6) = real_or_complex(   -50.0,  3.0, cv[0]);
  if (nn >  7) cv[ 7] = cm( 7, 7) = real_or_complex(   100.0,  1.0, cv[0]);
  if (nn >  8) cv[ 8] = cm( 8, 8) = real_or_complex(   300.0,  1.0, cv[0]);
  if (nn >  9) cv[ 9] = cm( 9, 9) = real_or_complex(   500.0,  1.0, cv[0]);
  if (nn > 10) cv[10] = cm(10,10) = real_or_complex(  1000.0,  1.0, cv[0]);
  if (nn > 11) cv[11] = cm(11,11) = real_or_complex(  4000.0,  1.0, cv[0]);
  if (nn > 12) cv[12] = cm(12,12) = real_or_complex(  5000.0,  1.0, cv[0]);
  if (nn > 13) cv[13] = cm(13,13) = real_or_complex( 10000.0,  1.0, cv[0]);
  if (nn > 14) cv[14] = cm(14,14) = real_or_complex( 80000.0,  1.0, cv[0]);
  if (nn > 15) cv[15] = cm(15,15) = real_or_complex(100000.0,  1.0, cv[0]);
  gmm::mult(cq, cm, ca); 
  gmm::mult(ca, cr, cm);
  // print_for_matlab(cm);
  cout << "\neigenvalues to be computed : " << cv << endl;
  exectime = dal::uclock_sec();
  implicit_qr_algorithm(cm, eigc, cq);
  cout << "time to compute implicit QR : "
       << dal::uclock_sec()-exectime;
  /* gmm::clean(eigc, 1E-10); */ cout << "\neigenvalues found : " << eigc << endl;
  gmm::clean(cq, 1E-10);
  cout.precision(6);
  // cout << "eigenvectors : " << cq << endl;
  for (size_type l = 0; l < nn; ++l) {
    bool found = false;
     for (size_type k = 0; k < nn; ++k)
       if (dal::abs(eigc[l] - cv[k]) < sqrt(sqrt(tol)) * (dal::abs(eigc[l])+1.0))
	 { cv[k] = -1.123236; found = true; break; }
     if (found == false) {
       cout << "Eigenvalue " << l << " not found\n" << std::flush;
       DAL_THROW(dal::failure_error, "Error on QR algorithm.");
     }
  }

}

int main(void)
{
  dal::exception_callback_debug cb;
  dal::exception_callback::set_exception_callback(&cb);

  try {

    cout.precision(16);

    test_gauss_det();
    
    test_qr(gmm::dense_matrix<double>(100, 100));
    test_qr(gmm::row_matrix<std::vector<std::complex<long double> > >(10, 10));
    test_qr(gmm::col_matrix<std::vector<float> >(10, 10));
    // faire un test avec QD ?
    

    gmm::dense_matrix<double> m(10, 10);
    std::vector<double> y(10), x(10), b(10);
    gmm::copy(gmm::identity_matrix(), m);
    size_type j = 5, k = 12;
    for (size_type i = 0; i < 10; ++i) { 
      m(i, j) = double(k);
      j = (j + 6) % 10; k = (k + 15) % 31; 
      m(i, j) = double(k);
      j = (j + 6) % 10; k = (k + 15) % 31;
      x[i] = 10.0 * double(int(i) - 5 + ((i >= 5) ? 1 : 0));
    }

    gmm::mult(m, x, b);
    cout << "m = " << m << endl;
    cout << "x = " << x << endl;
    cout << "b = " << b << endl;


    cout << "/***********************************************************/\n";
    cout << "/*                   Test of dense_matrix                  */\n";
    cout << "/***********************************************************/\n";
    
    gmm::clear(y);
    gmm::iteration iter(1e-16);
    gmm::bicgstab(m, y, b, gmm::identity_matrix(), iter);
    cout << "y = " << y << endl;
    gmm::add(gmm::scaled(x, double(-1.0)), y);
    cout << "y = " << y << endl;
    double error = gmm::vect_norm2(y);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);
    
    cout << "/***********************************************************/\n";
    cout << "/*                   Test of dense_matrix                  */\n";
    cout << "/***********************************************************/\n";

    gmm::dense_matrix<double> m2(10,10);
    gmm::copy(m, m2);
    cout << "m2 = " << m2 << endl;
    std::vector<double> y2(10), b2(10);
    gmm::copy(b, b2);
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(m2, y2, b2, gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);
    
    cout << "transposed(m2) = " << gmm::transposed(m2) << endl;
    cout << "transposed(transposed(m2)) = "
	 << gmm::transposed(gmm::transposed(m2)) << endl;
    
    gmm::transposed(gmm::transposed(m2))(3, 1) = 5.0;
    if (m2(3, 1) != 5.0) 
      DAL_THROW(dal::failure_error, "write error on matrix m2.");
    gmm::transposed(gmm::transposed(m2))(3, 1) = 0.0;

    cout << "/***********************************************************/\n";
    cout << "/*             Test of row_matrix<wsvector>                */\n";
    cout << "/***********************************************************/\n";

    gmm::csc_matrix<double> cscm(10,10), cscm2;
    gmm::copy(m, cscm);
    gmm::Harwell_Boeing_save("toto.mat", cscm);
    gmm::Harwell_Boeing_load("toto.mat", cscm2);


    gmm::row_matrix<gmm::wsvector<double> > m3(10, 10);
    gmm::copy(m, m3);
    cout << "m3 = " << m3 << endl;
    cout << "transposed(m3) = " << gmm::transposed(m3) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::ilut_precond<gmm::row_matrix<gmm::wsvector<double> > > P(m3,2,1E-3);
    gmm::bicgstab(m3, y2, b, P, iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);
    
    cout << "/***********************************************************/\n";
    cout << "/*         Test of row_matrix<std::vector>                 */\n";
    cout << "/***********************************************************/\n";

    gmm::row_matrix<std::vector<double> > m4(10, 10);
    gmm::copy(m, m4);
    cout << "m4 = " << m4 << endl;
    cout << "transposed(m4) = " << gmm::transposed(m4) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(m4, y2, b, gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);
    
    cout << "/***********************************************************/\n";
    cout << "/*             Test of col_matrix<wsvector>                */\n";
    cout << "/***********************************************************/\n";

    gmm::col_matrix<gmm::wsvector<double> > m5(10, 10);
    gmm::copy(m3, m5);
    cout << "m5 = " << m5 << endl;
    cout << "transposed(m5) = " << gmm::transposed(m5) << endl;
    gmm::clear(y2);
    iter.init();
    // iter.set_noisy(2);
    gmm::gmres(m5, y2, b, P, 5, iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    cout << "/***********************************************************/\n";
    cout << "/*             Test of row_matrix<slvector>                */\n";
    cout << "/***********************************************************/\n";

    gmm::row_matrix<gmm::slvector<double> > m5bis(10, 10);
    gmm::copy(m3, m5bis);
    cout << "m5bis = " << m5bis << endl;
    cout << "transposed(m5bis) = " << gmm::transposed(m5bis) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(m5bis, y2, b, gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);


    cout << "/***********************************************************/\n";
    cout << "/*             Test of csr_matrix<double>                  */\n";
    cout << "/***********************************************************/\n";

    gmm::csr_matrix<double> m5ter(10, 10);
    gmm::copy(m3, m5ter);
    cout << "m5ter = " << m5ter << endl;
    cout << "transposed(m5ter) = " << gmm::transposed(m5ter) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(m5ter, y2, b, gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);


    cout << "/***********************************************************/\n";
    cout << "/*             Test of csc_matrix<double>                  */\n";
    cout << "/***********************************************************/\n";

    gmm::csc_matrix<double, 2> m5a(10, 10);
    gmm::copy(m3, m5a);
    cout << "m5a = " << m5ter << endl;
    cout << "transposed(m5a) = " << gmm::transposed(m5a) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(m5a, y2, b, gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);


    cout << "/***********************************************************/\n";
    cout << "/*         Test of sub_matrices of dense_matrix            */\n";
    cout << "/***********************************************************/\n";

    gmm::sub_interval sint1(10, 10), sint2(15, 10);
    gmm::sub_slice ssli1(10, 10, 3), ssli2(15, 10, 2);
    std::vector<gmm::size_type> ind1(10), ind2(10);
    ind1[0] = 1;  ind1[1] = 3;  ind1[2] = 5;  ind1[3] = 10; ind1[4] = 12; 
    ind1[5] = 15; ind1[6] = 22; ind1[7] = 23; ind1[8] = 24; ind1[9] = 27; 
    ind2[0] = 0;  ind2[1] = 2;  ind2[2] = 5;  ind2[3] = 17; ind2[4] = 24; 
    ind2[5] = 25; ind2[6] = 26; ind2[7] = 30; ind2[8] = 31; ind2[9] = 32; 
    gmm::sub_index sind1(ind1), sind2(ind2);

    gmm::dense_matrix<double> m6(38, 38);
    gmm::clear(m6);
    gmm::copy(m, gmm::sub_matrix(m6, sint1, sint2));
    cout << "m6 = " << m6 << endl;
    cout << "gmm::sub_matrix(m6, sint1, sint2)  = "
//       << gmm::transposed(gmm::transposed
// 			   (gmm::sub_matrix(m6,sint2, sint1))) << endl;
	 << gmm::sub_matrix(m6,sint1, sint2) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m6, sint1, sint2), y2, b,
		  gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m3, gmm::sub_matrix(m6, ssli1, ssli2));
    cout << "m6 = " << m6 << endl;
    cout << "gmm::sub_matrix(m6, ssli1, ssli2)  = "
	 << gmm::sub_matrix(m6, ssli1, ssli2) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m6, ssli1, ssli2), y2, b,
		  gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m3, gmm::sub_matrix(m6, sint1, ssli2));
    cout << "m6 = " << m6 << endl;
    cout << "gmm::sub_matrix(m6, sint1, ssli2)  = "
	 << gmm::sub_matrix(m6, sint1, ssli2) << endl;
    gmm::clear(y2);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m6, sint1, ssli2), y2, b,
		  gmm::identity_matrix(), iter);
    cout << "y2 = " << y2 << endl;
    gmm::add(gmm::scaled(x, -1.0), y2);
    error = gmm::vect_norm2(y2);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m4, gmm::sub_matrix(m6, sind1, sind2));
    gmm::wsvector<double> y3(10);
    cout << "m6 = " << m6 << endl;
    cout << "gmm::sub_matrix(m6, sind1, sind2)  = "
	 << gmm::sub_matrix(m6, sind1, sind2) << endl;
    gmm::clear(y3);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m6, sind1, sind2), y3, b,
		  gmm::identity_matrix(), iter);
    cout << "y3 = " << y3 << endl;
    gmm::add(gmm::scaled(x, -1.0), y3);
    error = gmm::vect_norm2(y3);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    cout << "/***********************************************************/\n";
    cout << "/*      Test of sub_matrices of row_matrix<wsvector>       */\n";
    cout << "/***********************************************************/\n";

    gmm::row_matrix<gmm::wsvector<double> > m7(38, 40);
    gmm::clear(m7);
    gmm::copy(m3, gmm::sub_matrix(m7, sind1, sind2));
    cout << "m7 = " << m7 << endl;
    cout << "gmm::transposed(gmm::sub_matrix(m7, sind1, sind2))  = "
	 << gmm::transposed(gmm::sub_matrix(m7, sind1, sind2)) << endl;
    gmm::clear(y);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m7, sind1, sind2), y, b,
		  gmm::identity_matrix(), iter);
    cout << "y = " << y << endl;
    gmm::add(gmm::scaled(x, -1.0), y);
    error = gmm::vect_norm2(y);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m, gmm::sub_matrix(m7, sint1, ssli2));
    cout << "m7 = " << m7 << endl;
    cout <<
     "gmm::transposed(gmm::transposed(gmm::sub_matrix(m7, sint1, ssli2))  =\n"
	 << gmm::transposed(gmm::transposed(gmm::sub_matrix(m7, sint1,ssli2)))
	 << endl;
    gmm::clear(y3);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m7, sint1, ssli2), y3, b,
		  gmm::identity_matrix(), iter);
    cout << "y3 = " << y3 << endl;
    gmm::add(gmm::scaled(x, -1.0), y3);
    error = gmm::vect_norm2(y3);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    cout << "/***********************************************************/\n";
    cout << "/*      Test of sub_matrices of row_matrix<rsvector>       */\n";
    cout << "/***********************************************************/\n";

    gmm::row_matrix<gmm::rsvector<double> > m8(40, 40);
    gmm::clear(m8);
    gmm::copy(m3, gmm::sub_matrix(m8, sind1, sind2)); //essayer m3 ensuite
    cout << "m8 = " << m8 << endl;
    cout << "gmm::transposed(gmm::sub_matrix(m8, sind1, sind2))  = "
	 << gmm::transposed(gmm::sub_matrix(m8, sind1, sind2)) << endl;
    gmm::clear(y);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m8, sind1, sind2), y, b,
		  gmm::identity_matrix(), iter);
    cout << "y = " << y << endl;
    gmm::add(gmm::scaled(x, -1.0), y);
    error = gmm::vect_norm2(y);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m, gmm::sub_matrix(m8, sint1, ssli2));
    cout << "m8 = " << m8 << endl;
    cout <<
      "gmm::transposed(gmm::transposed(gmm::sub_matrix(m8,sint1,ssli2))  =\n"
	 << gmm::transposed(gmm::transposed(gmm::sub_matrix(m8, sint1, ssli2)))
	 << endl;
    gmm::clear(y3);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m8, sint1, ssli2), y3, b,
		  gmm::identity_matrix(), iter);
    cout << "y3 = " << y3 << endl;
    gmm::add(gmm::scaled(x, -1.0), y3);
    error = gmm::vect_norm2(y3);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : " << error);

    cout << "/***********************************************************/\n";
    cout << "/*      Test of sub_matrices of row_matrix<slvector>       */\n";
    cout << "/***********************************************************/\n";

    gmm::row_matrix<gmm::slvector<double> > m7bis(38, 40);
    gmm::clear(m7bis);
    gmm::copy(m3, gmm::sub_matrix(m7bis, sind1, sind2));
    cout << "m7bis = " << m7bis << endl;
    cout << "gmm::transposed(gmm::sub_matrix(m7bis, sind1, sind2))  = "
	 << gmm::transposed(gmm::sub_matrix(m7bis, sind1, sind2)) << endl;
    gmm::clear(y);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m7bis, sind1, sind2), y, b,
		  gmm::identity_matrix(), iter);
    cout << "y = " << y << endl;
    gmm::add(gmm::scaled(x, -1.0), y);
    error = gmm::vect_norm2(y);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m, gmm::sub_matrix(m7bis, sint1, ssli2));
    cout << "m7bis = " << m7bis << endl;
    cout <<
    "gmm::transposed(gmm::transposed(gmm::sub_matrix(m7bis,sint1,ssli2)) =\n"
    << gmm::transposed(gmm::transposed(gmm::sub_matrix(m7bis, sint1, ssli2)))
    << endl;
    gmm::clear(y3);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m7bis, sint1, ssli2), y3, b,
		  gmm::identity_matrix(), iter);
    cout << "y3 = " << y3 << endl;
    gmm::add(gmm::scaled(x, -1.0), y3);
    error = gmm::vect_norm2(y3);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);


    cout << "/***********************************************************/\n";
    cout << "/*      Test of sub_matrices of col_matrix<slvector>       */\n";
    cout << "/***********************************************************/\n";

    gmm::col_matrix<gmm::slvector<double> > m7ter(38, 40);
    gmm::clear(m7ter);
    gmm::copy(m3, gmm::sub_matrix(m7ter, sind1, sind2));
    cout << "m7ter = " << m7ter << endl;
    cout << "gmm::transposed(gmm::sub_matrix(m7ter, sind1, sind2))  = "
	 << gmm::transposed(gmm::sub_matrix(m7ter, sind1, sind2)) << endl;
    gmm::clear(y);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m7ter, sind1, sind2), y, b,
		  gmm::identity_matrix(), iter);
    cout << "y = " << y << endl;
    gmm::add(gmm::scaled(x, -1.0), y);
    error = gmm::vect_norm2(y);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);

    gmm::copy(m, gmm::sub_matrix(m7ter, sint1, ssli2));
    cout << "m7ter = " << m7ter << endl;
    cout <<
      "gmm::transposed(gmm::transposed(gmm::sub_matrix(m7t,sint1,ssli2)) =\n"
	 << gmm::transposed(gmm::transposed(gmm::sub_matrix(m7ter,
							    sint1, ssli2)))
	 << endl;
    gmm::clear(y3);
    iter.init();
    gmm::bicgstab(gmm::sub_matrix(m7ter, sint1, ssli2), y3, b,
		  gmm::identity_matrix(), iter);
    cout << "y3 = " << y3 << endl;
    gmm::add(gmm::scaled(x, -1.0), y3);
    error = gmm::vect_norm2(y3);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);



    cout << "/***********************************************************/\n";
    cout << "/*      matrix-matrix multiplication                       */\n";
    cout << "/***********************************************************/\n";

    gmm::row_matrix<gmm::rsvector<double> > m9(10, 10);
    gmm::mult(gmm::transposed(gmm::sub_matrix(m8, sint1, ssli2)), 
	      gmm::sub_matrix(m8, sint1, ssli2), m9);
    cout << "m9 = " << m9 << endl;
    gmm::mult(gmm::transposed(gmm::sub_matrix(m8, sint1, ssli2)), b, y);
    iter.init();
    gmm::cg(m9, y3, y, gmm::identity_matrix(), iter);
    cout << "y3 = " << y3 << endl;
    gmm::add(gmm::scaled(x, -1.0), y3);
    error = gmm::vect_norm2(y3);
    cout << "Error : " << error << endl;
    if (error > 1.0E-10)
      DAL_THROW(dal::failure_error, "computation error too large : "<< error);
    

    /***********************************************************/
    /*    Const references test.                               */
    /***********************************************************/

    const_ref_test(m9, m5);


  }
  DAL_STANDARD_CATCH_ERROR;

  return 0;
}
