//+
SetFactory("OpenCASCADE");
Box(1) = {-0.5, -0.5, 0, 1, 1, 1};
//+
Box(2) = {-0.5, -0.5, 0, 1, 1, 1};
//+
Box(3) = {0.6, -0.5, 0, 1, 1, 1};
//+
Compound Volume(4) = {3};
