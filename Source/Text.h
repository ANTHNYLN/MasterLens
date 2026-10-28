#pragma once

#include <juce_core/juce_core.h>

namespace ml
{
/** Builds a juce::String from a UTF-8 literal.

    juce::String's const char* constructor treats the bytes as ASCII, which turns
    any non-ASCII (e.g. Chinese) literal into mojibake, so all localised literals
    must go through this helper.
*/
inline juce::String U8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}
} // namespace ml
