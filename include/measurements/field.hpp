#ifndef TRIUM_FIELD_H_INCLUDED_
#define TRIUM_FIELD_H_INCLUDED_

/**
 * Density field object.
 *
 * This is a template based on the particle container type.
 *
 */
template <class ParticleContainer>
class DensityField {
 public:
  fftw_complex* field;  ///> gridded complex field

  /**
   * Return individual grid value of the field.
   *
   * @param id Grid ID/index.
   * @returns Field value.
   */
  const fftw_complex& operator[](int id) {
    return this->field[id];
  }

  /**
   * Construct density field.
   *
   * @param params (Reference to) the input parameter set.
   */
  DensityField(ParameterSet& params) {
    this->params = params;

    /// Set up a complex field and allocate memory.
    this->field = NULL;
    this->field = fftw_alloc_complex(this->params.nmesh_tot);
    bytes += double(this->params.nmesh_tot) * sizeof(fftw_complex)
      / 1024. / 1024. / 1024.;

    /// Initialise the field with zero values.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] = 0.;
      this->field[i][1] = 0.;
    }
  }

  /**
   * Destruct density field.
   */
  ~DensityField() {
    finalise_density_field();
  }

  /**
   * Finalise density field data.
   */
  void finalise_density_field() {
    /// Free memory usage.
    if (this->field != NULL) {
      fftw_free(this->field); this->field = NULL;
      bytes -= double(this->params.nmesh_tot) * sizeof(fftw_complex)
        / 1024. / 1024. / 1024.;
    }
  }

  /**
   * Assign weighted density field to a grid by interpolation scheme.
   *
   * @param particles Particle container.
   * @param weight Weight field.
   * @returns Exit status.
   */
  int assign_weighted_field_to_grid(
    ParticleContainer& particles,
    fftw_complex* weight
  ) {
    if (0) {
    } else if (this->params.assignment == "NGP") {
      this->assign_weighted_field_to_grid_ngp(particles, weight);
    } else if (this->params.assignment == "CIC") {
      this->assign_weighted_field_to_grid_cic(particles, weight);
    } else if (this->params.assignment == "TSC") {
      this->assign_weighted_field_to_grid_tsc(particles, weight);
    } else {
      return -1;
    }

    return 0;
  }

  /**
   * Assign weighted density field to a grid by the nearest-grid-point
   * scheme.
   *
   * @param particles Particle container.
   * @param weight Particle weight.
   * @returns Exit status.
   */
  int assign_weighted_field_to_grid_ngp(
    ParticleContainer& particles,
    fftw_complex* weight
  ) {
    // Reinitialise the field with zero values.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] = 0.;
      this->field[i][1] = 0.;
    }

    /// Here the over-density is given by Σ_i w_i δ_D(x - x_i),
    /// where δ_D <-> δ_K / dV, dV = volume / nmesh.
    double dV = this->params.volume / double(this->params.nmesh_tot);
      // NOTE: standard variable naming convention overriden
    double cell_vol_factor = 1. / dV;
    int order = 1;
      // order of the interpolation scheme, i.e. number of mesh grid,
      // per dimension, to which a single particle is assigned

    for (int id = 0; id < particles.nparticles; id++) {
      int ijk[order][3];  // coordinates of covered mesh grids
      double win[order][3];  // interpolation window
      for (int axis = 0; axis < 3; axis++) {
        double loc_grid = this->params.nmesh[axis]
          * particles[id].pos[axis] / this->params.boxsize[axis];
        ijk[0][axis] = int(loc_grid + 0.5);
        win[0][axis] = 1.;
          // only `0` element as `order == 1`
      }

      for (int i_order = 0; i_order < order; i_order++) {
        for (int j_order = 0; j_order < order; j_order++) {
          for (int k_order = 0; k_order < order; k_order++) {
            long long coord_flat = (
              ijk[i_order][0] * this->params.nmesh[1] + ijk[j_order][1]
            ) * this->params.nmesh[2] + ijk[k_order][2];

            if (coord_flat >= 0 && coord_flat < this->params.nmesh_tot) {
              this->field[coord_flat][0] += cell_vol_factor
                * weight[id][0]
                * win[i_order][0] * win[j_order][1] * win[k_order][2];
              this->field[coord_flat][1] += cell_vol_factor
                * weight[id][1]
                * win[i_order][0] * win[j_order][1] * win[k_order][2];
            }
          }
        }
      }
    }

    return 0;
  }

  /**
   * Assign weighted density field to a grid by the cloud-in-cell
   * scheme.
   *
   * @param particles Particle container.
   * @param weight Particle weight.
   * @returns Exit status.
   */
  int assign_weighted_field_to_grid_cic(
    ParticleContainer& particles,
    fftw_complex* weight
  ) {
    // Reinitialise the field with zero values.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] = 0.;
      this->field[i][1] = 0.;
    }

    /// Here over-density is given by Σ_i w_i δ_D(x - x_i),
    /// where δ_D <-> δ_K / dV, dV = volume / nmesh.
    double dV = this->params.volume / double(this->params.nmesh_tot);
      // NOTE: standard variable naming convention overriden
    double cell_vol_factor = 1. / dV;
    int order = 2;
      // order of the interpolation scheme, i.e. number of mesh grid,
      // per dimension, to which a single particle is assigned

    for (int id = 0; id < particles.nparticles; id++) {
      int ijk[order][3];  // coordinates of covered mesh grids
      double win[order][3];  // interpolation window
      for (int axis = 0; axis < 3; axis++) {
        double loc_grid = this->params.nmesh[axis]
          * particles[id].pos[axis] / this->params.boxsize[axis];
        ijk[0][axis] = int(loc_grid);
        ijk[1][axis] = int(loc_grid) + 1;

        double s = loc_grid - double(int(loc_grid));
        win[0][axis] = 1. - s;
        win[1][axis] = s;
          // up to element `1` as `order == 2`
      }

      for (int i_order = 0; i_order < order; i_order++) {
        for (int j_order = 0; j_order < order; j_order++) {
          for (int k_order = 0; k_order < order; k_order++) {
            long long coord_flat = (
              ijk[i_order][0] * this->params.nmesh[1] + ijk[j_order][1]
            ) * this->params.nmesh[2] + ijk[k_order][2];

            if (coord_flat >= 0 && coord_flat < this->params.nmesh_tot) {
              this->field[coord_flat][0] += cell_vol_factor
                * weight[id][0]
                * win[i_order][0] * win[j_order][1] * win[k_order][2];
              this->field[coord_flat][1] += cell_vol_factor
                * weight[id][1]
                * win[i_order][0] * win[j_order][1] * win[k_order][2];
            }
          }
        }
      }
    }

    return 0;
  }

  /**
   * Assign weighted density field to a grid by the
   * triangular-shaped-cloud scheme.
   *
   * @param particles Particle container.
   * @param weight Particle weight.
   * @returns Exit status.
   */
  int assign_weighted_field_to_grid_tsc(
    ParticleContainer& particles,
    fftw_complex* weight
  ) {
    // Reinitialise the field with zero values.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] = 0.;
      this->field[i][1] = 0.;
    }

    /// Here over-density is given by Σ_i w_i δ_D(x - x_i),
    /// where δ_D <-> δ_K / dV, dV = volume / nmesh.
    double dV = this->params.volume / double(this->params.nmesh_tot);
      // NOTE: standard variable naming convention overriden
    double cell_vol_factor = 1. / dV;
    int order = 3;
      // order of the interpolation scheme, i.e. number of mesh grid,
      // per dimension, to which a single particle is assigned

    for (int id = 0; id < particles.nparticles; id++) {
      int ijk[order][3];  // coordinates of covered mesh grids
      double win[order][3];  // interpolation window
      for (int axis = 0; axis < 3; axis++) {
        double loc_grid = this->params.nmesh[axis] * particles[id].pos[axis]
          / this->params.boxsize[axis];
        ijk[1][axis] = int(loc_grid + 0.5);
        ijk[0][axis] = int(loc_grid + 0.5) - 1;
        ijk[2][axis] = int(loc_grid + 0.5) + 1;

        double s = loc_grid - double(int(loc_grid + 0.5));
        win[0][axis] = 0.5 * (0.5 - s) * (0.5 - s);
        win[1][axis] = 0.75 - s * s;
        win[2][axis] = 0.5 * (0.5 + s) * (0.5 + s);
          // up to element `2` as `order == 3`
      }

      for (int i_order = 0; i_order < order; i_order++) {
        for (int j_order = 0; j_order < order; j_order++) {
          for (int k_order = 0; k_order < order; k_order++) {
            long long coord_flat = (
              ijk[i_order][0] * this->params.nmesh[1] + ijk[j_order][1]
            ) * this->params.nmesh[2] + ijk[k_order][2];

            if (coord_flat >= 0 && coord_flat < this->params.nmesh_tot) {
              this->field[coord_flat][0] += cell_vol_factor
                * weight[id][0]
                * win[i_order][0] * win[j_order][1] * win[k_order][2];
              this->field[coord_flat][1] += cell_vol_factor
                * weight[id][1]
                * win[i_order][0] * win[j_order][1] * win[k_order][2];
            }
          }
        }
      }
    }

    return 0;
  }

  /**
   * Calculate the reduced spherical harmonic transform of weighted
   * density field fluctuation(s).
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_data Data-source particle lines of sight.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Exit status.
   */
  int calc_ylm_weighted_fluctuation(
    ParticleContainer& particles_data, ParticleContainer& particles_rand,
    LineOfSight* los_data, LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {
    /// Initialise the random field and the weight field.
    DensityField<ParticleCatalogue> density_rand(this->params);
    fftw_complex* weight = NULL;

    /// Calculate the data-source transformed weighted field.
    weight = fftw_alloc_complex(particles_data.nparticles);
    for (int id = 0; id < particles_data.nparticles; id++) {
      double los[3] = {
        los_data[id].pos[0], los_data[id].pos[1], los_data[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      weight[id][0] = ylm.real() * particles_data[id].w;
      weight[id][1] = ylm.imag() * particles_data[id].w;
    }

    this->assign_weighted_field_to_grid(particles_data, weight);

    fftw_free(weight); weight = NULL;

    /// Calculate the random-source transformed weighted field.
    weight = fftw_alloc_complex(particles_rand.nparticles);
    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      weight[id][0] = ylm.real() * particles_rand[id].w;
      weight[id][1] = ylm.imag() * particles_rand[id].w;
    }

    density_rand.assign_weighted_field_to_grid(particles_rand, weight);

    fftw_free(weight); weight = NULL;

    /// Subtract to compute fluctuations, i.e. δn_LM.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] -= alpha * density_rand.field[i][0];
      this->field[i][1] -= alpha * density_rand.field[i][1];
    }

    return 0;
  }

  /**
   * Calculate the reduced spherical harmonic transform of a mean-density
   * field.
   *
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Exit status.
   */
  int calc_ylm_weighted_mean_density(
    ParticleContainer& particles_rand,
    LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {
    /// Initialise the weight field.
    fftw_complex* weight = NULL;

    /// Calculate the transformed weighted mean field.
    weight = fftw_alloc_complex(particles_rand.nparticles);
    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      weight[id][0] = ylm.real() * particles_rand[id].w;
      weight[id][1] = ylm.imag() * particles_rand[id].w;
    }

    this->assign_weighted_field_to_grid(particles_rand, weight);

    fftw_free(weight); weight = NULL;

    /// Apply mean density matching normalisation (i.e. the alpha ratio)
    /// to compute \bar{n}_LM.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] *= alpha;
      this->field[i][1] *= alpha;
    }

    return 0;
  }

  /**
   * Calculate the reduced spherical harmonic transform of weighted
   * density fields for calculating bispectrum shot noise.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_data Data-source particle lines of sight.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Exit status.
   */
  int calc_ylm_weighted_fields_for_bispec_shotnoise(
    ParticleContainer& particles_data, ParticleContainer& particles_rand,
    LineOfSight* los_data, LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {
    /// Initialise the random field and the weight field.
    DensityField<ParticleCatalogue> density_rand(this->params);
    fftw_complex* weight = NULL;

    /// Calculate the data-source transformed weighted field.
    weight = fftw_alloc_complex(particles_data.nparticles);
    for (int id = 0; id < particles_data.nparticles; id++) {
      double los[3] = {
        los_data[id].pos[0], los_data[id].pos[1], los_data[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      ylm = std::conj(ylm);  // NOTE: cojugation is essential

      weight[id][0] = ylm.real() * pow(particles_data[id].w, 2);
      weight[id][1] = ylm.imag() * pow(particles_data[id].w, 2);
    }

    this->assign_weighted_field_to_grid(particles_data, weight);
    fftw_free(weight); weight = NULL;

    /// Calculate the random-source transformed weighted field.
    weight = fftw_alloc_complex(particles_rand.nparticles);
    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      ylm = std::conj(ylm);  // NOTE: cojugation is essential

      weight[id][0] = ylm.real() * pow(particles_rand[id].w, 2);
      weight[id][1] = ylm.imag() * pow(particles_rand[id].w, 2);
    }

    density_rand.assign_weighted_field_to_grid(particles_rand, weight);
    fftw_free(weight); weight = NULL;

    /// Compute total shot noise contribution, i.e. N_LM in eq. (46)
    /// in arXiv:1803.02132.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] += alpha * alpha * density_rand.field[i][0];
      this->field[i][1] += alpha * alpha * density_rand.field[i][1];
    }

    return 0;
  }

  /**
   * Calculate the reduced spherical harmonic transform of a mean-density
   * field for calculating 3-point window function shot noise.
   *
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Exit status.
   */
  int calc_ylm_weighted_mean_density_for_3pt_window_shotnoise(
    ParticleContainer& particles_rand,
    LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {  // ??? find matching equation
    /// Initialise the weight field.
    fftw_complex* weight = NULL;

    /// Calculate the conjugated transformed weighted mean field.
    weight = fftw_alloc_complex(particles_rand.nparticles);
    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      ylm = std::conj(ylm);  // NOTE: cojugation is essential

      weight[id][0] = ylm.real() * pow(particles_rand[id].w, 2);
      weight[id][1] = ylm.imag() * pow(particles_rand[id].w, 2);
    }

    this->assign_weighted_field_to_grid(particles_rand, weight);

    fftw_free(weight); weight = NULL;

    /// ??? Compute ?, i.e. ? in eq. (?)
    /// in arXiv:1803.02132.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] *= alpha * alpha;
      this->field[i][1] *= alpha * alpha;
    }

    return 0;
  }

  /**
   * Calculate density field fluctuation(s) in a periodic box.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param params (Reference to) the input parameter set.
   * @returns Exit status.
   */
  int calc_fluctuation_in_box(
    ParticleContainer& particles_data,
    ParameterSet& params
  ) {
    /// Initialise the unit weight field.
    fftw_complex* weight = NULL;

    weight = fftw_alloc_complex(particles_data.nparticles);
    for (int id = 0; id < particles_data.nparticles; id++) {
      weight[id][0] = 1.;
      weight[id][1] = 0.;
    }

    /// Calculate the interpolated field.
    this->assign_weighted_field_to_grid(particles_data, weight);
    fftw_free(weight); weight = NULL;

    /// Subtract the global mean density to compute fluctuations, i.e. δn.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] -= double(particles_data.nparticles) / params.volume;
      this->field[i][1] -= 0.;
    }

    return 0;
  }

  /**
   * Calculate density field fluctuation(s) in a periodic box
   * for reconstruction.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param particles_rand (Reference to) the random-source particle container.
   * @param alpha Alpha ratio.
   * @returns Exit status.
   */
  int calc_fluctuation_in_box_for_recon(
    ParticleContainer& particles_data,
    ParticleContainer& particles_rand,
    double alpha
  ) {
    /// Initialise the random field and the weight field.
    DensityField<ParticleCatalogue> density_rand(this->params);
    fftw_complex* weight = NULL;

    /// Calculate the interpolated data-source field.
    weight = fftw_alloc_complex(particles_data.nparticles);
    for (int id = 0; id < particles_data.nparticles; id++) {
      weight[id][0] = 1.;
      weight[id][1] = 0.;
    }

    this->assign_weighted_field_to_grid(particles_data, weight);
    fftw_free(weight); weight = NULL;

    /// Calculate the interpolated random-source field.
    weight = fftw_alloc_complex(particles_rand.nparticles);
    for (int id = 0; id < particles_rand.nparticles; id++) {
      weight[id][0] = 1.;
      weight[id][1] = 0.;
    }

    density_rand.assign_weighted_field_to_grid(particles_rand, weight);
    fftw_free(weight); weight = NULL;

    /// Subtract to compute fluctuations, i.e. δn.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] -= alpha * density_rand.field[i][0];
      this->field[i][1] -= alpha * density_rand.field[i][1];
    }

    return 0;
  }

  /**
   * Calculate the density field in a periodic box for bispectrum
   * calculations.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @returns Exit status.
   */
  int calc_density_field_in_box_for_bispec(ParticleContainer& particles_data) {
    /// Initialise the unit weight field.
    fftw_complex* weight = NULL;

    weight = fftw_alloc_complex(particles_data.nparticles);
    for (int id = 0; id < particles_data.nparticles; id++) {
      weight[id][0] = 1.;
      weight[id][1] = 0.;
    }

    /// Calculate the interpolated field.
    this->assign_weighted_field_to_grid(particles_data, weight);
    fftw_free(weight); weight = NULL;

    return 0;
  }

  /**
   * Calculate Fourier transform of the field.
   *
   * @returns Exit status.
   */
  int calc_fourier_transform() {
    /// Apply appropriate volume normalisation for FFT.
    double dV = this->params.volume / double(this->params.nmesh_tot);
      // convert ∫d^3x = dV Σ_i
      // NOTE: standard variable naming convention overriden

    for (int i = 0;  i < this->params.nmesh_tot; i++) {
      this->field[i][0] *= dV;
      this->field[i][1] *= dV;
    }

    /// Perform FFT.
    fftw_plan fft_plan_forward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      this->field, this->field,
      FFTW_FORWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_forward);
    fftw_destroy_plan(fft_plan_forward);

    return 0;
  }

  /**
   * Calculate inverse Fourier transform of the (transformed) field.
   *
   * @returns Exit status.
   */
  int calc_inverse_fourier_transform() {
    /// Apply appropriate volume normalisation for FFT.
    double V = this->params.volume;
      // convert ∫d^3k / (2\pi)^3 = (1/V) Σ_i
      // NOTE: standard variable naming convention overriden

    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] /= V;
      this->field[i][1] /= V;
    }

    /// Perform inverse FFT.
    fftw_plan fft_plan_backward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      this->field, this->field,
      FFTW_BACKWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_backward);
    fftw_destroy_plan(fft_plan_backward);

    /// This is primarily used to compute G_LM in eq. (42) in arXiv:1803.02132.

    return 0;
  }

  /**
   * Calculate inverse Fourier transform of the (transformed) (density
   * fluctuation) field for bispectrum calculations.
   *
   * @param density FFT-transformed density (fluctuation) field.
   * @param kmag_in Wavenumber bin wavenumber.
   * @param dk_in Wavenumber bin width.
   * @param ylm Reduced spherical harmonic on a grid.
   * @returns Exit status.
   */
  int calc_inverse_fourier_transform_for_bispec(
    DensityField& density,
    double kmag_in, double dk_in,
    std::complex<double>* ylm
  ) {
    /// Initialise the field with zero values.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] = 0.;
      this->field[i][1] = 0.;
    }

    /// Set up mode sampling.
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    double kvec[3];
    int nmode = 0;
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          /// Calculate the wave-vector/-number representing the mesh grid.
          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] : (k - this->params.nmesh[2]) * dk[2];
          double kmag = sqrt(
            kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]
          );

          /// Determine grid contribution to the specified wavenumber bin,
          /// i.e. eq. (42) in arXiv:1803.02132.
          double k_lower = (kmag_in > dk_in/2) ? (kmag_in - dk_in/2) : 0.;
          double k_upper = kmag_in + dk_in/2;
            // ??? factor these two lines above outside the for-loop
          if (kmag > k_lower && kmag <= k_upper) {
            std::complex<double> den(
              density[coord_flat][0], density[coord_flat][1]
            );

            double win = this->calc_interpolation_window_in_fourier(kvec);
            den /= win;  // apply interpolation-window compensation

            this->field[coord_flat][0] = (ylm[coord_flat] * den).real();
            this->field[coord_flat][1] = (ylm[coord_flat] * den).imag();
            nmode++;
          } else {
            this->field[coord_flat][0] = 0.;
            this->field[coord_flat][1] = 0.;
          }
        }
      }
    }

    fftw_plan fft_plan_backward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      this->field, this->field,
      FFTW_BACKWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_backward);
    fftw_destroy_plan(fft_plan_backward);

    /// Apply the 4π equivalent factor in eq. (42) in arXiv:1803.02132
    /// by mode averaging to compute F_LM.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] /= double(nmode);
      this->field[i][1] /= double(nmode);
    }

    return 0;
  }

  /**
   * Calculate the inverse Fourier transform of a transformed field
   * for three-point correlation functions.
   *
   * @param density FFT-transformed density field.
   * @param rmag_in Separation magnitude.
   * @param ylm Reduced spherical harmonic on a grid.
   * @param bessel_j Spherical Bessel function interpolator.
   * @returns Exit status.
   */
  int calc_inverse_fourier_transform_for_3pt_corr_func(
    DensityField& density,
    double rmag_in,
    std::complex<double>* ylm,
    SphericalBesselCalculator& bessel_j
  ) {
    /// Initialise the field with zero values.
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      this->field[i][0] = 0.;
      this->field[i][1] = 0.;
    }

    /// Set up mode sampling.
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    double kvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          /// Calculate the wave-vector/-number representing the mesh grid.
          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] : (k - this->params.nmesh[2]) * dk[2];
          double kmag = sqrt(
            kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]
          );

          /// Apply interpolation-window compensation.
          std::complex<double> den(
            density[coord_flat][0], density[coord_flat][1]
          );
          double win = this->calc_interpolation_window_in_fourier(kvec);
          den /= win;

          /// Weight with spherical Bessel functions to compute F_LM
          /// in eq. (49) in arXiv:1803.02132.
          this->field[coord_flat][0] =
            bessel_j.eval(kmag * rmag_in) * (ylm[i] * den).real()
            / this->params.volume;
          this->field[coord_flat][1] =
            bessel_j.eval(kmag * rmag_in) * (ylm[i] * den).imag()
            / this->params.volume;
        }
      }
    }

    fftw_plan fft_plan_backward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      this->field, this->field,
      FFTW_BACKWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_backward);
    fftw_destroy_plan(fft_plan_backward);

    return 0;
  }

  /**
   * Calculate the interpolarion window in Fourier space for
   * assignment schemes.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier(double* kvec) {
    if (0) {
    } else if (this->params.assignment == "NGP") {
      return this->calc_interpolation_window_in_fourier_ngp(kvec);
    } else if (this->params.assignment == "CIC") {
      return this->calc_interpolation_window_in_fourier_cic(kvec);
    } else if (this->params.assignment == "TSC") {
      return this->calc_interpolation_window_in_fourier_tsc(kvec);
    } else {
      return 1.;
    }

    return 1.;
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * the nearest-grid-point assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier_ngp(const double* kvec) {
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double wx = (i != 0) ? sin(xk) / xk : 1.;
    double wy = (j != 0) ? sin(yk) / yk : 1.;
    double wz = (k != 0) ? sin(zk) / zk : 1.;
      // sin(x) / x -> 1 as x -> 0
    double wk = wx * wy * wz;

    return pow(wk, 1);
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * the cloud-in-cell assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier_cic(const double* kvec) {
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double wx = (i != 0) ? sin(xk) / xk : 1.;
    double wy = (j != 0) ? sin(yk) / yk : 1.;
    double wz = (k != 0) ? sin(zk) / zk : 1.;
      // sin(x) / x -> 1 as x -> 0
    double wk = wx * wy * wz;

    return pow(wk, 2);
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * the triangular-shaped-cloud assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier_tsc(const double* kvec) {
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double wx = (i != 0) ? sin(xk) / xk : 1.;
    double wy = (j != 0) ? sin(yk) / yk : 1.;
    double wz = (k != 0) ? sin(zk) / zk : 1.;
      // sin(x) / x -> 1 as x -> 0
    double wk = wx * wy * wz;

    return pow(wk, 3);
  }

  /**
   * Calculate compensation needed in Fourier transform for
   * assignment schemes.
   *
   * @returns Exit status.
   */
  int apply_assignment_compensation() {
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    double kvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] : (k - this->params.nmesh[2]) * dk[2];

          double win = this->calc_interpolation_window_in_fourier(kvec);

          this->field[coord_flat][0] /= win;
          this->field[coord_flat][1] /= win;
        }
      }
    }

    return 0;
  }

  /**
   * Calculate survey volume normalisation.
   *
   * @param particles_rand (Reference to) the random-source particle container.
   * @returns survey_volume_norm Survey volume normalisation.
   */
  double calc_survey_volume_norm(ParticleContainer& particles_rand) {
    /// Initialise the unit weight field.
    fftw_complex* weight = NULL;

    weight = fftw_alloc_complex(particles_rand.nparticles);
    for (int id = 0; id < particles_rand.nparticles; id++) {
      weight[id][0] = 1.;
      weight[id][1] = 0.;
    }

    /// Calculate the interpolated field.
    this->assign_weighted_field_to_grid(particles_rand, weight);
    fftw_free(weight); weight = NULL;

    /// Perform normalisation integral, I_2 = ∫d^3x \bar{n}(x)^2.
    double dV = this->params.volume / double(this->params.nmesh_tot);
      // convert ∫d^3x = dV Σ_i
      // NOTE: standard variable naming convention overriden
    double norm = 0.;
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      norm += this->field[i][0] * this->field[i][0] * dV;
    }

    double survey_volume_norm =
      double(particles_rand.nparticles) * double(particles_rand.nparticles)
      / norm;  // NOTE: `double` needed to prevent int overflow

    return survey_volume_norm;
  }

 private:
  ParameterSet params;
};

