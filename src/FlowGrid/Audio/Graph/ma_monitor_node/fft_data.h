#pragma once

#include <fftw3.h>

struct fft_data {
    fftwf_plan plan;
    fftwf_complex *data;
};
