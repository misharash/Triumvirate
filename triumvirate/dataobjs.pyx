"""
Data Objects (:mod:`~triumvirate.dataobjs`)
==========================================================================

Define data objects.

"""
import numpy as np

from .dataobjs cimport CppBinning


cdef class Binning:
    """Binning.

    Parameters
    ----------
    space : {'config', 'fourier'}
        Coordinate space.
    scheme : {'lin', 'log', 'linpad', 'logpad'}
        Binning scheme.
    bin_min, bin_max : float, optional
        Minimum and maximum of the bin range (defaults are `None`).
    num_bins : int, optional
        Number of bins (default is `None`).

    Attributes
    ----------
    space : {'config', 'fourier'}
        Coordinate space.
    scheme : {'lin', 'log', 'linpad', 'logpad'}
        Binning scheme.
    bin_min, bin_max : float or None
        Minimum and maximum of the bin range.
    num_bins : int or None
        Number of bins.
    bin_edges : (:attr:`num_bins` + 1,) list of float
        Bin edges.
    bin_centres : (:attr:`num_bins`,) list of float
        Bin centres.
    bin_widths : (:attr:`num_bins`,) list of float
        Bin widths.

    Notes
    -----
    The bin setting method supports linear ('lin') and
    log-linear/exponential ('log') binning, with possible linear padding
    from zero for the first 5 bins ('linpad' or 'logpad').  The padding
    is either 1.e-3 ('fourier' space) and 10. ('config' space), or
    determined by the mesh grid resolution if set using mesh grid sizes.

    """

    def __cinit__(self, space, scheme,
                  bin_min=None, bin_max=None, num_bins=None):

        self.thisptr = new CppBinning(
            space.encode('utf-8'), scheme.encode('utf-8')
        )

        self.scheme = scheme
        self.space = space

        _settable = True
        if bin_min is not None:
            self.bin_min = bin_min
        else:
            _settable = False
        if bin_max is not None:
            self.bin_max = bin_max
        else:
            _settable = False
        if num_bins is not None:
            self.num_bins = num_bins
        else:
            _settable = False

        if _settable:
            self.set_bins(self.bin_min, self.bin_max, self.num_bins)

    @classmethod
    def from_parameter_set(cls, parameter_set):
        """Create binning scheme from a parameter set.

        This sets :attr:`scheme` and :attr:`space`.

        Parameters
        ----------
        parameter_set : :class:`~triumvirate.parameters.ParameterSet`
            Parameter set.

        """
        self = cls(
            parameter_set['space'], parameter_set['binning'],
            bin_min=parameter_set['range'][0],
            bin_max=parameter_set['range'][-1],
            num_bins=parameter_set['num_bins']
        )

        return self

    def set_bins(self, bin_min, bin_max, num_bins):
        """Set bin quantities including bin edges, centres and widths.

        Parameters
        ----------
        bin_min, bin_max : float
            Minimum and maximum of the bin range.
        num_bins : int
            Number of bins.

        """
        self.thisptr.set_bins(bin_min, bin_max, num_bins)

        self.bin_edges = self.thisptr.bin_edges
        self.bin_centres = self.thisptr.bin_centres
        self.bin_widths = self.thisptr.bin_widths

    def set_grid_based_bins(self, boxsize, ngrid):
        """Set linear binning based on a mesh grid.

        The binning scheme is overriden to 'lin'.  The bin width is
        given by the grid resolution in configuration space or the
        fundamental wavenumber in Fourier space.  The bin minimum is zero
        and the bin maximum is half the boxsize in configuration space or
        the Nyquist wavenumber in Fourier space.

        Parameters
        ----------
        boxsize : (array of) float
            Mesh box size (maximum).
        ngrid : (array of) int
            Mesh grid number (minimum).

        """
        boxsize = np.asarray(boxsize).max()
        ngrid = np.asarray(ngrid).min()

        self.thisptr.set_bins(boxsize, ngrid)

        self.bin_edges = self.thisptr.bin_edges
        self.bin_centres = self.thisptr.bin_centres
        self.bin_widths = self.thisptr.bin_widths

    def set_custom_bins(self, bin_edges):
        """Set custom bins using bin edges.

        :attr:`bin_centres` and :attr:`bin_widths` are also set
        for internal consistency.

        Parameters
        ----------
        bin_edges : array of float
            Bin edges of length (:attr:`num_bins` + 1).

        """
        bin_centres = np.add(bin_edges[:-1], bin_edges[1:]) / 2.
        bin_widths = np.subtract(bin_edges[1:], bin_edges[:-1])

        self.bin_edges = bin_edges
        self.bin_centres = bin_centres
        self.bin_widths = bin_widths

        self.thisptr.bin_edges = self.bin_edges
        self.thisptr.bin_centres = self.bin_centres
        self.thisptr.bin_widths = self.bin_widths
