/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#include "FFTwrapper.h"

#include <assert.h>
#include <iostream>
using namespace std;

template<typename Type>	std::complex<Type> make_complex(Type value[]){return std::complex<Type>(value[0], value[1]);}

template<typename Type>	std::complex<Type> make_complex(Type real, Type imag){return std::complex<Type>(real, imag);}

FFTwrapper::FFTwrapper(bool forward)
{
	m_size = 0;
    m_forward = forward;

    #ifdef FFT_FFTW3
        m_fftw3_out = m_fftw3_in = NULL;
        m_fftw3_plan = NULL;
    #elif FFT_FFTREAL
        m_fftreal_out = NULL;
        m_fftreal_fft = NULL;
    #endif
}
FFTwrapper::FFTwrapper(int n)
{
	resize(n);
}
void FFTwrapper::resize(int n)
{
	assert(n>0);

	m_size = n;

    #ifdef FFT_FFTW3
        delete[] m_fftw3_in;
        delete[] m_fftw3_out;
        m_fftw3_out = m_fftw3_in = NULL;

        m_fftw3_in = new fftw_complex[m_size];
        m_fftw3_out = new fftw_complex[m_size];

        //  | FFTW_PRESERVE_INPUT
        if(m_forward)
            m_fftw3_plan = fftw_plan_dft_1d(m_size, m_fftw3_in, m_fftw3_out, FFTW_FORWARD, FFTW_MEASURE);
        else
            m_fftw3_plan = fftw_plan_dft_1d(m_size, m_fftw3_in, m_fftw3_out, FFTW_BACKWARD, FFTW_MEASURE);

    #elif FFT_FFTREAL
        delete[] m_fftreal_out;
        m_fftreal_out = NULL;

        m_fftreal_fft = new ffft::FFTReal<double>(m_size);
        m_fftreal_out = new double[m_size];
    #endif

    in.resize(m_size);
    out.resize(m_size);
}

void FFTwrapper::execute()
{
//    std::cout << "FFTwrapper::execute()" << endl;
    execute(in, out);
}

void FFTwrapper::execute(const vector<double>& in, vector<std::complex<double> >& out)
{
//    std::cout << "FFTwrapper::execute " << m_size << endl;

    assert(int(in.size())>=m_size);

    #ifdef FFT_FFTW3
        for(int i=0; i<m_size; i++)
        {
            m_fftw3_in[i][0] = in[i];
            m_fftw3_in[i][1] = 0.0;
        }

        fftw_execute(m_fftw3_plan);

        if(int(out.size())<m_size)
            out.resize(m_size);

        for(int i=0; i<m_size; i++)
            out[i] = make_complex(m_fftw3_out[i]);

    #elif FFT_FFTREAL
        if (m_forward) // DFT
            m_fftreal_fft->do_fft(m_fftreal_out, &(in[0]));
        else           // IDFT
            assert(false); // TODO
//std::cout << "FFTwrapper::execute do done" << endl;

        out[0] = m_fftreal_out[0]; // DC
        for(int f=1; f<m_size/2; f++)
            out[f] = make_complex(m_fftreal_out[f], m_fftreal_out[m_size/2+f]);
        out[m_size/2] = m_fftreal_out[m_size/2]; // Nyquist
//std::cout << "FFTwrapper::execute copy done" << endl;

//std::cout << "FFTwrapper::execute delete done" << endl;
    #endif
}

FFTwrapper::~FFTwrapper()
{
    #ifdef FFT_FFTW3
        if(m_fftw3_plan)	fftw_destroy_plan(m_fftw3_plan);
        if(m_fftw3_in)	delete[] m_fftw3_in;
        if(m_fftw3_out)	delete[] m_fftw3_out;
    #elif FFT_FFTREAL
        if(m_fftreal_fft)	delete m_fftreal_fft;
        if(m_fftreal_out)	delete[] m_fftreal_out;
    #endif
}