#include <iostream>

#include <gsl/gsl_interp.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_spline.h>

#include "measurements/bessel.hpp"

/**
 * Test the evaluation of spherical Bessel functions.
 */
int main(int argc, char* argv[]) {
  /// Read the order and argument from command-line input.
  int ell = atoi(argv[1]);
  double x = atof(argv[2]);

  /// Initialise the interpolated-function class.
  SphericalBesselCalculator spherical_j_ell(ell);

  /// Display the numerical result.
  std::cout <<
    "j_" << ell << "(" << x << ") = " << spherical_j_ell.eval(x)
  << std::endl;

  return 0;
}
