#include "WaveformSVGRenderer.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>

namespace
{
    constexpr float defaultPadding = 8.0f;

    static inline float toY (float sample, float midY, float halfHeight)
    {
        const float clamped = juce::jlimit (-1.0f, 1.0f, sample);
        return midY - (clamped * halfHeight);
    }
}

juce::String WaveformSVGRenderer::generateWaveformSVG (const juce::AudioBuffer<float>& buffer,
                                                       int numPlotPoints,
                                                       float width,
                                                       float height)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0 || numPlotPoints <= 1)
        return generateBlankWaveformSVG (width, height);

    numPlotPoints = std::max (numPlotPoints, 2);
    const int totalSamples = buffer.getNumSamples();
    const int samplesPerPoint = std::max (1, totalSamples / numPlotPoints);

    std::vector<std::pair<float, float>> minMaxPairs;
    minMaxPairs.reserve ((size_t) numPlotPoints);

    for (int start = 0; start < totalSamples; start += samplesPerPoint)
    {
        const int end = std::min (totalSamples, start + samplesPerPoint);
        float localMin = std::numeric_limits<float>::max();
        float localMax = std::numeric_limits<float>::lowest();

        for (int chan = 0; chan < buffer.getNumChannels(); ++chan)
        {
            const float* data = buffer.getReadPointer (chan);
            for (int i = start; i < end; ++i)
            {
                const float sample = data[i];
                localMin = std::min (localMin, sample);
                localMax = std::max (localMax, sample);
            }
        }

        if (localMin == std::numeric_limits<float>::max())
            localMin = 0.0f;
        if (localMax == std::numeric_limits<float>::lowest())
            localMax = 0.0f;

        minMaxPairs.emplace_back (localMin, localMax);
    }

    const float viewWidth = std::max (width, 1.0f);
    const float viewHeight = std::max (height, 1.0f);
    const float usableHeight = std::max (viewHeight - (defaultPadding * 2.0f), 1.0f);
    const float halfHeight = usableHeight / 2.0f;
    const float midY = viewHeight / 2.0f;
    const float xStep = (minMaxPairs.size() > 1)
                            ? viewWidth / (float) (minMaxPairs.size() - 1)
                            : viewWidth;

    std::ostringstream pathStream;
    pathStream.setf (std::ios::fixed);
    pathStream.precision (3);

    // Upper envelope
    pathStream << "M 0 " << toY (minMaxPairs.front().second, midY, halfHeight);
    for (size_t i = 1; i < minMaxPairs.size(); ++i)
        pathStream << " L " << (xStep * (float) i) << ' ' << toY (minMaxPairs[i].second, midY, halfHeight);

    // Lower envelope (reverse for a closed path)
    for (size_t rev = minMaxPairs.size(); rev-- > 0;)
        pathStream << " L " << (xStep * (float) rev) << ' ' << toY (minMaxPairs[rev].first, midY, halfHeight);

    pathStream << " Z";

    std::ostringstream svg;
    svg.setf (std::ios::fixed);
    svg.precision (2);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << viewWidth
        << "\" height=\"" << viewHeight << "\" viewBox=\"0 0 " << viewWidth << ' ' << viewHeight << "\" preserveAspectRatio=\"none\">";
    svg << "<path d=\"" << pathStream.str()
        << "\" fill=\"#c3c8d1\" stroke=\"#39404d\" stroke-width=\"1.6\" stroke-linejoin=\"round\" />";
    svg << "<line x1=\"0\" y1=\"" << midY << "\" x2=\"" << viewWidth
        << "\" y2=\"" << midY << "\" stroke=\"#9aa1ad\" stroke-width=\"1.1\" opacity=\"0.6\" />";
    svg << "</svg>";

    return svg.str();
}

juce::String WaveformSVGRenderer::generateBlankWaveformSVG (float width, float height)
{
    const float viewWidth = std::max (width, 1.0f);
    const float viewHeight = std::max (height, 1.0f);
    const float midY = viewHeight / 2.0f;

    std::ostringstream svg;
    svg.setf (std::ios::fixed);
    svg.precision (2);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << viewWidth
        << "\" height=\"" << viewHeight << "\" viewBox=\"0 0 " << viewWidth << ' ' << viewHeight << "\">";
    svg << "<rect x=\"0\" y=\"0\" width=\"" << viewWidth << "\" height=\"" << viewHeight
        << "\" rx=\"10\" ry=\"10\" fill=\"#e5e7ec\" />";
    svg << "<line x1=\"0\" y1=\"" << midY << "\" x2=\"" << viewWidth
        << "\" y2=\"" << midY << "\" stroke=\"#b7bdc6\" stroke-width=\"2\" stroke-dasharray=\"6 6\" />";
    svg << "<text x=\"" << (viewWidth / 2.0f) << "\" y=\"" << (midY - 10.0f)
        << "\" text-anchor=\"middle\" fill=\"#8f95a1\" font-family=\"Segoe UI, Helvetica, Arial\" font-size=\"13\">No sample</text>";
    svg << "</svg>";
    return svg.str();
}
