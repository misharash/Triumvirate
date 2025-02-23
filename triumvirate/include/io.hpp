// Copyright (C) [GPLv3 Licence]
//
// This file is part of the Triumvirate program. See the COPYRIGHT
// and LICENCE files at the top-level directory of this distribution
// for details of copyright and licensing.
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

/**
 * @file io.hpp
 * @authors Mike S Wang (https://github.com/MikeSWang)
 *          Naonori Sugiyama (https://github.com/naonori)
 * @brief I/O support including custom exceptions and utility functions.
 *
 */

#ifndef TRIUMVIRATE_INCLUDE_IO_HPP_INCLUDED_
#define TRIUMVIRATE_INCLUDE_IO_HPP_INCLUDED_

#include <string>

#include "parameters.hpp"
#include "particles.hpp"
#include "dataobjs.hpp"

namespace trv {

/// **********************************************************************
/// Utilities
/// **********************************************************************

namespace sys {

/**
 * @brief Check if a file path is set.
 *
 * @param pathstr File path string.
 * @returns { @c true , @c false }
 */
bool if_filepath_is_set(std::string pathstr);

}  // namespace trv::sys


/// **********************************************************************
/// Outputs
/// **********************************************************************

const char comment_delimiter[] = "#";  ///< header comment delimiter

/// ----------------------------------------------------------------------
/// Pre-measurement header
/// ----------------------------------------------------------------------

/**
 * @brief Print the pre-measurement header to a file including information
 *        about the catalogue(s) and mesh grid assignment.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param catalogue_data (Data-source) particle catalogue.
 * @param catalogue_rand (Random-source) particle catalogue.
 * @param norm_factor Normalisation factor.
 * @param norm_factor_alt Alternative normalisation factor.
 */
void print_measurement_header_to_file(
  std::FILE* fileptr, trv::ParameterSet& params,
  trv::ParticleCatalogue& catalogue_data, trv::ParticleCatalogue& catalogue_rand,
  double norm_factor, double norm_factor_alt
);

/**
 * @brief Print the pre-measurement header to a file including information
 *        about the catalogue(s) and mesh grid assignment.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param catalogue Particle catalogue.
 * @param norm_factor Normalisation factor.
 * @param norm_factor_alt Alternative normalisation factor.
 *
 * @overload
 */
void print_measurement_header_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::ParticleCatalogue& catalogue,
  double norm_factor, double norm_factor_alt
);


/// ----------------------------------------------------------------------
/// Two-point measurement header
/// ----------------------------------------------------------------------

/**
 * @brief Print measurements to a file including the normalisation
 *        factors and data table columns.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param meas_powspec Power spectrum measurements.
 */
void print_measurement_datatab_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::PowspecMeasurements& meas_powspec
);

/**
 * @brief Print measurements to a file including the normalisation
 *        factors and data table columns.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param meas_2pcf Two-point correlation function measurements.
 *
 * @overload
 */
void print_measurement_datatab_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::TwoPCFMeasurements& meas_2pcf
);

/**
 * @brief Print measurements to a file including the normalisation
 *        factors and data table columns.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param meas_2pcf_win Two-point correlation function window measurements.
 *
 * @overload
 */
void print_measurement_datatab_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::TwoPCFWindowMeasurements& meas_2pcf_win
);


/// ----------------------------------------------------------------------
/// Three-point measurement header
/// ----------------------------------------------------------------------

/**
 * @brief Print measurements to a file including the normalisation
 *        factors and data table columns.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param meas_bispec Bispectrum measurements.
 *
 * @overload
 */
void print_measurement_datatab_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::BispecMeasurements& meas_bispec
);

/**
 * @brief Print measurements to a file including the normalisation
 *        factors and data table columns.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param meas_3pcf Three-point correlation function measurements.
 *
 * @overload
 */
void print_measurement_datatab_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::ThreePCFMeasurements& meas_3pcf
);

/**
 * @brief Print measurements to a file including the normalisation
 *        factors and data table columns.
 *
 * @param fileptr File to print to.
 * @param params Parameter set.
 * @param meas_3pcf_win Three-point correlation function window
 *                      measurements.
 *
 * @overload
 */
void print_measurement_datatab_to_file(
  std::FILE* fileptr,
  trv::ParameterSet& params, trv::ThreePCFWindowMeasurements& meas_3pcf_win
);

}  // namespace trv

#endif  // !TRIUMVIRATE_INCLUDE_IO_HPP_INCLUDED_