template <class ParticleContainer>
class TwoPointStatistics {
 public:
  std::complex<double>* pk;  ///< power spectrum statistics
  std::complex<double>* xi;  ///< two-point correlation function statistics
  int* nmode_pk;  ///< number of wavevector modes
  int* npair_xi;  ///< number of separation pairs

  /**
   * Construct two-point statistics.
   *
   * @param params (Reference to) the input parameter set.
   */
  TwoPointStatistics(ParameterSet& params){
    this->params = params;

    /// Set up binned power spectrum and mode counter.
    this->pk = new std::complex<double>[params.num_kbin];
    this->nmode_pk = new int[params.num_kbin];
    for (int i = 0; i < params.num_kbin; i++) {
      this->pk[i] = 0.;
      this->nmode_pk[i] = 0;
    }

    /// Set up binned two-point correlation function and pair counter.
    this->xi = new std::complex<double>[params.num_rbin];
    this->npair_xi = new int[params.num_rbin];
    for (int i = 0; i < params.num_rbin; i++) {
      this->xi[i] = 0.;
      this->npair_xi[i] = 0;
    }
  }

  /**
   * Destruct two-point statistics.
   */
  ~TwoPointStatistics(){
    finalise_2pt_stats();
  }

  /**
   * Finalise two-point statistics.
   */
  void finalise_2pt_stats() {
    /// Free memory usage.
    delete[] this->pk; this->pk = NULL;
    delete[] this->nmode_pk; this->nmode_pk = NULL;
    delete[] this->xi; this->xi = NULL;
    delete[] this->npair_xi; this->npair_xi = NULL;
  }

