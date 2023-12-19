#include <math.h>

#include "window_functions.h"

static constexpr real sq(real x) noexcept { return x * x; }

void cosine_window(real *w, unsigned n, const real *coeff, unsigned ncoeff, bool sflag) {
    // Generalized cosine window.
    //
    // Many window functions described in signal processing literature can be written as linear combinations of cosines over the window length.
    //
    // Let 'x' be values going from 0 for the first element, and 2*pi for the last element.
    // The window can then be written as:
    //
    // w = c0 * cos(0 * x) + c1 * cos(1 * x) + c2 * cos(2 * x) + c3 * cos(3 * x) + ...
    //
    // (Note that the first term simplifies to just the constant value c0.)
    //
    // Examples of cosine windows implemented in Matlab:
    //
    //                              c0          c1           c2           c3            c4
    // -------------------------------------------------------------------------------------------
    // rectangular window          1.0
    // hann window                 0.5         -0.5
    // hamming window              0.54        -0.46
    // blackman window             0.42        -0.5         0.08
    // blackman-harris window      0.35875     -0.48829     0.14128      -0.01168
    // nuttall window              0.3635819   -0.4891775   0.1365995    -0.0106411
    // flattop window              0.21557895  -0.41663158  0.277263158  -0.083578947  0.006947368
    //
    // The "flattop" coefficients given above follow Matlab's "flattopwin" implementation.
    // The signal processing literature in fact describes many different 'flattop' windows.
    //
    // Note 1 : Octave defines the flattopwin coefficients differently, see implementation below.
    //     The coefficient values used correspond to:
    //     [0.21550795224343777, -0.4159303478298349, 0.2780052583940347, -0.08361708547045386, 0.006939356062238697]
    //
    // Note 2 : Octave defines the nuttallwin coefficients differently, see implementation below:
    //     The coefficient values used are:
    //     [0.355768, -0.487396, 0.144232, -0.012604]

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    const unsigned wlength = sflag ? (n - 1) : n;
    for (unsigned i = 0; i < n; ++i) {
        real wi = 0.0;
        for (unsigned j = 0; j < ncoeff; ++j) wi += coeff[j] * __cospi(real(2 * i * j) / real(wlength));
        w[i] = wi;
    }
}

void rectwin(real *w, unsigned n) {
    // Technically, this is a cosine window with coefficient {1}.
    for (unsigned i = 0; i < n; ++i) w[i] = 1.0;
}

void hann(real *w, unsigned n, bool sflag) {
    // Hann window.
    //
    // Extrema are 0.
    // Center value is 1 for odd length,
    //     0.5 - 0.5 * cos(pi * L / (L - 1)) for even length.

    const real coeff[2] = {0.5, -0.5};
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(real), sflag);
}

void hamming(real *w, unsigned n, bool sflag) {
    // Hamming window
    //
    // Note that the Hamming window is raised; its extreme values are 0.08.
    //
    // The center value is 1 for odd length;
    // The venter values are 0.54 - 0.46 * cos(pi * L / (L - 1)) for even length.

    const real coeff[2] = {0.54, -0.46};
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(real), sflag);
}

void blackman(real *w, unsigned n, bool sflag) {
    // Blackman window

    const real coeff[3] = {0.42, -0.5, 0.08};
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(real), sflag);
}

void blackmanharris(real *w, unsigned n, bool sflag) {
    // Blackman-Harris window
    //
    // Note: very similar to the Nuttall window.

    const real coeff[4] = {0.35875, -0.48829, 0.14128, -0.01168};
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(real), sflag);
}

void nuttallwin(real *w, unsigned n, bool sflag) {
    // Nuttall window
    //
    // Note: very similar to the Blackman-Harris window.

    const real coeff[4] = {0.3635819, -0.4891775, 0.1365995, -0.0106411};
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(real), sflag);
}

void flattopwin(real *w, unsigned n, bool sflag) {
    // Flattop window
    //
    // This window contains negative values.

    const real coeff[5] = {0.21557895, -0.41663158, 0.277263158, -0.083578947, 0.006947368};
    cosine_window(w, n, coeff, sizeof(coeff) / sizeof(real), sflag);
}

void triang(real *w, unsigned n) {
    // Triangular window
    //
    //   triang(1) == {              1.0              }
    //   triang(2) == {            0.5 0.5            }
    //   triang(3) == {          0.5 1.0 0.5          }
    //   triang(4) == {      0.25 0.75 0.75 0.25      }
    //   triang(5) == {    0.33 0.66 1.0 0.66 0.33    }
    //   triang(6) == { 0.16 0.50 0.83 0.83 0.50 0.16 }
    //
    // Even length:
    //     Center values are (1 - 1 / L); extrema are (1 / L).
    //
    // Odd length:
    //     Center value is 1; extrema are 2 / (L + 1).

    const unsigned denominator = (n % 2 != 0) ? (n + 1) : n;
    for (unsigned i = 0; i < n; ++i) w[i] = 1.0 - fabs(2.0 * i - (n - 1)) / denominator;
}

