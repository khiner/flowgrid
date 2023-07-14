// Copied from https://github.com/sidneycadot/WindowFunctions with only minor modifications,
// excluding the FFT and Chebyshev implementation, using configurable `real` type instead of only `real`,
// and some minor cleanup & performance improvements.

#pragma once

typedef float real;

// COSINE WINDOWS

// The cosine window functions have an `sflag` parameter that specifies whether the generated window should be
// 'symmetric' (sflag == true), or 'periodic' (sflag == false).
// The symmetric choice returns a perfectly symmetric window.
// The periodic choice works as if a window with (size + 1) elements is calculated, after which the last element is dropped.
// A symmetric window is preferred in FIR filter design, while a periodic window is preferred in spectral analysis.

using WindowFunctionType = void (*)(float *, unsigned);

// Generic cosine window
void cosine_window(real *w, unsigned n, const real *coeff, unsigned ncoeff, bool sflag);

// Specific cosine windows
void rectwin(real *w, unsigned n); // order == 1
void hann(real *w, unsigned n, bool sflag = false); // order == 2
void hamming(real *w, unsigned n, bool sflag = false); // order == 2
void blackman(real *w, unsigned n, bool sflag = false); // order == 3
void blackmanharris(real *w, unsigned n, bool sflag = false); // order == 4
void nuttallwin(real *w, unsigned n, bool sflag = false); // order == 4
void flattopwin(real *w, unsigned n, bool sflag = false); // order == 5

// Periodic defaults for cosine windows
void hann_periodic(real *w, unsigned n);
void hamming_periodic(real *w, unsigned n);
void blackman_periodic(real *w, unsigned n);
void blackmanharris_periodic(real *w, unsigned n);
void nuttallwin_periodic(real *w, unsigned n);
void flattopwin_periodic(real *w, unsigned n);

// OTHER WINDOWS, NOT PARAMETERIZED
void triang(real *w, unsigned n);
void bartlett(real *w, unsigned n);
void barthannwin(real *w, unsigned n);
void bohmanwin(real *w, unsigned n);
void parzenwin(real *w, unsigned n);

// OTHER WINDOWS, PARAMETERIZED
void gausswin(real *w, unsigned n, real alpha);
void tukeywin(real *w, unsigned n, real r);
void taylorwin(real *w, unsigned n, unsigned nbar, real sll);
void kaiser(real *w, unsigned n, real beta);
