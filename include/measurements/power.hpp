#ifndef TRIUM_POWER_H_INCLUDED_
#define TRIUM_POWER_H_INCLUDED_

#ifndef TRIUM_FIELD_H_INCLUDED_
#include "field.hpp"
#endif

/**
 * Calculate power spectrum from catalogues.
 *
 * @param particles_data (Reference to) the data-source particle container.
 * @param particles_rand (Reference to) the random-source particle container.
 * @param los_data Data-source particle lines of sight.
 * @param los_rand Random-source particle lines of sight.
 * @param params (Reference to) the input parameter set.
 * @param alpha Alpha ratio.
 * @param kbin Wavenumber bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_power_spec(
  ParticleCatalogue& particles_data, ParticleCatalogue & particles_rand,
  LineOfSight* los_data, LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* kbin,
  double survey_vol_norm
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring power spectrum.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for power spectrum measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopoles of the Fourier--harmonic transform of the density
  /// fluctuation, i.e. δn_00.
  DensityField<ParticleCatalogue> dn_00(params);
  dn_00.calc_ylm_weighted_fluctuation(
    particles_data, particles_rand,
    los_data, los_rand,
    alpha,
    0, 0
  );
  dn_00.calc_fourier_transform();

  /// Compute power spectrum.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    /// Compute Fourier--harmonic transform of the density fluctuation.
    DensityField<ParticleCatalogue> dn_LM(params);
      // NOTE: standard variable naming convention overriden
    dn_LM.calc_ylm_weighted_fluctuation(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );
    dn_LM.calc_fourier_transform();

    /// Compute shot noise.
    TwoPointStatistics<ParticleCatalogue> stats(params);
    std::complex<double> shotnoise = stats.calc_shotnoise_for_power_spec(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );

    /// ???
    for (int m1_ = - params.ell1; m1_ <= params.ell1; m1_++) {
      /// ??? Calculate equivalent to (-1)^m_1 δ_{m_1, -M}.
      double coupling = (2*params.ELL + 1) * (2*params.ell1 + 1)
        * wigner_3j(params.ell1, 0, params.ELL, 0, 0, 0)
        * wigner_3j(params.ell1, 0, params.ELL, m1_, 0, M_);
      if (fabs(coupling) < 1.e-10) {
        continue;
      }

      stats.calc_power_spec(dn_LM, dn_00, kbin, shotnoise, params.ell1, m1_);

      for (int i = 0; i < params.num_kbin; i++) {
        pk_save[i] += coupling * stats.pk[i];
      }
    }

    durationInSec = double(clock() - timeStart);
    if (thisTask == 0) {
      printf(
        "[Status] :: Computed order M = %d "
        "(... %.3f seconds elapsed in total).\n",
        M_, durationInSec / CLOCKS_PER_SEC
      );
    }
  }

  /// Normalise and then save the output.
  double norm = ParticleCatalogue::calc_norm_for_power_spec(
    particles_data, survey_vol_norm
  );

  char buf[1024];
  sprintf(buf, "%s/pk%d", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] pk_save;

  return 0;
}

/**
 * Calculate two-point correlation function.
 *
 * @param particles_data (Reference to) the data-source particle container.
 * @param particles_rand (Reference to) the random-source particle container.
 * @param los_data Data-source particle lines of sight.
 * @param los_rand Random-source particle lines of sight.
 * @param params (Reference to) the input parameter set.
 * @param alpha Alpha ratio.
 * @param rbin Separation bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_corr_func(
  ParticleCatalogue& particles_data, ParticleCatalogue& particles_rand,
  LineOfSight* los_data, LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* rbin,
  double survey_vol_norm
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring two-point correlation function.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point correlation function measurements. "
      );
      printf("Please set `ell1 = ELL` and `ell2 = 0`.\n");
      exit(1);
    }
  }

  /// Compute monopole of the Fourier--harmonic transform of the density
  /// fluctuation.
  DensityField<ParticleCatalogue> dn_00(params);
  dn_00.calc_ylm_weighted_fluctuation(
    particles_data, particles_rand,
    los_data, los_rand,
    alpha,
    0, 0
  );
  dn_00.calc_fourier_transform();

  /// Compute two-point correlation function.
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] = 0.;
  }

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    /// Compute Fourier--harmonic transform of the density fluctuation.
    DensityField<ParticleCatalogue> dn_LM(params);
      // NOTE: standard variable naming convention overriden
    dn_LM.calc_ylm_weighted_fluctuation(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );
    dn_LM.calc_fourier_transform();

    /// Compute shot noise.
    TwoPointStatistics<ParticleCatalogue> stats(params);
    std::complex<double> shotnoise = stats.calc_shotnoise_for_power_spec(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );

    /// ???
    for (int m1_ = - params.ell1; m1_ <= params.ell1; m1_++) {
      /// ??? Calculate equivalent to (-1)^m_1 δ_{m_1, -M}.
      double coupling = (2*params.ELL + 1) * (2*params.ell1 + 1)
        * wigner_3j(params.ell1, 0, params.ELL, 0, 0, 0)
        * wigner_3j(params.ell1, 0, params.ELL, m1_, 0, M_);
      if (fabs(coupling) < 1.e-10) {
        continue;
      }

      stats.calc_corr_func(dn_LM, dn_00, rbin, shotnoise, params.ell1, m1_);

      for (int i = 0; i < params.num_rbin; i++) {
        xi_save[i] += coupling * stats.xi[i];
      }
    }

    durationInSec = double(clock() - timeStart);
    if (thisTask == 0) {
      printf(
        "[Status] :: Computed order M = %d "
        "(... %.3f seconds elapsed in total).\n",
        M_, durationInSec / CLOCKS_PER_SEC
      );
    }
  }

  /// Normalise and then save the output.
  double norm = ParticleCatalogue::calc_norm_for_power_spec(
    particles_data, survey_vol_norm
  );

  char buf[1024];
  sprintf(buf, "%s/xi%d", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_rbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", rbin[i], norm * xi_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] xi_save;

  return 0;
}

/**
 * Calculate power spectrum window function.
 *
 * @param particles_rand (Reference to) the random-source particle container.
 * @param los_rand Random-source particle lines of sight.
 * @param params (Reference to) the input parameter set.
 * @param alpha Alpha ratio.
 * @param kbin Wavenumber bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_power_spec_window(
  ParticleCatalogue& particles_rand,
  LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* kbin,
  double survey_vol_norm
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring power spectrum window function.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point statistics measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopole of the Fourier--harmonic transform of the mean density.
  DensityField<ParticleCatalogue> dn_00(params);
  dn_00.calc_ylm_weighted_mean_density(particles_rand, los_rand, alpha, 0, 0);
  dn_00.calc_fourier_transform();

  /// Initialise output power spectrum window function.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }
  std::cout << "Current memory usage: " << bytes << " bytes." << std::endl;

  /// Compute shot noise.
  TwoPointStatistics<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise = stats.calc_shotnoise_for_corr_func_window(
    particles_rand, los_rand, alpha, params.ELL, 0
  );
  std::cout << "Current memory usage: " << bytes << " bytes." << std::endl;

  /// Compute power spectrum window function.
  stats.calc_power_spec(dn_00, dn_00, kbin, shotnoise, params.ELL, 0);
    // `ell1` or `ELL` as  equivalent

  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] += stats.pk[i];
  }
  std::cout << "Current memory usage: " << bytes << " bytes." << std::endl;

  /// Normalise and then save the output.
  double norm = ParticleCatalogue::calc_norm_for_power_spec(
    particles_rand, survey_vol_norm
  );
  norm /= alpha * alpha;
  norm /= params.volume;  // NOTE: volume normalisation is essential

  char buf[1024];
  sprintf(buf, "%s/pk%d_window", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  if (0) {
  } else if (thisTask == 0) {
    printf(
      "[Info] :: Power spectrum in the lowest wavenumber bin: %.2f.",
      norm * pk_save[0].real()
    );
  }

  delete[] pk_save;

  return 0;
}

/**
 * Calculate two-point correlation window function.
 *
 * @param particles_rand (Reference to) the random-source particle container.
 * @param los_rand Random-source particle lines of sight.
 * @param params (Reference to) the input parameter set.
 * @param alpha Alpha ratio.
 * @param kbin Wavenumber bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_corr_func_window(
  ParticleCatalogue& particles_rand,
  LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* rbin,
  double survey_vol_norm
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring two-point correlation window function.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point statistics measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopole of the Fourier--harmonic transform of the mean density.
  DensityField<ParticleCatalogue> dn_00(params);
  dn_00.calc_ylm_weighted_mean_density(particles_rand, los_rand, alpha, 0, 0);
  dn_00.calc_fourier_transform();

  /// Initialise output two-point correlation function.
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] = 0.;
  }

  /// Compute two-point correlation function.
  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    /// Compute Fourier--harmonic transform of the density fluctuation.
    DensityField<ParticleCatalogue> dn_LM(params);
      // NOTE: standard variable naming convention overriden
    dn_LM.calc_ylm_weighted_mean_density(
      particles_rand, los_rand, alpha, params.ELL, M_
    );
    dn_LM.calc_fourier_transform();

    /// Compute shot noise.
    TwoPointStatistics<ParticleCatalogue> stats(params);
    std::complex<double> shotnoise = stats.calc_shotnoise_for_corr_func_window(
      particles_rand, los_rand, alpha, params.ELL, M_
    );

    /// ???
    for (int m1_ = - params.ell1; m1_ <= params.ell1; m1_++) {
      /// ??? Calculate equivalent to (-1)^m_1 δ_{m_1, -M}.
      double coupling = (2*params.ELL + 1) * (2*params.ell1 + 1)
          * wigner_3j(params.ell1, 0, params.ELL, 0, 0, 0)
          * wigner_3j(params.ell1, 0, params.ELL, m1_, 0, M_);
      if (fabs(coupling) < 1.e-10) {
        continue;
      }

      stats.calc_corr_func(dn_LM, dn_00, rbin, shotnoise, params.ell1, m1_);

      for (int i = 0; i < params.num_rbin; i++) {
        xi_save[i] += coupling * stats.xi[i];
      }
    }

    durationInSec = double(clock() - timeStart);
    if (thisTask == 0) {
      printf(
        "[Status] :: Computed order M = %d "
        "(... %.3f seconds elapsed in total).\n",
        M_, durationInSec / CLOCKS_PER_SEC
      );
    }
  }

  /// Normalise and then save the output.
  double norm = ParticleCatalogue::calc_norm_for_power_spec(
    particles_rand, survey_vol_norm
  );
  norm /= alpha * alpha;

  char buf[1024];
  sprintf(buf, "%s/xi%d_window", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_rbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", rbin[i], norm * xi_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] xi_save;

  return 0;
}

/**
 * Calculate power spectrum in a periodic box.
 *
 * @param particles_data (Reference to) the data-source particle container.
 * @param params (Reference to) the input parameter set.
 * @param kbin Wavenumber bins.
 * @returns Exit status.
 */
