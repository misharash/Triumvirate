"""
Two-Point Correlator Algorithms (:mod:`~triumvirate._twopt`)
==========================================================================

Declaration and wrapping of two-point statistic algorithms.

"""
from cython.operator cimport dereference as deref
from libc.stdlib cimport free, malloc

import numpy as np
cimport numpy as np

from triumvirate._particles cimport CppParticleCatalogue, _ParticleCatalogue
from triumvirate.dataobjs cimport (
    Binning, CppBinning,
    LineOfSight,
    PowspecMeasurements, TwoPCFMeasurements, TwoPCFWindowMeasurements
)
from triumvirate.parameters cimport CppParameterSet, ParameterSet


cdef extern from "include/twopt.hpp":
    # --------------------------------------------------------------------
    # Normalisation
    # --------------------------------------------------------------------

    double calc_powspec_normalisation_from_mesh_cpp \
        "trv::calc_powspec_normalisation_from_mesh" (
            CppParticleCatalogue& catalogue,
            CppParameterSet& params,
            double alpha
        )

    double calc_powspec_normalisation_from_particles_cpp \
        "trv::calc_powspec_normalisation_from_particles" (
            CppParticleCatalogue& catalogue,
            double alpha
        ) except +


    # --------------------------------------------------------------------
    # Full statistics
    # --------------------------------------------------------------------

    PowspecMeasurements compute_powspec_cpp "trv::compute_powspec" (
        CppParticleCatalogue& particles_data,
        CppParticleCatalogue& particles_rand,
        LineOfSight* los_data,
        LineOfSight* los_rand,
        CppParameterSet& params,
        CppBinning& kbinning,
        double norm_factor
    )

    TwoPCFMeasurements compute_corrfunc_cpp "trv::compute_corrfunc" (
        CppParticleCatalogue& particles_data,
        CppParticleCatalogue& particles_rand,
        LineOfSight* los_data,
        LineOfSight* los_rand,
        CppParameterSet& params,
        CppBinning& rbinning,
        double norm_factor
    )

    PowspecMeasurements compute_powspec_in_gpp_box_cpp \
        "trv::compute_powspec_in_gpp_box" (
            CppParticleCatalogue& particles_data,
            CppParameterSet& params,
            CppBinning& kbinning,
            double norm_factor
        )

    TwoPCFMeasurements compute_corrfunc_in_gpp_box_cpp \
        "trv::compute_corrfunc_in_gpp_box" (
            CppParticleCatalogue& particles_data,
            CppParameterSet& params,
            CppBinning& rbinning,
            double norm_factor
        )

    TwoPCFWindowMeasurements compute_corrfunc_window_cpp \
        "trv::compute_corrfunc_window" (
            CppParticleCatalogue& particles_rand,
            LineOfSight* los_rand,
            CppParameterSet& params,
            CppBinning& rbinning,
            double alpha,
            double norm_factor
        )


def _calc_powspec_normalisation_from_mesh(
        _ParticleCatalogue catalogue not None,
        ParameterSet params not None,
        double alpha
    ):
    return calc_powspec_normalisation_from_mesh_cpp(
        deref(catalogue.thisptr), deref(params.thisptr), alpha
    )


def _calc_powspec_normalisation_from_particles(
        _ParticleCatalogue catalogue not None, double alpha
    ):
    return calc_powspec_normalisation_from_particles_cpp(
        deref(catalogue.thisptr), alpha
    )


def _compute_powspec(
        _ParticleCatalogue particles_data not None,
        _ParticleCatalogue particles_rand not None,
        np.ndarray[double, ndim=2, mode='c'] los_data not None,
        np.ndarray[double, ndim=2, mode='c'] los_rand not None,
        ParameterSet params not None,
        Binning kbinning not None,
        double norm_factor
    ):
    # Parse lines of sight per particle.
    cdef LineOfSight* los_data_cpp = <LineOfSight*>malloc(
        len(los_data) * sizeof(LineOfSight)
    )
    for pid, (los_x, los_y, los_z) in enumerate(los_data):
        los_data_cpp[pid].pos[0] = los_x
        los_data_cpp[pid].pos[1] = los_y
        los_data_cpp[pid].pos[2] = los_z

    cdef LineOfSight* los_rand_cpp = <LineOfSight*>malloc(
        len(los_rand) * sizeof(LineOfSight)
    )
    for pid, (los_x, los_y, los_z) in enumerate(los_rand):
        los_rand_cpp[pid].pos[0] = los_x
        los_rand_cpp[pid].pos[1] = los_y
        los_rand_cpp[pid].pos[2] = los_z

    # Run algorithm.
    cdef PowspecMeasurements results
    results = compute_powspec_cpp(
        deref(particles_data.thisptr), deref(particles_rand.thisptr),
        los_data_cpp, los_rand_cpp,
        deref(params.thisptr), deref(kbinning.thisptr),
        norm_factor
    )

    free(los_data_cpp); free(los_rand_cpp)

    return {
        'kbin': np.array(results.kbin),
        'keff': np.array(results.keff),
        'nmodes': np.array(results.nmodes),
        'pk_raw': np.array(results.pk_raw),
        'pk_shot': np.array(results.pk_shot),
    }