void bartlett(real *w, unsigned n) {
    // Bartlett window
    //
    //   bartlett(1) == {           1.0           }
    //   bartlett(2) == {         0.0 0.0         }
    //   bartlett(3) == {       0.0 1.0 0.0       }
    //   bartlett(4) == {    0.0 0.66 0.66 0.0    }
    //   bartlett(5) == {   0.0 0.5 1.0 0.5 0.0   }
    //   bartlett(6) == { 0.0 0.4 0.8 0.8 0.4 0.0 }
    //
    // Center value is 1 for odd length, 1 - 1 / (L - 1) for even length.
    // Extrema are 0.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    const unsigned denominator = (n - 1);
    for (unsigned i = 0; i < n; ++i) w[i] = 1.0 - fabs(2.0 * i - (n - 1)) / denominator;
}

void barthannwin(real *w, unsigned n) {
    // Modified Bartlett-Hann window.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    for (unsigned i = 0; i < n; ++i) {
        const real x = fabs(i / (n - 1.0) - 0.5);
        w[i] = 0.62 - 0.48 * x + 0.38 * __cospi(2.0 * x);
    }
}

void bohmanwin(real *w, unsigned n) {
    // Bohmann window.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    for (unsigned i = 0; i < n; ++i) {
        const real x = fabs(2.0 * i - (n - 1)) / (n - 1);
        w[i] = (1.0 - x) * __cospi(x) + __sinpi(x) * M_1_PI;
    }
}

void parzenwin(real *w, unsigned n) {
    // The Parzen window.
    //
    // This is an approximation of the Gaussian window.
    // The Gaussian shape is approximated by two different polynomials, one for x < 0.5 and one for x > 0.5.
    // At x == 0.5, the polynomials meet. The minimum value of the two polynomials is taken.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    for (unsigned i = 0; i < n; ++i) {
        const real x = fabs(2.0 * i - (n - 1)) / n;
        const real y = 1.0 - x;
        w[i] = fmin(1.0 - 6.0 * x * x + 6.0 * x * x * x, 2.0 * y * y * y);
    }
}

void gausswin(real *w, unsigned n, real alpha) {
    // Gaussian window.

    // The parameter for the gausswin() function is different for the Matlab, Octave, and SciPy versions of this function:

    // - Matlab uses "Alpha", with a default value of 2.5.
    // - Octave uses "A";
    // - Scipy uses "std".
    //  Matlab vs SciPy:     Alpha * std == (N - 1) / 2
    //  Matlab vs Octave:    Alpha * N == A * (N - 1)

    // In this implementation, we follow the Matlab convention.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    for (unsigned i = 0; i < n; ++i) {
        const real x = fabs(2.0 * i - (n - 1)) / (n - 1);
        const real ax = alpha * x;
        w[i] = exp(-0.5 * sq(ax));
    }
}

void tukeywin(real *w, unsigned n, real r) {
    // Tukey window.

    // This window uses a cosine-shaped ramp-up and ramp-down, with an all-one part in the middle.
    // The parameter 'r' defines the fraction of the window covered by the ramp-up and ramp-down.

    // r <= 0 is identical to a rectangular window.
    // r >= 1 is identical to a Hann window.
    //
    // In Matlab, the default value for parameter r is 0.5.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    r = fmax(0.0, fmin(1.0, r)); // Clip between 0 and 1.
    for (unsigned i = 0; i < n; ++i) {
        w[i] = (__cospi(fmax(fabs((real)i - (n - 1) / 2.0) * (2.0 / (n - 1) / r) - (1.0 / r - 1.0), 0.0)) + 1.0) / 2.0;
    }
}

void taylorwin(real *w, unsigned n, unsigned nbar, real sll) {
    // Taylor window.
    //
    // Default Matlab parameters: nbar ==4, sll == -30.0.
    //
    // The Taylor window is cosine-window like, in that it is the sum of weighted cosines of different periods.

    // sll is in dB(power).
    // Calculate the amplification factor, e.g. sll = -60 --> amplification = 1000.0

    const real amplification = pow(10.0, -sll / 20.0);
    const real a = acosh(amplification) * M_1_PI;
    const real a2 = sq(a);

    // Taylor pulse widening (dilation) factor.

    const real sp2 = sq(nbar) / (a2 + sq(nbar - 0.5));
    for (unsigned i = 0; i < n; ++i) w[i] = 1.0; // Initial value.

    for (unsigned m = 1; m < nbar; ++m) {
        // Calculate Fm as a function of: m, sp2, a
        real numerator = 1.0;
        real denominator = 1.0;
        for (unsigned i = 1; i < nbar; ++i) {
            numerator *= (1.0 - sq(m) / (sp2 * (a2 + sq(i - 0.5))));
            if (i != m) denominator *= (1.0 - sq(m) / sq(i));
        }

        // Add cosine term to each of the window components.
        const real Fm = -(numerator / denominator);
        for (unsigned i = 0; i < n; ++i) {
            w[i] += Fm * __cospi(real(2.0 * m * (i + 0.5)) / real(n));
        }
    }
}

