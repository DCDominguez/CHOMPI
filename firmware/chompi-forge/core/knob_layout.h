#pragma once
#include <cstdint>
#include "parameters.h"
#include "preset_menu.h"

// What each knob page controls, how far one click moves it, and the colour of
// the knob's light: TAPE's layout first (docs/forge/TAPE_CONTROLS.md), Forge's
// extra synth controls on the pages after it, and a patch's own knob assignment
// (v5 `knobs`) on one last page. Logical knobs 0-3 = SW4, SW1, SW2, SW3 (TAPE's
// knob order); 4 = SW5, 5 = SW6. Pure functions: the panel (audio) and the LED
// task (main loop) share them.
namespace forge {
namespace knobs {

constexpr unsigned kMaxPages = 5;

// Pages per knob before the patch page. Sampler and synth patches keep the same
// positions (TAPE's start/end become the synth's envelope). Effects-only patches
// (line in through the effects, no voices) put the delay on knobs 1-3 instead.
enum class Kind : uint8_t { Synth, Sampler, Effects };
inline Kind KindOf(const Parameters& p) { return !p.synth ? Kind::Effects : p.Sampler() ? Kind::Sampler : Kind::Synth; }
inline Parameter LayoutTarget(unsigned knob, unsigned page, Kind kind) {
    using P = Parameter;
    static const P sampler_pages[4][4] = {
        {P::Speed, P::VoiceGain, P::Resonance, P::FilterAmount},   // SW4: TAPE pitch, gain | resonance, filter env
        {P::SampleStart, P::Attack, P::LfoRate, P::Count},          // SW1: TAPE start, attack | LFO rate
        {P::SampleEnd, P::Release, P::LfoFilter, P::SampleXfade},   // SW2: TAPE end, release | LFO filter, loop crossfade
        {P::Space, P::Saturation, P::DjFilter, P::Count}};          // SW3: TAPE reverb+delay, saturation, DJ filter
    static const P synth_pages[4][4] = {
        {P::Speed, P::VoiceGain, P::Resonance, P::FilterAmount},
        {P::Attack, P::Decay, P::LfoRate, P::Count},
        {P::Release, P::Sustain, P::LfoFilter, P::Osc2Detune},
        {P::Space, P::Saturation, P::DjFilter, P::Count}};
    static const P effects_pages[4][4] = {
        {P::Mix, P::Level, P::Count, P::Count}, {P::Time, P::Count, P::Count, P::Count},
        {P::Feedback, P::Count, P::Count, P::Count}, {P::Space, P::Saturation, P::DjFilter, P::Count}};
    if(page >= 4) return P::Count;
    return (kind == Kind::Sampler ? sampler_pages : kind == Kind::Synth ? synth_pages : effects_pages)[knob & 3][page];
}
// TAPE's own pages per knob (knob_num_pages: 2, 2, 2, 3); these get TAPE's colours.
inline unsigned TapePages(unsigned knob) { return (knob & 3) == 3 ? 3 : 2; }
inline unsigned LayoutPages(unsigned knob, Kind kind) {
    static const uint8_t n[4] = {4, 3, 4, 3}, effects[4] = {2, 1, 1, 3};
    return (kind == Kind::Effects ? effects : n)[knob & 3];
}
inline bool HasPatchPage(unsigned knob, const Parameters& p) { return p.knobs[knob & 3] != 0; }
FORGE_NOINLINE inline unsigned Pages(unsigned knob, const Parameters& p) { return LayoutPages(knob, KindOf(p)) + (HasPatchPage(knob, p) ? 1 : 0); }
inline bool IsPatchPage(unsigned knob, unsigned page, const Parameters& p) {
    return HasPatchPage(knob, p) && page == LayoutPages(knob, KindOf(p));
}
FORGE_NOINLINE inline Parameter Target(unsigned knob, unsigned page, const Parameters& p) {
    if(IsPatchPage(knob, page, p)) return p.KnobParameter(knob & 3);
    const Kind kind = KindOf(p);
    return LayoutTarget(knob, page < LayoutPages(knob, kind) ? page : 0, kind);
}

// One click. TAPE: SW4 and SW5 count 1 per click, the others 3; coarse controls
// move .01 per count, sample start/end and the free pitch .003 (so .03 per click on
// SW1-SW3 and SW6, .009 for start/end, .003 for pitch).
inline float Step(unsigned logical_knob, Parameter target) {
    const float counts = logical_knob == 0 || logical_knob == 4 ? 1.f : 3.f;
    const bool fine = target == Parameter::SampleStart || target == Parameter::SampleEnd || target == Parameter::Speed;
    return counts * (fine ? .003f : .01f);
}
constexpr float kLoopSpeedPerClick = 4.f * .003f;   // TAPE: speed v*4-2, fine step

// TAPE NormalPage colours.
namespace colour {
constexpr Rgb white{1.f, 1.f, 1.f}, red{1.f, 0.f, 0.f}, orange{1.f, .6f, .24f}, yellow{1.f, .95f, .05f},
    green{0.f, 1.f, 0.f}, teal{.14f, 1.f, .92f}, med_blue{0.f, .84f, 1.f}, blue{0.f, 0.f, 1.f},
    purple{.58f, .05f, 1.f}, pink{1.f, .36f, .62f};
}
inline Rgb Fade(Rgb a, Rgb b, float t) { return {a.r + t * (b.r - a.r), a.g + t * (b.g - a.g), a.b + t * (b.b - a.b)}; }
inline Rgb Fade3(Rgb a, Rgb b, Rgb c, float t) { return t < .5f ? Fade(a, b, t * 2.f) : Fade(b, c, (t - .5f) * 2.f); }
inline Rgb Fade4(Rgb a, Rgb b, Rgb c, Rgb d, float t) {
    return t < .33f ? Fade(a, b, t * 3.f) : t < .66f ? Fade(b, c, (t - .33f) * 3.f) : Fade(c, d, (t - .66f) * 3.f);
}
inline Rgb Scale(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
// 0.16: one colour rule for every list on the panel: the n-th choice (or page) is always
// the same colour, white first: white, green, yellow, orange, red, pink, purple, blue, teal.
inline Rgb StepColour(unsigned step) {
    using namespace colour;
    static const Rgb steps[9] = {white, green, yellow, orange, red, pink, purple, blue, teal};
    return steps[step % 9];
}

// A knob's light for its page and value (TAPE pages: TAPE's value colours;
// Forge's extra pages: red then green; the patch page and the effects-only delay
// knobs: dim to bright white).
FORGE_COLD inline Rgb KnobColour(unsigned knob, unsigned page, float value, bool patch_page, bool effects = false) {
    using namespace colour;
    value = Clamp(value, 0.f, 1.f);
    if(patch_page || (effects && (knob & 3) != 3)) return Scale(white, .12f + .5f * value);
    if(page >= TapePages(knob)) return page == TapePages(knob) ? Scale(red, .2f + .8f * value) : Scale(green, .2f + .8f * value);
    switch(knob & 3) {
        case 0: return page == 0 ? Fade4(med_blue, green, yellow, red, value < .5f ? value * 2.f : (1.f - value) * 2.f)
                                 : Fade3(blue, pink, red, value);
        case 1: return page == 0 ? Fade(yellow, orange, value) : Fade(Scale(purple, .2f), purple, value);
        case 2: return page == 0 ? Fade(orange, red, value) : Fade(Scale(purple, .2f), purple, value);
        default: return page == 0 ? Fade3(teal, med_blue, blue, value) : page == 1 ? Fade3(yellow, orange, red, value)
                                  : Fade3(purple, pink, white, value);
    }
}
// SW6: volume (page 0, output level shown dim to bright green) or input gain (page 1, TAPE: blue to red).
inline Rgb VolumeColour(bool input_page, float value) {
    using namespace colour;
    return input_page ? Fade(blue, red, Clamp(value, 0.f, 1.f)) : Scale(green, .1f + .9f * Clamp(value, 0.f, 1.f));
}
} // namespace knobs
} // namespace forge