def _compute_corrfunc(
        _ParticleCatalogue particles_data not None,
        _ParticleCatalogue particles_rand not None,
        np.ndarray[double, ndim=2, mode='c'] los_data not None,
        np.ndarray[double, ndim=2, mode='c'] los_rand not None,
        ParameterSet params not None,
        Binning rbinning not None,
        double norm_factor
    ):
    # Parse lines of sight per particle.
    cdef LineOfSight* los_data_cpp = <LineOfSight*>malloc(
        len(los_data) * sizeof(LineOfSight)
    )
    for pid, (los_x, los_y, los_z) in enumerate(los_data):
        los_data_cpp[pid].pos[0] = los_x
        los_data_cpp[pid].pos[1] = los_y
        los_data_cpp[pid].pos[2] = los_z

    cdef LineOfSight* los_rand_cpp = <LineOfSight*>malloc(
        len(los_rand) * sizeof(LineOfSight)
    )
    for pid, (los_x, los_y, los_z) in enumerate(los_rand):
        los_rand_cpp[pid].pos[0] = los_x
        los_rand_cpp[pid].pos[1] = los_y
        los_rand_cpp[pid].pos[2] = los_z

    # Run algorithm.
    cdef TwoPCFMeasurements results
    results = compute_corrfunc_cpp(
        deref(particles_data.thisptr), deref(particles_rand.thisptr),
        los_data_cpp, los_rand_cpp,
        deref(params.thisptr), deref(rbinning.thisptr),
        norm_factor
    )

    free(los_data_cpp); free(los_rand_cpp)

    return {
        'rbin': np.array(results.rbin),
        'reff': np.array(results.reff),
        'npairs': np.array(results.npairs),
        'xi': np.array(results.xi),
    }


def _compute_powspec_in_gpp_box(
        _ParticleCatalogue particles_data not None,
        ParameterSet params not None,
        Binning kbinning not None,
        double norm_factor
    ):
    cdef PowspecMeasurements results
    results = compute_powspec_in_gpp_box_cpp(
        deref(particles_data.thisptr),
        deref(params.thisptr), deref(kbinning.thisptr),
        norm_factor
    )

    return {
        'kbin': np.array(results.kbin),
        'keff': np.array(results.keff),
        'nmodes': np.array(results.nmodes),
        'pk_raw': np.array(results.pk_raw),
        'pk_shot': np.array(results.pk_shot),
    }


def _compute_corrfunc_in_gpp_box(
        _ParticleCatalogue particles_data not None,
        ParameterSet params not None,
        Binning rbinning not None,
        double norm_factor
    ):
    cdef TwoPCFMeasurements results
    results = compute_corrfunc_in_gpp_box_cpp(
        deref(particles_data.thisptr),
        deref(params.thisptr), deref(rbinning.thisptr),
        norm_factor
    )

    return {
        'rbin': np.array(results.rbin),
        'reff': np.array(results.reff),
        'npairs': np.array(results.npairs),
        'xi': np.array(results.xi),
    }


def _compute_corrfunc_window(
        _ParticleCatalogue particles_rand not None,
        np.ndarray[double, ndim=2, mode='c'] los_rand not None,
        ParameterSet params not None,
        Binning rbinning not None,
        double alpha,
        double norm_factor
    ):
    # Parse lines of sight per particle.
    cdef LineOfSight* los_rand_cpp = <LineOfSight*>malloc(
        len(los_rand) * sizeof(LineOfSight)
    )
    for pid, (los_x, los_y, los_z) in enumerate(los_rand):
        los_rand_cpp[pid].pos[0] = los_x
        los_rand_cpp[pid].pos[1] = los_y
        los_rand_cpp[pid].pos[2] = los_z

    # Run algorithm.
    cdef TwoPCFWindowMeasurements results
    results = compute_corrfunc_window_cpp(
        deref(particles_rand.thisptr), los_rand_cpp,
        deref(params.thisptr), deref(rbinning.thisptr),
        alpha, norm_factor
    )

    free(los_rand_cpp)

    return {
        'rbin': np.array(results.rbin),
        'reff': np.array(results.reff),
        'npairs': np.array(results.npairs),
        'xi': np.array(results.xi),
    }
