#include "DistortionPlusModel.h"
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    circuitpedal::DistortionPlusModel pedal;
    pedal.prepare(48000.0);
    pedal.setDistortion(0.80f);
    pedal.setOutput(0.80f);

    double inPeak = 0.0;
    double outPeak = 0.0;
    double sumSq = 0.0;
    constexpr int N = 48000;
    constexpr double pi = 3.14159265358979323846;

    for (int n = 0; n < N; ++n)
    {
        const float x = static_cast<float>(0.12 * std::sin(2.0 * pi * 440.0 * n / 48000.0));
        const float y = pedal.processSample(x);
        if (!std::isfinite(y))
        {
            std::cerr << "FAIL: non-finite sample at " << n << "\n";
            return 1;
        }
        inPeak = std::max(inPeak, std::abs(static_cast<double>(x)));
        outPeak = std::max(outPeak, std::abs(static_cast<double>(y)));
        sumSq += static_cast<double>(y) * static_cast<double>(y);
    }

    const double rms = std::sqrt(sumSq / N);
    std::cout << "PASS\n"
              << "input_peak=" << inPeak << "\n"
              << "output_peak=" << outPeak << "\n"
              << "output_rms=" << rms << "\n";

    return (outPeak > 0.01 && outPeak <= 1.0) ? 0 : 2;
}
