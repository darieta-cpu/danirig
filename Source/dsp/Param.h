#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

namespace dr
{
// Parametro atomico: scritto dalla UI, letto dal thread audio senza lock.
struct Param
{
    enum class Kind { Float, Toggle, Choice };

    Param (const juce::String& id_, const juce::String& name_, float mn, float mx, float df,
           const juce::String& suffix_, Kind k, int decimals_)
        : id (id_), name (name_), suffix (suffix_), minV (mn), maxV (mx), defV (df), kind (k),
          decimals (decimals_), v (df) {}

    float get() const noexcept   { return v.load (std::memory_order_relaxed); }
    bool  on()  const noexcept   { return get() > 0.5f; }
    int   index() const noexcept { return (int) std::lround (get()); }
    void  set (float x) noexcept { v.store (juce::jlimit (minV, maxV, x), std::memory_order_relaxed); }
    void  reset() noexcept       { set (defV); }

    juce::String id, name, suffix;
    float minV, maxV, defV;
    Kind kind;
    int decimals;
    juce::StringArray choices;
    std::atomic<float> v;
};

class ParamOwner
{
public:
    Param& addFloat (const juce::String& id, const juce::String& name, float mn, float mx, float df,
                     const juce::String& suffix = {}, int decimals = 1)
    {
        params.push_back (std::make_unique<Param> (id, name, mn, mx, df, suffix, Param::Kind::Float, decimals));
        return *params.back();
    }
    Param& addToggle (const juce::String& id, const juce::String& name, bool df)
    {
        params.push_back (std::make_unique<Param> (id, name, 0.0f, 1.0f, df ? 1.0f : 0.0f, juce::String(),
                                                   Param::Kind::Toggle, 0));
        return *params.back();
    }
    Param& addChoice (const juce::String& id, const juce::String& name, const juce::StringArray& choices, int df)
    {
        params.push_back (std::make_unique<Param> (id, name, 0.0f, (float) (choices.size() - 1), (float) df,
                                                   juce::String(), Param::Kind::Choice, 0));
        params.back()->choices = choices;
        return *params.back();
    }
    Param* find (const juce::String& id) const
    {
        for (auto& p : params) if (p->id == id) return p.get();
        return nullptr;
    }

    std::vector<std::unique_ptr<Param>> params;
};
} // namespace dr
