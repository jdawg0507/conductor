#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/NamChain.h"

namespace conductor {

// Gear-specific knob definitions: label, atomicField, min, max, step, unit
struct KnobDef {
    std::string label;
    int fieldIndex; // 0=gainDb, 1=outputLevelDb, 2=mix, 3=trebleDb, 4=midDb, 5=bassDb
    float rangeMin;
    float rangeMax;
    float step;
    std::string unit; // "dB", "%", ""
    bool isPercentage{false}; // If true, display value * 100 with % suffix
};

inline std::vector<KnobDef> getKnobsForGear(const GearSlotConfig& cfg) {
    std::string name = cfg.name;
    
    // ── AMPS: 5 knobs ──
    if (cfg.type == GearType::Amp) {
        return {
            {"GAIN",     0, -18.0f, 18.0f, 0.1f, "dB", false},
            {"BASS",     5, -12.0f, 12.0f, 0.1f, "dB", false},
            {"MID",      4, -12.0f, 12.0f, 0.1f, "dB", false},
            {"TREBLE",   3, -12.0f, 12.0f, 0.1f, "dB", false},
            {"OUTPUT",   1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // ── SPACES: 2 knobs ──
    if (cfg.type == GearType::Space) {
        return {
            {"MIX",    2, 0.0f, 1.0f, 0.01f, "%", true},
            {"OUTPUT", 1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // ── STOMPBOXES: gear-specific knobs ──

    // ISP Decimator (Noise Gate)
    if (name.find("Decimator") != std::string::npos || name.find("decimator") != std::string::npos) {
        return {
            {"THRESHOLD", 0, -60.0f, 0.0f, 0.5f, "dB", false},
            {"DECAY",     2, 0.0f, 1.0f, 0.01f, "", true},  // Mix doubles as decay control
            {"OUTPUT",    1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // Tube Screamer / Klon (Overdrive)
    if (name.find("Screamer") != std::string::npos || name.find("TS9") != std::string::npos ||
        name.find("Klon") != std::string::npos || name.find("Centaur") != std::string::npos) {
        return {
            {"DRIVE",  0, -6.0f, 24.0f, 0.1f, "dB", false},
            {"TONE",   3, -12.0f, 12.0f, 0.1f, "dB", false},
            {"LEVEL",  1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // Big Muff (Fuzz)
    if (name.find("Muff") != std::string::npos) {
        return {
            {"SUSTAIN", 0, -6.0f, 24.0f, 0.1f, "dB", false},
            {"TONE",    3, -12.0f, 12.0f, 0.1f, "dB", false},
            {"VOLUME",  1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // ProCo Rat (Distortion)
    if (name.find("Rat") != std::string::npos && name.find("ProCo") != std::string::npos) {
        return {
            {"DISTORTION", 0, -6.0f, 24.0f, 0.1f, "dB", false},
            {"FILTER",     3, -12.0f, 12.0f, 0.1f, "dB", false},
            {"VOLUME",     1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // Boss CE-2 (Chorus)
    if (name.find("CE-2") != std::string::npos || name.find("Chorus") != std::string::npos) {
        return {
            {"RATE",   0, 0.0f, 10.0f, 0.1f, "", false},
            {"DEPTH",  2, 0.0f, 1.0f, 0.01f, "%", true},
            {"OUTPUT", 1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // MXR Phase 90 (Phaser)
    if (name.find("Phase") != std::string::npos) {
        return {
            {"SPEED",  0, 0.0f, 10.0f, 0.1f, "", false},
            {"DEPTH",  2, 0.0f, 1.0f, 0.01f, "%", true},
            {"OUTPUT", 1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // Space Echo / RE-201 (Delay)
    if (name.find("Echo") != std::string::npos || name.find("RE-201") != std::string::npos) {
        return {
            {"REPEATS",  0, 0.0f, 10.0f, 0.1f, "", false},
            {"ECHO VOL", 2, 0.0f, 1.0f, 0.01f, "%", true},
            {"OUTPUT",   1, -36.0f, 12.0f, 0.1f, "dB", false},
        };
    }
    
    // Dyna Comp (Compressor)
    if (name.find("Dyna") != std::string::npos || name.find("Comp") != std::string::npos) {
        return {
            {"SENSITIVITY", 0, 0.0f, 24.0f, 0.1f, "dB", false},
            {"OUTPUT",      1, -36.0f, 12.0f, 0.1f, "dB", false},
            {"ATTACK",      2, 0.0f, 1.0f, 0.01f, "%", true},
        };
    }
    
    // Generic fallback: DRIVE / OUTPUT / MIX
    return {
        {"DRIVE",  0, -24.0f, 24.0f, 0.1f, "dB", false},
        {"OUTPUT", 1, -36.0f, 12.0f, 0.1f, "dB", false},
        {"MIX",    2, 0.0f, 1.0f, 0.01f, "%", true},
    };
}

inline std::atomic<float>& getAtomicField(GearSlotConfig& cfg, int fieldIndex) {
    switch (fieldIndex) {
        case 0: return cfg.gainDb;
        case 1: return cfg.outputLevelDb;
        case 2: return cfg.mix;
        case 3: return cfg.trebleDb;
        case 4: return cfg.midDb;
        case 5: return cfg.bassDb;
        default: return cfg.gainDb;
    }
}

class PedalCardComponent : public juce::Component {
public:
    static constexpr int MAX_KNOBS = 5;

    PedalCardComponent(GearSlotConfig& slotConfig) : mConfig(slotConfig) {
        for (int i = 0; i < MAX_KNOBS; ++i) {
            mSliders[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            mSliders[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addChildComponent(mSliders[i]);
        }

        mBypassButton.setButtonText("ENGAGE");
        mBypassButton.onClick = [this]() {
            bool current = mConfig.isBypassed.load();
            mConfig.isBypassed.store(!current);
            repaint();
        };
        addAndMakeVisible(mBypassButton);

        rebuildKnobs();
    }

    void refreshFromConfig() {
        rebuildKnobs();
        repaint();
    }

    void rebuildKnobs() {
        mKnobDefs = getKnobsForGear(mConfig);
        mActiveKnobs = static_cast<int>(std::min(mKnobDefs.size(), (size_t)MAX_KNOBS));

        for (int i = 0; i < MAX_KNOBS; ++i) {
            if (i < mActiveKnobs) {
                auto& def = mKnobDefs[i];
                auto& field = getAtomicField(mConfig, def.fieldIndex);
                mSliders[i].setRange(def.rangeMin, def.rangeMax, def.step);
                mSliders[i].setValue(field.load(), juce::dontSendNotification);
                // Capture fieldIndex by value for the callback
                int fi = def.fieldIndex;
                mSliders[i].onValueChange = [this, fi]() {
                    // Find the slider index that triggered this
                    for (int k = 0; k < mActiveKnobs; ++k) {
                        if (mKnobDefs[k].fieldIndex == fi) {
                            getAtomicField(mConfig, fi).store(static_cast<float>(mSliders[k].getValue()));
                            break;
                        }
                    }
                };
                mSliders[i].setVisible(true);
            } else {
                mSliders[i].setVisible(false);
            }
        }

        resized();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        bool bypassed = mConfig.isBypassed.load();
        bool isEmpty = mConfig.name == "Empty";

        // Determine finish color palette based on gear type and personality
        juce::Colour chassisTop(0xff1e2530);
        juce::Colour chassisBottom(0xff0d1117);
        juce::Colour accentNeon(0xff00f0ff);
        juce::Colour jewelGlow(0xffef4444);

        if (mConfig.name.find("TS9") != std::string::npos || mConfig.name.find("Screamer") != std::string::npos) {
            chassisTop = juce::Colour(0xff144225);
            chassisBottom = juce::Colour(0xff081c10);
            accentNeon = juce::Colour(0xff22c55e);
            jewelGlow = juce::Colour(0xffef4444);
        } else if (mConfig.name.find("Klon") != std::string::npos) {
            chassisTop = juce::Colour(0xff573a15);
            chassisBottom = juce::Colour(0xff291a08);
            accentNeon = juce::Colour(0xfff59e0b);
            jewelGlow = juce::Colour(0xfff59e0b);
        } else if (mConfig.name.find("Muff") != std::string::npos) {
            chassisTop = juce::Colour(0xff2d3748);
            chassisBottom = juce::Colour(0xff171d26);
            accentNeon = juce::Colour(0xffe2e8f0);
            jewelGlow = juce::Colour(0xffef4444);
        } else if (mConfig.name.find("Rat") != std::string::npos) {
            chassisTop = juce::Colour(0xff18181b);
            chassisBottom = juce::Colour(0xff09090b);
            accentNeon = juce::Colour(0xffef4444);
            jewelGlow = juce::Colour(0xffef4444);
        } else if (mConfig.name.find("CE-2") != std::string::npos) {
            chassisTop = juce::Colour(0xff034373);
            chassisBottom = juce::Colour(0xff011b30);
            accentNeon = juce::Colour(0xff38bdf8);
            jewelGlow = juce::Colour(0xff38bdf8);
        } else if (mConfig.name.find("Phase") != std::string::npos) {
            chassisTop = juce::Colour(0xff6c2409);
            chassisBottom = juce::Colour(0xff310f03);
            accentNeon = juce::Colour(0xfff97316);
            jewelGlow = juce::Colour(0xffef4444);
        } else if (mConfig.name.find("Decimator") != std::string::npos) {
            chassisTop = juce::Colour(0xff1a1a2e);
            chassisBottom = juce::Colour(0xff0a0a14);
            accentNeon = juce::Colour(0xff10b981);
            jewelGlow = juce::Colour(0xff10b981);
        } else if (mConfig.name.find("Comp") != std::string::npos || mConfig.name.find("Dyna") != std::string::npos) {
            chassisTop = juce::Colour(0xff2e1a3a);
            chassisBottom = juce::Colour(0xff140a1c);
            accentNeon = juce::Colour(0xffa855f7);
            jewelGlow = juce::Colour(0xffa855f7);
        } else if (mConfig.name.find("Echo") != std::string::npos || mConfig.name.find("RE-201") != std::string::npos) {
            chassisTop = juce::Colour(0xff2e2a1a);
            chassisBottom = juce::Colour(0xff141208);
            accentNeon = juce::Colour(0xffeab308);
            jewelGlow = juce::Colour(0xffeab308);
        } else if (mConfig.type == GearType::Amp) {
            chassisTop = juce::Colour(0xff2b241b);
            chassisBottom = juce::Colour(0xff14100c);
            accentNeon = juce::Colour(0xfffbbf24);
            jewelGlow = juce::Colour(0xfffbbf24);
        } else if (mConfig.type == GearType::Space) {
            chassisTop = juce::Colour(0xff141e2e);
            chassisBottom = juce::Colour(0xff090e17);
            accentNeon = juce::Colour(0xff818cf8);
            jewelGlow = juce::Colour(0xff818cf8);
        }

        // 1. Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 4.0f), 12.0f);

        // 2. Main CNC Chassis Body
        juce::ColourGradient bodyGrad(chassisTop, bounds.getX(), bounds.getY(),
                                      chassisBottom, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(bounds, 12.0f);

        // 3. Subtle Milled Edge Bevel
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.2f);
        g.setColour(juce::Colour(0xff090c10));
        g.drawRoundedRectangle(bounds.reduced(1.8f), 10.5f, 1.0f);

        // 4. Corner Hex Bolts
        auto drawHexBolt = [&g](float cx, float cy) {
            g.setColour(juce::Colour(0xff0b0e14));
            g.fillEllipse(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
            juce::ColourGradient boltGrad(juce::Colour(0xff64748b), cx - 3.5f, cy - 3.5f,
                                          juce::Colour(0xff1e293b), cx + 3.5f, cy + 3.5f, false);
            g.setGradientFill(boltGrad);
            g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
            g.setColour(juce::Colour(0xff090c10));
            g.fillEllipse(cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
        };
        drawHexBolt(bounds.getX() + 10.0f, bounds.getY() + 10.0f);
        drawHexBolt(bounds.getRight() - 10.0f, bounds.getY() + 10.0f);
        drawHexBolt(bounds.getX() + 10.0f, bounds.getBottom() - 10.0f);
        drawHexBolt(bounds.getRight() - 10.0f, bounds.getBottom() - 10.0f);

        // 5. Header Engraved Nameplate
        auto headerBounds = bounds;
        auto headerArea = headerBounds.removeFromTop(54.0f).reduced(16.0f, 8.0f);
        g.setColour(juce::Colour(0xff090d14).withAlpha(0.85f));
        g.fillRoundedRectangle(headerArea, 6.0f);
        g.setColour(accentNeon.withAlpha(0.40f));
        g.drawRoundedRectangle(headerArea, 6.0f, 1.0f);

        // Type Badge
        std::string typeLabel = "STOMPBOX";
        if (mConfig.type == GearType::Amp) typeLabel = "VACUUM TUBE AMPLIFIER";
        else if (mConfig.type == GearType::Space) typeLabel = "STUDIO SPACE & REVERB";

        g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
        g.setColour(accentNeon);
        g.drawText(typeLabel, headerArea.removeFromTop(13.0f), juce::Justification::centred);

        // Model Name
        g.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
        g.setColour(isEmpty ? juce::Colour(0xff64748b) : juce::Colours::white);
        g.drawText(mConfig.name, headerArea, juce::Justification::centred);

        // 6. Faceted 3D Jewel Pilot Lamp
        float jewelX = bounds.getCentreX();
        float jewelY = bounds.getY() + 72.0f;
        float jewelRadius = 8.0f;

        g.setColour(juce::Colour(0xff0f141c));
        g.fillEllipse(jewelX - 12.0f, jewelY - 12.0f, 24.0f, 24.0f);
        juce::ColourGradient chromeGrad(juce::Colour(0xff94a3b8), jewelX - 10.0f, jewelY - 10.0f,
                                        juce::Colour(0xff334155), jewelX + 10.0f, jewelY + 10.0f, false);
        g.setGradientFill(chromeGrad);
        g.drawEllipse(jewelX - 10.0f, jewelY - 10.0f, 20.0f, 20.0f, 2.0f);

        if (!bypassed && !isEmpty) {
            g.setColour(jewelGlow.withAlpha(0.35f));
            g.fillEllipse(jewelX - 18.0f, jewelY - 18.0f, 36.0f, 36.0f);
            g.setColour(jewelGlow.withAlpha(0.65f));
            g.fillEllipse(jewelX - 12.0f, jewelY - 12.0f, 24.0f, 24.0f);
            juce::ColourGradient jewelGrad(juce::Colours::white, jewelX - 2.0f, jewelY - 2.0f,
                                           jewelGlow, jewelX + jewelRadius, jewelY + jewelRadius, true);
            g.setGradientFill(jewelGrad);
            g.fillEllipse(jewelX - jewelRadius, jewelY - jewelRadius, jewelRadius * 2.0f, jewelRadius * 2.0f);
        } else {
            juce::ColourGradient offGrad(juce::Colour(0xff334155), jewelX - 4.0f, jewelY - 4.0f,
                                         juce::Colour(0xff090c10), jewelX + jewelRadius, jewelY + jewelRadius, false);
            g.setGradientFill(offGrad);
            g.fillEllipse(jewelX - jewelRadius, jewelY - jewelRadius, jewelRadius * 2.0f, jewelRadius * 2.0f);
        }

        // 7. Gear-specific knob labels and values
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        g.setColour(juce::Colour(0xff94a3b8));

        for (int i = 0; i < mActiveKnobs; ++i) {
            auto sliderBounds = mSliders[i].getBounds();
            auto& def = mKnobDefs[i];

            // Label
            g.setColour(juce::Colour(0xff94a3b8));
            g.drawText(def.label, sliderBounds.getX() - 6, sliderBounds.getBottom() + 1,
                       sliderBounds.getWidth() + 12, 12, juce::Justification::centred);

            // Value
            g.setColour(accentNeon.withAlpha(0.85f));
            g.setFont(juce::FontOptions(7.5f));
            juce::String valStr;
            if (def.isPercentage) {
                valStr = juce::String(static_cast<int>(mSliders[i].getValue() * 100.0)) + "%";
            } else if (def.unit == "dB") {
                valStr = juce::String(mSliders[i].getValue(), 1) + " dB";
            } else {
                valStr = juce::String(mSliders[i].getValue(), 1);
            }
            g.drawText(valStr, sliderBounds.getX() - 6, sliderBounds.getBottom() + 12,
                       sliderBounds.getWidth() + 12, 11, juce::Justification::centred);
            g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        }

        // 8. Stomp switch housing
        auto footBounds = mBypassButton.getBounds().toFloat();
        g.setColour(juce::Colour(0xff090c10));
        g.fillRoundedRectangle(footBounds.expanded(3.0f), 8.0f);
        g.setColour(juce::Colour(0xff2d3748));
        g.drawRoundedRectangle(footBounds.expanded(3.0f), 8.0f, 1.0f);
    }

    void resized() override {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(88);

        // Knob area — dynamically sized per number of active knobs
        auto knobArea = bounds.removeFromTop(62).reduced(2, 0);
        if (mActiveKnobs > 0) {
            int knobW = knobArea.getWidth() / mActiveKnobs;
            for (int i = 0; i < mActiveKnobs; ++i) {
                mSliders[i].setBounds(knobArea.getX() + i * knobW, knobArea.getY(), knobW, 46);
            }
        }

        // Bottom Section: Heavy Duty Stomp Switch
        auto footArea = bounds.removeFromBottom(42).reduced(22, 6);
        mBypassButton.setBounds(footArea);
        mBypassButton.setButtonText(mConfig.isBypassed.load() ? "BYPASSED" : "ACTIVE");
    }

private:
    GearSlotConfig& mConfig;
    juce::Slider mSliders[MAX_KNOBS];
    juce::TextButton mBypassButton;
    std::vector<KnobDef> mKnobDefs;
    int mActiveKnobs{3};
};

} // namespace conductor
