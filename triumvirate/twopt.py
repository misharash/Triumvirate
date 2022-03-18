"""
Two-Point Correlator Measurements (:mod:`~triumvirate.twopt`)
==========================================================================

Measuring two-point correlator statistics from catalogues.

"""
import numpy as np

from parameters import InvalidParameter
from _catalogue import _ParticleCatalogue
from _twopt import (
    _calc_powspec_normalisation_from_mesh,
    _calc_powspec_normalisation_from_particles,
    _compute_powspec,
)

# TODO: Complete docs.
def compute_powspec(catalogue_data, catalogue_rand, params,
                    los_data=None, los_rand=None, logger=None):
    """Compute power spectrum from data and random catalogues.

    Parameters
    ----------
    catalogue_data : :class:`~triumvirate.catalogue.ParticleCatalogue`
        Data-source catalogue.
    catalogue_rand : :class:`~triumvirate.catalogue.ParticleCatalogue`
        Random-source catalogue.
    params : :class:`~triumvirate.parameters.ParameterSet`
        Measurement parameters.
    los_data : (N, 3) array of float, optional
        Specified lines of sight for the data-source catalogue.
        If `None` (default), this is automatically computed using
        :meth:`~triumvirate.catalogue.ParticleCatalogue.compute_los`.
    los_rand : (N, 3) array of float, optional
        Specified lines of sight for the random-source catalogue.
        If `None` (default), this is automatically computed using
        :meth:`~triumvirate.catalogue.ParticleCatalogue.compute_los`.
    logger : :class:`logging.Logger`, optional
        Logger (default is `None`).

    Returns
    -------
    _type_
        _description_

    """
    # Prepare catalogues.
    catalogue_data.boxify_catalogues_for_fft(
        [params['boxsize'][axis] for axis in ['x', 'y', 'z']],
        [params['ngrid'][axis] for axis in ['x', 'y', 'z']],
        catalogue_ref=catalogue_rand
    )

    nz_data = np.nan_to_num(np.array(catalogue_data._pdata['nz'], dtype=float))
    nz_rand = np.nan_to_num(np.array(catalogue_rand._pdata['nz'], dtype=float))
    nz_data = 5.e-4 * np.ones_like(catalogue_data._pdata['nz'], dtype=float)
    nz_rand = 5.e-4 * np.ones_like(catalogue_rand._pdata['nz'], dtype=float)

    particles_data = _ParticleCatalogue(
        catalogue_data._pdata['x'],
        catalogue_data._pdata['y'],
        catalogue_data._pdata['z'],
        nz_data,
        catalogue_data._pdata['ws'],
        catalogue_data._pdata['wc']
    )
    particles_rand = _ParticleCatalogue(
        catalogue_rand._pdata['x'],
        catalogue_rand._pdata['y'],
        catalogue_rand._pdata['z'],
        nz_rand,
        catalogue_rand._pdata['ws'],
        catalogue_rand._pdata['wc']
    )

    # Compute auxiliary quantities.
    if los_data is None:
        los_data = catalogue_data.compute_los()
    if los_rand is None:
        los_rand = catalogue_rand.compute_los()

    los_data = np.ascontiguousarray(los_data)
    los_rand = np.ascontiguousarray(los_rand)

    kbin = np.ascontiguousarray(
        np.linspace(*params['range'], num=params['dim'])
    )

    alpha = catalogue_data.wtotal / catalogue_rand.wtotal

    if logger:
        try:
            logger.info("", cpp_state='start')
        except TypeError:
            logger.info("Entering C++ run...")

    if params['norm_convention'] == 'mesh':
        norm = _calc_powspec_normalisation_from_mesh(particles_rand, params, alpha)
    elif params['norm_convention'] == 'particle':
        norm = _calc_powspec_normalisation_from_particles(particles_rand, alpha)
    else:
        raise InvalidParameter("Invalid `norm_convention` parameter.")

    if logger:
        try:
            logger.info("", cpp_state='end')
        except TypeError:
            logger.info("... exited C++ run.")

    if logger:
        logger.info("Alpha contrast: %.6e.", alpha)
        logger.info("Normalisation constant: %.6e.", norm)

    # Perform measurement.
    if logger:
        try:
            logger.info("", cpp_state='start')
        except TypeError:
            logger.info("Entering C++ run...")

    # TODO: FIXME
    _compute_powspec(
        particles_data, particles_rand, los_data, los_rand,
        params, kbin, alpha, norm, save=True
    )

    if logger:
        try:
            logger.info("", cpp_state='end')
        except TypeError:
            logger.info("... exited C++ run.")

