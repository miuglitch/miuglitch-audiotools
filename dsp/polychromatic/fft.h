#pragma once
#include <cmath>
#include <complex>
#include <vector>

namespace GlitchCore
{

//==============================================================================
// Small iterative radix-2 complex FFT. Allocates only in prepare().
// Pure C++, no JUCE dependency.
//==============================================================================

class RadixTwoFFT
{
public:
    using Complex = std::complex<float>;

    void prepare (int order)
    {
        order_ = order;
        size_  = 1 << order;
        twiddles_.resize ((size_t) size_ / 2);
        for (int i = 0; i < size_ / 2; ++i)
        {
            const double a = -2.0 * 3.14159265358979323846 * (double) i / (double) size_;
            twiddles_[(size_t) i] = Complex ((float) std::cos (a), (float) std::sin (a));
        }
        bitrev_.resize ((size_t) size_);
        for (int i = 0; i < size_; ++i)
        {
            int r = 0;
            for (int b = 0; b < order_; ++b)
                if (i & (1 << b)) r |= 1 << (order_ - 1 - b);
            bitrev_[(size_t) i] = r;
        }
    }

    int getSize() const { return size_; }

    // In-place transform. inverse = true computes the unscaled inverse.
    void perform (Complex* data, bool inverse) const
    {
        for (int i = 0; i < size_; ++i)
        {
            const int j = bitrev_[(size_t) i];
            if (j > i) std::swap (data[i], data[j]);
        }

        for (int len = 2; len <= size_; len <<= 1)
        {
            const int half   = len >> 1;
            const int stride = size_ / len;
            for (int start = 0; start < size_; start += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    Complex w = twiddles_[(size_t) (k * stride)];
                    if (inverse) w = std::conj (w);
                    const Complex a = data[start + k];
                    const Complex b = data[start + k + half] * w;
                    data[start + k]        = a + b;
                    data[start + k + half] = a - b;
                }
            }
        }
    }

private:
    int order_ = 0;
    int size_  = 0;
    std::vector<Complex> twiddles_;
    std::vector<int>     bitrev_;
};

} // namespace GlitchCore
