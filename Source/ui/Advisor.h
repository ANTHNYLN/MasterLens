#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

#include "../dsp/AnalysisEngine.h"
#include "../dsp/DefectAnalyzer.h"
#include "../Basis.h"

namespace ml
{
enum class Severity { good = 0, info = 1, warn = 2, critical = 3 };

struct Issue
{
    juce::String id;
    juce::String title;
    juce::String detail;
    Severity severity = Severity::info;
    double detectedAt = 0.0;
    bool timed = false;
};

/** Rule based mix / mastering advisor.

    Every rule is evaluated at UI frame rate; a small hysteresis counter keeps the
    list from flickering on borderline material.
*/
class Advisor
{
public:
    void reset();

    void update (const StatsSnapshot& stats,
                 const std::vector<float>& bandDeviations,
                 float targetLufs,
                 float ceilingDb,
                 const SignalDefects& defects,
                 const BasisContext& basis,
                 bool hasTonalBasis);

    const std::vector<Issue>& getIssues() const noexcept { return visible; }

    Severity getOverallSeverity() const noexcept { return overall; }
    juce::String getHeadline() const;

    int count (Severity s) const noexcept
    {
        int n = 0;

        for (const auto& i : visible)
            if (i.severity == s)
                ++n;

        return n;
    }

private:
    struct Condition
    {
        Issue issue;
        int onTicks = 0;
        int offTicks = 0;
        double firstSeen = 0.0;
    };

    void apply (const std::vector<Issue>& found, const StatsSnapshot& stats);

    std::map<juce::String, Condition> conditions;
    std::vector<Issue> visible;
    Severity overall = Severity::good;
    int frame = 0;
};
} // namespace ml