static real chbevl(real x, const real *coeff, unsigned n) {
    // This implementation was derived from the Cephes Math Library implementation:
    //
    //    Cephes Math Library Release 2.8:  June, 2000
    //    Copyright 1984, 1987, 2000 by Stephen L. Moshier

    // Evaluate Chebyshev polynomial at 'x'.

    real b0 = 0.0;
    real b1 = 0.0;
    real b2;
    for (unsigned i = 0; i < n; ++i) {
        b2 = b1;
        b1 = b0;
        b0 = x * b1 - b2 + coeff[i];
    }
    return 0.5 * (b0 - b2);
}

static real bessel_i0(real x) {
    // This function is needed for the calculation of the Kaiser window function.

    // This implementation was derived from the Cephes Math Library implementation:
    //
    //    Cephes Math Library Release 2.8:  June, 2000
    //    Copyright 1984, 1987, 2000 by Stephen L. Moshier

    const real A[30] = {
        -4.41534164647933937950e-18, 3.33079451882223809783e-17,
        -2.43127984654795469359e-16, 1.71539128555513303061e-15,
        -1.16853328779934516808e-14, 7.67618549860493561688e-14,
        -4.85644678311192946090e-13, 2.95505266312963983461e-12,
        -1.72682629144155570723e-11, 9.67580903537323691224e-11,
        -5.18979560163526290666e-10, 2.65982372468238665035e-9,
        -1.30002500998624804212e-8, 6.04699502254191894932e-8,
        -2.67079385394061173391e-7, 1.11738753912010371815e-6,
        -4.41673835845875056359e-6, 1.64484480707288970893e-5,
        -5.75419501008210370398e-5, 1.88502885095841655729e-4,
        -5.76375574538582365885e-4, 1.63947561694133579842e-3,
        -4.32430999505057594430e-3, 1.05464603945949983183e-2,
        -2.37374148058994688156e-2, 4.93052842396707084878e-2,
        -9.49010970480476444210e-2, 1.71620901522208775349e-1,
        -3.04682672343198398683e-1, 6.76795274409476084995e-1
    };

    const real B[25] = {
        -7.23318048787475395456e-18, -4.83050448594418207126e-18,
        4.46562142029675999901e-17, 3.46122286769746109310e-17,
        -2.82762398051658348494e-16, -3.42548561967721913462e-16,
        1.77256013305652638360e-15, 3.81168066935262242075e-15,
        -9.55484669882830764870e-15, -4.15056934728722208663e-14,
        1.54008621752140982691e-14, 3.85277838274214270114e-13,
        7.18012445138366623367e-13, -1.79417853150680611778e-12,
        -1.32158118404477131188e-11, -3.14991652796324136454e-11,
        1.18891471078464383424e-11, 4.94060238822496958910e-10,
        3.39623202570838634515e-9, 2.26666899049817806459e-8,
        2.04891858946906374183e-7, 2.89137052083475648297e-6,
        6.88975834691682398426e-5, 3.36911647825569408990e-3,
        8.04490411014108831608e-1
    };

    x = fabs(x);
    return exp(x) * (x <= 8.0 ? chbevl(x / 2.0 - 2.0, A, 30) : (chbevl(32.0 / x - 2.0, B, 25) / sqrt(x)));
}

void kaiser(real *w, unsigned n, real beta) {
    // Kaiser window.
    // In Matlab, the default value for parameter beta is 0.5.

    if (n == 1) {
        w[0] = 1.0;
        return;
    }

    for (unsigned i = 0; i < n; ++i) {
        const real x = real(2.0 * i - (n - 1)) / real(n - 1);
        w[i] = bessel_i0(beta * sqrt(1.0 - sq(x))) / bessel_i0(beta);
    }
}

// Periodic defaults for cosine windows
void hann_periodic(real *w, unsigned n) { hann(w, n, false); }
void hamming_periodic(real *w, unsigned n) { hamming(w, n, false); }
void blackman_periodic(real *w, unsigned n) { blackman(w, n, false); }
void blackmanharris_periodic(real *w, unsigned n) { blackmanharris(w, n, false); }
void nuttallwin_periodic(real *w, unsigned n) { nuttallwin(w, n, false); }
void flattopwin_periodic(real *w, unsigned n) { flattopwin(w, n, false); }