int calc_power_spec_in_box(
  ParticleCatalogue& particles_data,
  ParameterSet& params,
  double* kbin
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring power spectrum.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for power spectrum measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Fourier transform the density field.
  DensityField<ParticleCatalogue> dn(params);
  dn.calc_fluctuation_in_box(particles_data, params);
  dn.calc_fourier_transform();

  /// Initialise output power spectrum.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }

  /// Compute shot noise.
  TwoPointStatistics<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise = double(particles_data.nparticles);

  /// Compute power spectrum.
  stats.calc_power_spec(dn, dn, kbin, shotnoise, params.ELL, 0);

  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] += double(2*params.ELL + 1) * stats.pk[i];
  }

  durationInSec = double(clock() - timeStart);
  if (thisTask == 0) {
    printf(
      "[Status] :: Computed power spectrum in a periodic box "
      "(... %.3f seconds elapsed in total).\n",
      durationInSec / CLOCKS_PER_SEC
    );
  }

  /// Normalise and then save the output.
  double norm = params.volume
    / double(particles_data.nparticles) / double(particles_data.nparticles);

  char buf[1024];
  sprintf(buf, "%s/pk%d", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] pk_save;

  return 0;
}

/**
 * Calculate two-point correlation function in a periodic box.
 *
 * @param particles_data (Reference to) the data-source particle container.
 * @param params (Reference to) the input parameter set.
 * @param kbin Wavenumber bins.
 * @returns Exit status.
 */
