#pragma once

#include <juce_core/juce_core.h>
#include <functional>

namespace ml
{
enum class Language { chinese = 0, english = 1 };

/** Runtime UI language with a single global switch.

    All user facing text goes through T(zh, en) so the whole editor can be
    re-rendered when the language changes.
*/
class Localisation
{
public:
    static Language get() noexcept { return current().load (std::memory_order_relaxed) == 1 ? Language::english
                                                                                            : Language::chinese; }

    static bool isEnglish() noexcept { return get() == Language::english; }

    static void set (Language l)
    {
        current().store (l == Language::english ? 1 : 0, std::memory_order_relaxed);

        if (auto cb = callback())
            if (*cb)
                (*cb)();
    }

    static juce::String tr (const char* zh, const char* en)
    {
        return juce::String::fromUTF8 (isEnglish() ? en : zh);
    }

    /** Replaces "{name}" tokens in a localised sentence. */
    static juce::String fill (const juce::String& text,
                              std::initializer_list<std::pair<const char*, juce::String>> vars)
    {
        auto s = text;

        for (const auto& v : vars)
            s = s.replace (juce::String ("{") + v.first + "}", v.second);

        return s;
    }

    static void setOnChange (std::function<void()> fn)
    {
        if (auto cb = callback())
            *cb = std::move (fn);
    }

    static void clearOnChange()
    {
        if (auto cb = callback())
            *cb = {};
    }

private:
    static std::atomic<int>& current()
    {
        static std::atomic<int> value { 0 };
        return value;
    }

    static std::shared_ptr<std::function<void()>>& callback()
    {
        static std::shared_ptr<std::function<void()>> fn = std::make_shared<std::function<void()>>();
        return fn;
    }
};

/** Localised string literal. */
inline juce::String T (const char* zh, const char* en)
{
    return Localisation::tr (zh, en);
}

inline juce::String TF (const juce::String& text,
                        std::initializer_list<std::pair<const char*, juce::String>> vars)
{
    return Localisation::fill (text, vars);
}
} // namespace ml
