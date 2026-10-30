#pragma once

#include "Localisation.h"
#include "Theme.h"

namespace ml
{
/** Shared wording for the four status levels used by the rail and cards. */
inline juce::String statusLabel (int level)
{
    switch (level)
    {
        case 3:  return T ("超标", "Over");
        case 2:  return T ("警告", "Warning");
        case 1:  return T ("注意", "Info");
        case 0:  return T ("合格", "OK");
        default: return T ("待测", "Idle");
    }
}

inline juce::Colour statusColour (int level)
{
    switch (level)
    {
        case 3:  return theme::bad;
        case 2:  return theme::warn;
        case 1:  return theme::info;
        case 0:  return theme::good;
        default: return theme::textFaint;
    }
}
} // namespace ml