int calc_corr_func_in_box(
  ParticleCatalogue& particles_data,
  ParameterSet& params,
  double* rbin
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring two-point correlation function.\n");

    if (!(params.ell1 == params.ELL) && (params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point correlation function measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Fourier transform the density field.
  DensityField<ParticleCatalogue> dn(params);
  dn.calc_fluctuation_in_box(particles_data, params);
  dn.calc_fourier_transform();

  /// Initialise output two-point correlation function.
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] = 0.;
  }

  /// Compute shot noise.
  TwoPointStatistics<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise = double(particles_data.nparticles);

  /// Compute two-point correlation function.
  stats.calc_corr_func(dn, dn, rbin, shotnoise, params.ELL, 0);

  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] += double(2 * params.ELL + 1) * stats.xi[i];
      // NOTE: `double` needed here as complex multiplication is defined
      // as a template unlike normal floats
  }

  durationInSec = double(clock() - timeStart);
  if (thisTask == 0) {
    printf(
      "[Status] :: Computed two-point correlation in a periodic box "
      "(... %.3f seconds elapsed in total).\n",
      durationInSec / CLOCKS_PER_SEC
    );
  }

  /// Normalise and then save the output.
  double norm = params.volume
    / double(particles_data.nparticles) / double(particles_data.nparticles);

  char buf[1024];
  sprintf(buf, "%s/xi%d", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_rbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", rbin[i], norm * xi_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] xi_save;

  return 0;
}