  /**
   * Calculate the power spectrum.
   *
   * @param density_a First density field.
   * @param density_b Second density field.
   * @param kbin Wavenumber bins.
   * @param shotnoise Power spctrum shot noise level.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Exit status.
   */
  int calc_power_spec(
    DensityField<ParticleContainer> & density_a,
    DensityField<ParticleContainer> & density_b,
    double* kbin,
    std::complex<double> shotnoise,
    int ell, int m
  ) {
    /// Set up fine-sampling of wavenumbers.
    double dk_sample = 0.0001;  // NOTE: discretionary
    int n_sample = 100000;  // NOTE: discretionary

    std::complex<double>* pk_sample = new std::complex<double>[n_sample];
    int* nmode_sample = new int[n_sample];
    for (int i = 0; i < n_sample; i++) {
      pk_sample[i] = 0.;
      nmode_sample[i] = 0;
    }

    /// Set up binned power spectrum measurements.
    for (int i = 0; i < this->params.num_kbin; i++) {
      this->pk[i] = 0.;
      this->nmode_pk[i] = 0;
    }

    /// ??? NOTE: Convert δ δ* = (2\pi)^3 δ_D(k_1 + k_2) P(k), where
    /// (2π)^3 δ_D(k_1 + k_2) <-> V, thus
    /// P(k) = (V/N^2) |δ(k)|^2.

    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    /// Perform power spectrum fine sampling.
    double kvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] :  (k - this->params.nmesh[2]) * dk[2];
          double kmag = sqrt(
            kvec[0] * kvec[0] + kvec[1] * kvec[1] + kvec[2] * kvec[2]
          );

