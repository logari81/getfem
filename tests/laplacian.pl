$bin_dir = "$ENV{srcdir}/../bin";
#$tmp = `$bin_dir/createmp laplacian.param`;
$tmp=toto;
sub catch { `rm -f $tmp`; exit(1); }
$SIG{INT} = 'catch';

open(TMPF, ">$tmp") or die "Open file $tmp impossible : $!\n";
print TMPF <<
LX = 1.0;		% size in X.
LY = 1.0;	        % size in Y.
LZ = 1.0;		% size in Z.
INCLINE = 0;            % Incline of the mesh.
FT = 0.1;               % parameter for the exact solution.
MESH_TYPE = 'GT_PK(2,1)';         % linear triangles
NX = 30;            	          % space step.
MESH_NOISE = 1; % Set to one if you want to "shake" the mesh
FEM_TYPE = 'FEM_PK(2,1)';  % P1 for triangles
INTEGRATION = 'IM_TRIANGLE(6)'; % quadrature rule for polynomials up
                               % to degree 6 on triangles
RESIDU = 1E-9;     	% residu for conjugate gradient.
GENERIC_DIRICHLET = 0;  % Generic Dirichlet condition for non-lagrangian elts.
ROOTFILENAME = 'laplacian';     % Root of data files.

;
close(TMPF);


$er = 0;

sub start_program # (N, K, NX, OPTION, SOLVER)
{
  my $def   = $_[0];

  # print ("def = $def\n");

  open F, "./laplacian $tmp $def 2>&1 |" or die;
  while (<F>) {
    if ($_ =~ /L2 error/) {
      ($a, $b) = split('=', $_);
      # print "La norme en question :", $b;
      if ($b > 0.01) { print "\nError too large\n"; $er = 1; }
    }
    if ($_ =~ /error has been detected/) {
      $er = 1;
      print "============================================\n";
      print $_, <F>;
    }
 # print $_;
  }
  close(F); if ($?) { `rm -f $tmp`; exit(1); }
}

start_program("");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PK(1,1)\"' -d 'FEM_TYPE=\"FEM_PK(1,2)\"' -d 'INTEGRATION=\"IM_EXACT_SIMPLEX(1)\"' -d NX=10 -d FT=1.0");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PK(3,1)\"' -d 'FEM_TYPE=\"FEM_PK(3,1)\"' -d 'INTEGRATION=\"IM_EXACT_SIMPLEX(3)\"' -d NX=3 -d FT=0.01");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PK(3,1)\"' -d 'FEM_TYPE=\"FEM_PK(3,2)\"' -d 'INTEGRATION=\"IM_TETRAHEDRON(5)\"' -d NX=3 -d FT=0.01");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PK(2,1)\"' -d 'FEM_TYPE=\"FEM_PK(2,2)\"' -d 'INTEGRATION=\"IM_EXACT_SIMPLEX(2)\"' -d NX=5");
print ".";
start_program("-d 'INTEGRATION=\"IM_TRIANGLE(2)\"'");
print ".";
start_program("-d 'INTEGRATION=\"IM_TRIANGLE(19)\"'");
print ".";
start_program("-d 'MESH_TYPE=\"GT_QK(2,1)\"' -d 'FEM_TYPE=\"FEM_QK(2,1)\"' -d 'INTEGRATION=\"IM_NC_PARALLELEPIPED(2,2)\"'");
#start_program("-d INTEGRATION=1  -d MESH_TYPE=1");
print ".";
start_program("-d 'MESH_TYPE=\"GT_QK(2,1)\"' -d 'FEM_TYPE=\"FEM_QK(2,1)\"' -d 'INTEGRATION=\"IM_QUAD(3)\"'");
#start_program("-d INTEGRATION=33 -d MESH_TYPE=1");
print ".";
start_program("-d 'MESH_TYPE=\"GT_QK(2,1)\"' -d 'FEM_TYPE=\"FEM_QK(2,1)\"' -d 'INTEGRATION=\"IM_QUAD(17)\"'");
#start_program("-d INTEGRATION=35 -d MESH_TYPE=1");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PRISM(3,1)\"' -d 'FEM_TYPE=\"FEM_PK_PRISM(3,1)\"' -d 'INTEGRATION=\"IM_NC_PRISM(3,2)\"' -d NX=3 -d FT=0.01");
#start_program("-d N=3 -d INTEGRATION=1 -d MESH_TYPE=2 -d NX=3 -d FT=0.01");
print ".";
start_program("-d 'MESH_TYPE=\"GT_QK(2,1)\"' -d 'FEM_TYPE=\"FEM_QK(2,1)\"' -d 'INTEGRATION=\"IM_GAUSS_PARALLELEPIPED(2,2)\"' -d NX=10 -d INCLINE=0.5");
#start_program("-d INTEGRATION=2 -d MESH_TYPE=1 -d NX=10 -d INCLINE=0.5");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PK(1,1)\"' -d 'FEM_TYPE=\"FEM_PK_HIERARCHICAL(1,4)\"' -d 'DATA_FEM_TYPE=\"FEM_PK(1,1)\"' -d 'INTEGRATION=\"IM_EXACT_SIMPLEX(1)\"' -d FT=1.0");
#start_program("-d N=1 -d FEM_TYPE=2 -d FT=1.0");
print ".";
start_program("-d 'MESH_TYPE=\"GT_PK(3,1)\"' -d 'FEM_TYPE=\"FEM_PK_HIERARCHICAL_COMPOSITE(3,1,2)\"' -d 'DATA_FEM_TYPE=\"FEM_PK(3,1)\"' -d 'INTEGRATION=\"IM_STRUCTURED_COMPOSITE(IM_TETRAHEDRON(2), 4)\"' -d NX=2 -d FT=1.0");
#start_program("-d K=2 -d KI=2 -d N=3 -d NX=1 -d FEM_TYPE=3 -d INTEGRATION=3 -d FT=1.0");
print ".\n";

`rm -f $tmp`;
if ($er == 1) { exit(1); }