/**
 * Calculate power spectrum in a periodic box for reconstruction.
 *
 * @param particles_data (Reference to) the data-source particle container.
 * @param particles_rand (Reference to) the random-source particle container.
 * @param params (Reference to) the input parameter set.
 * @param kbin Wavenumber bins.
 * @returns Exit status.
 */
int calc_power_spec_in_box_for_recon(
  ParticleCatalogue& particles_data,
  ParticleCatalogue& particles_rand,
  ParameterSet& params,
  double alpha,
  double* kbin
) {
  if (thisTask == 0) {
    printf("[Status] :: Measuring power spectrum.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for power spectrum measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Fourier transform the density field.
  DensityField<ParticleCatalogue> dn(params);
  dn.calc_fluctuation_in_box_for_recon(
    particles_data, particles_rand, alpha
  );
  dn.calc_fourier_transform();

  /// Initialise output power spectrum.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }

  /// Compute shot noise.
  TwoPointStatistics<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise =
    stats.calc_shotnoise_for_power_spec_in_box_for_recon(
      particles_data, particles_rand, alpha
    );

  /// Compute power spectrum.
  stats.calc_power_spec(dn, dn, kbin, shotnoise, params.ELL, 0);

  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] += double(2*params.ELL + 1) * stats.pk[i];
      // NOTE: `double` needed here as complex multiplication is defined
      // as a template unlike normal floats
  }

  durationInSec = double(clock() - timeStart);
  if (thisTask == 0) {
    printf(
      "[Status] :: Computed power spectrum in a periodic box "
      "for reconstruction (... %.3f seconds elapsed in total).\n",
      durationInSec / CLOCKS_PER_SEC
    );
  }

  /// Normalise and then save the output.
  double norm = params.volume
    / double(particles_data.nparticles) / double(particles_data.nparticles);

  char buf[1024];
  sprintf(buf, "%s/pk%d", params.output_dir.c_str(), params.ELL);

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] pk_save;

  return 0;
}

#endif