          int idx_k = int(kmag / dk_sample + 0.5);
          if (idx_k < n_sample) {
            std::complex<double> delta_a(
              density_a[coord_flat][0], density_a[coord_flat][1]
            );
            std::complex<double> delta_b(
              density_b[coord_flat][0], density_b[coord_flat][1]
            );
            std::complex<double> mode_power = delta_a * conj(delta_b);

            mode_power -= shotnoise * calc_shotnoise_func(kvec);
              // subtract shot noise

            double win = calc_interpolation_window_in_fourier(kvec);
            mode_power /= pow(win, 2);
              // apply interpolation-window compensation

            std::complex<double> ylm =
              ToolCollection::calc_reduced_spherical_harmonic(ell, m, kvec);
            mode_power *= ylm;  // weight by spherical harmonics

            pk_sample[idx_k] += mode_power;
            nmode_sample[idx_k]++;
          }
        }
      }
    }

    /// Perform power spectrum binning.
    double dkbin = kbin[1] - kbin[0];
    for (int j = 0; j < this->params.num_kbin; j++) {
      double k_lower = (kbin[j] > dkbin/2) ? (kbin[j] - dkbin/2) : 0.;
      double k_upper = kbin[j] + dkbin/2;
      for (int i = 0; i < n_sample; i++) {
        double k_sample = double(i) * dk_sample;
        if (k_sample > k_lower && k_sample <= k_upper) {
          this->pk[j] += pk_sample[i];
          this->nmode_pk[j] += nmode_sample[i];
        }
      }
    }

    for (int i = 0; i < this->params.num_kbin; i++) {
      if (this->nmode_pk[i] != 0) {
        this->pk[i] /= double(this->nmode_pk[i]);
      } else {
        this->pk[i] = 0.;
      }
    }  // average over equivalent modes

    delete[] pk_sample;
    delete[] nmode_sample;

    return 0;
  }

  /**
   * Calculate the two-point correlation function.
   *
   * @param density_a First density field.
   * @param density_b Second density field.
   * @param rbin Separation bins.
   * @param shotnoise Power spctrum shot noise level.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Exit status.
   */
  int calc_corr_func(
    DensityField<ParticleContainer>& density_a,
    DensityField<ParticleContainer>& density_b,
    double* rbin,
    std::complex<double> shotnoise,
    int ell, int m
  ) {
    /// Set up 3-d power spectrum sampling for inverse Fourier transform.
    fftw_complex* twopt3d_sample = fftw_alloc_complex(this->params.nmesh_tot);
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      twopt3d_sample[i][0] = 0.;
      twopt3d_sample[i][1] = 0.;
    }

    double vol_factor = 1. / this->params.volume;  // convert ∫d^3k = (1/V) Σ_i

    /// Compute gridded power spectrum.  See also `calc_power_spec` above.
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    /// ??? NOTE: Convert δ δ* = (2π)^3 δ_D(k_1 + k_2) P(k), where
    /// (2π)^3 δ_D(k_1 + k_2) <-> V, thus
    /// P(k) = (V/N^2) |δ(k)|^2.

    double kvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] : (k - this->params.nmesh[2]) * dk[2];

          std::complex<double> delta_a(
            density_a[coord_flat][0], density_a[coord_flat][1]
          );
          std::complex<double> delta_b(
            density_b[coord_flat][0], density_b[coord_flat][1]
          );
          std::complex<double> mode_power = delta_a * conj(delta_b);

          mode_power -= shotnoise * calc_shotnoise_func(kvec);

          double win = calc_interpolation_window_in_fourier(kvec);
          mode_power /= pow(win, 2);  // ??? find matching equation

          twopt3d_sample[coord_flat][0] = vol_factor * mode_power.real();
          twopt3d_sample[coord_flat][1] = vol_factor * mode_power.imag();
        }
      }
    }

    /// Inverse Fourier transform.
    fftw_plan fft_plan_backward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      twopt3d_sample, twopt3d_sample,
      FFTW_BACKWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_backward);
    fftw_destroy_plan(fft_plan_backward);

    /// Set up fine-sampling of separations.
    double dr_sample = 0.5;  // NOTE: discretionary
    int n_sample = 10000;  // NOTE: discretionary

    std::complex<double>* xi_sample = new std::complex<double>[n_sample];
    int* npair_sample = new int[n_sample];
    for (int i = 0; i < n_sample; i++) {
      xi_sample[i] = 0.;
      npair_sample[i] = 0;
    }

    /// Set up binned two-point correlation function measurements.
    for (int i = 0; i < this->params.num_rbin; i++) {
      this->xi[i] = 0.;
      this->npair_xi[i] = 0;
    }

    double dr[3];
    dr[0] = this->params.boxsize[0] / double(this->params.nmesh[0]);
    dr[1] = this->params.boxsize[1] / double(this->params.nmesh[1]);
    dr[2] = this->params.boxsize[2] / double(this->params.nmesh[2]);

    /// Perform two-point correlation function fine sampling.
    double rvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          rvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dr[0] : (i - this->params.nmesh[0]) * dr[0];
          rvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dr[1] : (j - this->params.nmesh[1]) * dr[1];
          rvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dr[2] : (k - this->params.nmesh[2]) * dr[2];
          double rmag = sqrt(
            rvec[0] * rvec[0] + rvec[1] * rvec[1] + rvec[2] * rvec[2]
          );

          int idx_r = int(rmag / dr_sample + 0.5);
          if (idx_r < n_sample) {
            std::complex<double> pair_corr(
              twopt3d_sample[coord_flat][0], twopt3d_sample[coord_flat][1]
            );  // pair correlation contribution

            std::complex<double> ylm =
              ToolCollection::calc_reduced_spherical_harmonic(ell, m, rvec);
            pair_corr *= ylm;  // weight by spherical harmonics

            xi_sample[idx_r] += pair_corr;
            npair_sample[idx_r]++;
          }
        }
      }
    }

    /// Perform two-point correlation function binning.
    double drbin = rbin[1] - rbin[0];  // regular binning
    for (int j = 0; j < this->params.num_rbin; j++) {
      double r_lower = (rbin[j] > drbin/2) ? (rbin[j] - drbin/2) : 0.;
      double r_upper = rbin[j] + drbin/2;
      for (int i = 0; i < n_sample; i++) {
        double rmag_sample = i * dr_sample;
        if (rmag_sample > r_lower && rmag_sample <= r_upper) {
          this->xi[j] += xi_sample[i];
          this->npair_xi[j] += npair_sample[i];
        }
      }
    }

    for (int i = 0; i < this->params.num_rbin; i++) {
      if (this->npair_xi[i] != 0) {
        this->xi[i] /= double(this->npair_xi[i]);
      } else {
        this->xi[i] = 0.;
      }
    }

    delete[] twopt3d_sample;
    delete[] xi_sample;
    delete[] npair_sample;

    return 0;
  }

  /**
   * Calculate the two-point correlation function for the three-point
   * correlation function.
   *
   * @param density_a First density field.
   * @param density_b Second density field.
   * @param rbin Separation bins.
   * @param shotnoise Power shot noise level.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @param ylm_a Reduced spherical harmonic on a grid for the first density field.
   * @param ylm_b Reduced spherical harmonic on a grid for the second density field.
   * @returns Exit status.
   */
  int calc_corr_func_for_3pt_corr_func(
    DensityField<ParticleContainer>& density_a,
    DensityField<ParticleContainer>& density_b,
    double* rbin,
    std::complex<double> shotnoise,
    int ell, int m,
    std::complex<double>* ylm_a, std::complex<double>* ylm_b
  ) {
    /// Set up 3-d power spectrum sampling for inverse Fourier transform.
    fftw_complex* twopt3d_sample = fftw_alloc_complex(this->params.nmesh_tot);
    for (int i = 0; i < this->params.nmesh_tot; i++) {
      twopt3d_sample[i][0] = 0.;
      twopt3d_sample[i][1] = 0.;
    }

    double vol_factor = 1. / this->params.volume;  // convert ∫d^3k = (1/V) Σ_i

    /// Compute gridded power spectrum.  See also `calc_power_spec` above.
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    /// ??? NOTE: Convert δ δ* = (2π)^3 δ_D(k_1 + k_2) P(k), where
    /// (2π)^3 δ_D(k_1 + k_2) <-> V, thus
    /// P(k) = (V/N^2) |δ(k)|^2.

    double kvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] : (k - this->params.nmesh[2]) * dk[2];

          std::complex<double> delta_a(
            density_a[coord_flat][0], density_a[coord_flat][1]
          );
          std::complex<double> delta_b(
            density_b[coord_flat][0], density_b[coord_flat][1]
          );
          std::complex<double> mode_power = delta_a * conj(delta_b);

          mode_power -= shotnoise * calc_shotnoise_func(kvec);

          double win = calc_interpolation_window_in_fourier(kvec);
          mode_power /= pow(win, 2);

          twopt3d_sample[coord_flat][0] = vol_factor * mode_power.real();
          twopt3d_sample[coord_flat][1] = vol_factor * mode_power.imag();
        }
      }
    }

    /// Inverse Fourier transform.
    fftw_plan fft_plan_backward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      twopt3d_sample, twopt3d_sample,
      FFTW_BACKWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_backward);
    fftw_destroy_plan(fft_plan_backward);

    /// Set up fine-sampling of separations.
    double dr_sample = 0.5;  // NOTE: discretionary
    int n_sample = 10000;  // NOTE: discretionary

    std::complex<double>* xi_sample = new std::complex<double>[n_sample];
    int* npair_sample = new int[n_sample];
    for (int i = 0; i < n_sample; i++) {
      xi_sample[i] = 0.;
      npair_sample[i] = 0;
    }

    for (int i = 0; i < this->params.num_rbin; i++) {
      this->xi[i] = 0.;
      this->npair_xi[i] = 0;
    }

    double dr[3];
    dr[0] = this->params.boxsize[0] / double(this->params.nmesh[0]);
    dr[1] = this->params.boxsize[1] / double(this->params.nmesh[1]);
    dr[2] = this->params.boxsize[2] / double(this->params.nmesh[2]);

    /// Perform two-point correlation function fine sampling.
    double rvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          rvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dr[0] : (i - this->params.nmesh[0]) * dr[0];
          rvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dr[1] : (j - this->params.nmesh[1]) * dr[1];
          rvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dr[2] : (k - this->params.nmesh[2]) * dr[2];
          double rmag = sqrt(
            rvec[0] * rvec[0] + rvec[1] * rvec[1] + rvec[2] * rvec[2]
          );

          int idx_r = int(rmag / dr_sample + 0.5);
          if (idx_r < n_sample) {
            std::complex<double> pair_corr(
              twopt3d_sample[coord_flat][0], twopt3d_sample[coord_flat][1]
            );  // pair correlation contribution

            pair_corr *= ylm_a[coord_flat] * ylm_b[coord_flat];
              // ??? find matching equation: weight by double spherical harmonics

            xi_sample[idx_r] += pair_corr;
            npair_sample[idx_r]++;
          }
        }
      }
    }

    /// Perform two-point correlation function binning.
    double drbin[this->params.num_rbin - 1];
    for (int j = 0; j < this->params.num_rbin - 1; j++) {
      drbin[j] = rbin[j + 1] - rbin[j];
    }  // irregular binning

    for (int j = 0; j < this->params.num_rbin; j++) {
      double r_lower = 0.;
      if (j == 0) {
        r_lower = (rbin[j] > drbin[j]/2) ? (rbin[j] - drbin[j]/2) : 0.;
      } else {
        r_lower = rbin[j] - drbin[j - 1]/2;
      }

      double r_upper = 0.;
      if (j == this->params.num_rbin - 1) {
        r_upper = (rbin[j] + drbin[j - 1]/2);
      } else {
        r_upper = (rbin[j] + drbin[j]/2);
      }

      for (int i = 0; i < n_sample; i++) {
        double rmag_sample = i * dr_sample;
        if (rmag_sample > r_lower && rmag_sample <= r_upper) {
          this->xi[j] += xi_sample[i];
          this->npair_xi[j] += npair_sample[i];
        }
      }
    }

    double dV = this->params.volume / double(this->params.nmesh_tot);
      // NOTE: standard variable naming convention overriden
    for (int i = 0; i < this->params.num_rbin; i++) {
      if (this->npair_xi[i] != 0) {
        this->xi[i] *=
          pow(-1., this->params.ell1 + this->params.ell2) / dV
          / double(this->npair_xi[i]) / double(this->npair_xi[i]);
          // ??? find matching equation
      } else {
        this->xi[i] = 0.;
      }
    }

    delete[] twopt3d_sample;
    delete[] xi_sample;
    delete[] npair_sample;

    return 0;
  }

  /**
   * Calculate shot noise for power spectrum.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_data Data-source particle lines of sight.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Shot noise for power spectrum.
   */
  std::complex<double> calc_shotnoise_for_power_spec(
    ParticleContainer& particles_data,
    ParticleContainer& particles_rand,
    LineOfSight* los_data,
    LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {
    /// Initialise shot noise contributions.
    std::complex<double> sum_data = 0.;
    std::complex<double> sum_rand = 0.;

    /// Perform direct summation with spherical harmonic weighting.
    for (int id = 0; id < particles_data.nparticles; id++) {
      double los[3] = {
        los_data[id].pos[0], los_data[id].pos[1], los_data[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      sum_data += pow(particles_data[id].w, 2) * ylm;
        // ??? find matching equation: single ylm only
    }

    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      sum_rand += pow(particles_rand[id].w, 2) * ylm;
        // ??? find matching equation: single ylm only
    }

    return sum_data + pow(alpha, 2) * sum_rand;
  }

  /**
   * Calculate shot noise for power spectrum in a periodic box
   * for reconstruction.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param particles_rand (Reference to) the random-source particle container.
   * @param alpha Alpha ratio.
   * @returns Shot noise for power spectrum.
   */
  std::complex<double> calc_shotnoise_for_power_spec_in_box_for_recon(
    ParticleContainer& particles_data,
    ParticleContainer& particles_rand,
    double alpha
  ) {
    std::complex<double> sum_data = double(particles_data.nparticles);
    std::complex<double> sum_rand = double(particles_rand.nparticles);

    return sum_data + pow(alpha, 2) * sum_rand;
  }

  /**
   * Calculate shot noise for two-point correlation function window.
   *
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Shot noise for two-point correlation function window.
   */
  std::complex<double> calc_shotnoise_for_corr_func_window(
    ParticleContainer& particles_rand,
    LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {
    std::complex<double> sum_rand = 0.;
    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      sum_rand += pow(particles_rand[id].w, 2) * ylm;
        // ??? find matching equation
    }

    return pow(alpha, 2) * sum_rand;
  }

  /**
   * Calculate shot noise for bispectrum from pure self-contributions.
   *
   * @param particles_data (Reference to) the data-source particle container.
   * @param particles_rand (Reference to) the random-source particle container.
   * @param los_data Data-source particle lines of sight.
   * @param los_rand Random-source particle lines of sight.
   * @param alpha Alpha ratio.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @returns Shot noise for bispectrum.
   */
  std::complex<double> calc_shotnoise_for_bispec_from_self(
    ParticleContainer& particles_data, ParticleContainer& particles_rand,
    LineOfSight* los_data, LineOfSight* los_rand,
    double alpha,
    int ell, int m
  ) {
    /// Initialise shot noise contributions.
    std::complex<double> sum_data = 0.;
    std::complex<double> sum_rand = 0.;

    /// Perform direct summation with spherical harmonic weighting.
    for (int id = 0; id < particles_data.nparticles; id++) {
      double los[3] = {
        los_data[id].pos[0], los_data[id].pos[1], los_data[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      sum_data += pow(particles_data[id].w, 3) * ylm;
    }

    for (int id = 0; id < particles_rand.nparticles; id++) {
      double los[3] = {
        los_rand[id].pos[0], los_rand[id].pos[1], los_rand[id].pos[2]
      };

      std::complex<double> ylm =
        ToolCollection::calc_reduced_spherical_harmonic(ell, m, los);

      sum_rand += pow(particles_rand[id].w, 3) * ylm;
    }

    /// Calculate \bar{S}_LM in eq. (46) in arXiv:1803.02132.
    return sum_data - pow(alpha, 3) * sum_rand;
  }

  /**
   * Calculate shot noise for bispectrum for each mesh grid.
   *
   * @param density_a First density field.
   * @param density_b Second density field.
   * @param shotnoise Shot noise.
   * @param ell Degree of the spherical harmonic.
   * @param m Order of the spherical harmonic.
   * @param xi Two-point correlation function.
   * @returns Exit status.
   */
  int calc_shotnoise_for_bispec_ijk(
    DensityField<ParticleContainer>& density_a,
    DensityField<ParticleContainer>& density_b,
    std::complex<double> shotnoise,
    int ell, int m,
    fftw_complex* xi
  ) {  // ??? find matching equation
    double vol_factor = 1. / this->params.volume;  // convert ∫d^3k = (1/V) Σ_i

    /// Compute gridded power spectrum.  See also `calc_power_spec` above.
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    /// ??? NOTE: Convert δ δ* = (2π)^3 δ_D(k_1 + k_2) P(k), where
    /// (2π)^3 δ_D(k_1 + k_2) <-> V, thus
    /// P(k) = (V/N^2) |δ(k)|^2.

    double kvec[3];
    for (int i = 0; i < this->params.nmesh[0]; i++) {
      for (int j = 0; j < this->params.nmesh[1]; j++) {
        for (int k = 0; k < this->params.nmesh[2]; k++) {
          long long coord_flat =
            (i * this->params.nmesh[1] + j) * this->params.nmesh[2] + k;

          kvec[0] = (i < this->params.nmesh[0]/2) ?
            i * dk[0] : (i - this->params.nmesh[0]) * dk[0];
          kvec[1] = (j < this->params.nmesh[1]/2) ?
            j * dk[1] : (j - this->params.nmesh[1]) * dk[1];
          kvec[2] = (k < this->params.nmesh[2]/2) ?
            k * dk[2] : (k - this->params.nmesh[2]) * dk[2];

          std::complex<double> delta_a(
            density_a[coord_flat][0], density_a[coord_flat][1]
          );
          std::complex<double> delta_b(
            density_b[coord_flat][0], density_b[coord_flat][1]
          );
          std::complex<double> mode_power = delta_a * conj(delta_b);

          mode_power -= shotnoise * calc_shotnoise_func(kvec);

          double win = calc_interpolation_window_in_fourier(kvec);
          mode_power /= pow(win, 2);

          xi[coord_flat][0] = vol_factor * mode_power.real();
          xi[coord_flat][1] = vol_factor * mode_power.imag();
        }
      }
    }

    /// Inverse Fourier transform.
    fftw_plan fft_plan_backward = fftw_plan_dft_3d(
      this->params.nmesh[0], this->params.nmesh[1], this->params.nmesh[2],
      xi, xi,
      FFTW_BACKWARD, FFTW_ESTIMATE
    );
    fftw_execute(fft_plan_backward);
    fftw_destroy_plan(fft_plan_backward);

    return 0;
  }

  /**
   * Calculate the shot noise scale-dependent function.
   *
   * @param kvec Wavevector.
   * @returns Value of the shot noise function.
   */
  double calc_shotnoise_func(double* kvec) {
    /// See below eqs. (45) and (46) in arXiv:1803.02132, as well as
    /// arXiv:astro-ph/0409240.
    if (0) {
    } else if (this->params.assignment == "NGP") {
      return this->calc_shotnoise_func_ngp(kvec);
    } else if (this->params.assignment == "CIC") {
      return this->calc_shotnoise_func_cic(kvec);
    } else if (this->params.assignment == "TSC") {
      return this->calc_shotnoise_func_tsc(kvec);
    } else {
      return 0.;
    }

    return 0.;
  }

  /**
   * Calculate the shot noise scale-dependent function for the
   * nearest-grid-point assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Value of the shot noise function.
   */
  double calc_shotnoise_func_ngp(const double* kvec) {
    return 1.;
  }

  /**
   * Calculate the shot noise scale-dependent function for the
   * cloud-in-cell assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns val Value of the shot noise function.
   */
  double calc_shotnoise_func_cic(const double* kvec) {
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double cx = (i != 0) ? sin(xk): 0.;
    double cy = (j != 0) ? sin(yk): 0.;
    double cz = (k != 0) ? sin(zk): 0.;
    double val = (1. - 2./3. * cx * cx)
      * (1. - 2./3. * cy * cy)
      * (1. - 2./3. * cz * cz);

    return val;
  }

  /**
   * Calculate the shot noise scale-dependent function for the
   * triangular-shaped-cloud assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns val Value of the shot noise function.
   */
  double calc_shotnoise_func_tsc(const double* kvec) {
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double cx = (i != 0) ? sin(xk): 0.;
    double cy = (j != 0) ? sin(yk): 0.;
    double cz = (k != 0) ? sin(zk): 0.;
    double val = (1. - cx * cx + 2./15. * pow(cx, 4))
      * (1. - cy * cy + 2./15. * pow(cy, 4))
      * (1. - cz * cz + 2./15. * pow(cz, 4));

    return val;
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * assignment schemes.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier(double* kvec) {  // ??? redudant
    if (0) {
    } else if (this->params.assignment == "NGP") {
      return this->calc_interpolation_window_in_fourier_ngp(kvec);
    } else if (this->params.assignment == "CIC") {
      return this->calc_interpolation_window_in_fourier_cic(kvec);
    } else if (this->params.assignment == "TSC") {
      return this->calc_interpolation_window_in_fourier_tsc(kvec);
    } else {
      return 1.;
    }

    return 1.;
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * the nearest-grid-point assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier_ngp(const double* kvec) {  // ??? redudant
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double wx = (i != 0) ? sin(xk) / xk : 1.;
    double wy = (j != 0) ? sin(yk) / yk : 1.;
    double wz = (k != 0) ? sin(zk) / zk : 1.;
      // sin(x) / x -> 1 as x -> 0
    double wk = wx * wy * wz;

    return pow(wk, 1);
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * the cloud-in-cell assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier_cic(const double* kvec) {  // ??? redudant
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double wx = (i != 0) ? sin(xk) / xk : 1.;
    double wy = (j != 0) ? sin(yk) / yk : 1.;
    double wz = (k != 0) ? sin(zk) / zk : 1.;
      // sin(x) / x -> 1 as x -> 0
    double wk = wx * wy * wz;

    return pow(wk, 2);
  }

  /**
   * Calculate interpolarion window in Fourier space for
   * the triangular-shaped-cloud assignment scheme.
   *
   * @param kvec Wavevector.
   * @returns Window value in Fourier space.
   */
  double calc_interpolation_window_in_fourier_tsc(const double* kvec) {  // ??? redudant
    double dk[3];
    dk[0] = 2.*M_PI / this->params.boxsize[0];
    dk[1] = 2.*M_PI / this->params.boxsize[1];
    dk[2] = 2.*M_PI / this->params.boxsize[2];

    int i = int(kvec[0] / dk[0] + 1.e-5);
    int j = int(kvec[1] / dk[1] + 1.e-5);
    int k = int(kvec[2] / dk[2] + 1.e-5);

    double xk = M_PI * i / double(this->params.nmesh[0]);
    double yk = M_PI * j / double(this->params.nmesh[1]);
    double zk = M_PI * k / double(this->params.nmesh[2]);

    double wx = (i != 0) ? sin(xk) / xk : 1.;
    double wy = (j != 0) ? sin(yk) / yk : 1.;
    double wz = (k != 0) ? sin(zk) / zk : 1.;
      // sin(x) / x -> 1 as x -> 0
    double wk = wx * wy * wz;

    return pow(wk, 3);
  }

 private:
  ParameterSet params;
};

#endif
