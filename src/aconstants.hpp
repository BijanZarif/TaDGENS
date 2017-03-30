#ifndef __ACONSTANTS_H

#define __ACONSTANTS_H

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <iomanip>

#define PI 3.14159265358979323846
#define SQRT3 1.73205080756887729353

/// tolerance to check if something is zero, ie, machine epsilon
#define ZERO_TOL 2.2e-16

/// A small number likely smaller than most convergence tolerances
#define SMALL_NUMBER 1e-12

#define NDIM 2
#define NVARS 4

#ifndef MESHDATA_DOUBLE_PRECISION
#define MESHDATA_DOUBLE_PRECISION 20
#endif

namespace acfd
{
	typedef double acfd_real;
	typedef int acfd_int;
}

#ifndef EIGEN_CORE_H
#include <Eigen/Core>

/// Matrix type to be used for dense linear algebra
typedef Eigen::Matrix<acfd::acfd_real, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> Matrix;

/// Vector type to be used for dense linear algebra
typedef Eigen::Matrix<acfd::acfd_real, Eigen::Dynamic, 1> Vector;

#endif


#endif
