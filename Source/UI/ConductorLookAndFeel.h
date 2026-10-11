#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace conductor {

class ConductorLookAndFeel : public juce::LookAndFeel_V4 {
public:
    ConductorLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff090c10));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff161b24));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0e131a));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xfff8fafc));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1e293b));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00f0ff));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float cornerRadius = 6.0f;

        // Button shadow
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), cornerRadius);

        // Metallic gradient
        juce::Colour base = backgroundColour;
        if (shouldDrawButtonAsDown) {
            base = base.darker(0.3f);
        } else if (shouldDrawButtonAsHighlighted) {
            base = base.brighter(0.2f);
        }

        juce::ColourGradient grad(base.brighter(0.12f), bounds.getX(), bounds.getY(),
                                  base.darker(0.25f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, cornerRadius);

        // High precision milled border with neon accent when highlighted
        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
            g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.6f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.2f);
        } else {
            g.setColour(juce::Colour(0xff334155).withAlpha(0.6f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, 1.0f);
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // 1. Recessed Dial Well (Cast shadow inside chassis)
        g.setColour(juce::Colour(0xff080a0e));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // 2. Track Well Groove (Dark background arc)
        float trackRadius = radius * 0.90f;
        juce::Path backgroundTrack;
        backgroundTrack.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff141923));
        g.strokePath(backgroundTrack, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Glowing Neon Arc (Dual-pass Bloom: Diffuse glow + sharp vibrant core)
        if (sliderPosProportional > 0.005f) {
            juce::Path valueArc;
            valueArc.addCentredArc(centre.x, centre.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, toAngle, true);

            // Ambient Glow
            g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.28f));
            g.strokePath(valueArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Razor-sharp Neon Core
            g.setColour(juce::Colour(0xff38bdf8));
            g.strokePath(valueArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 4. Knurled Grip Teeth (Outer ring)
        float knobRadius = radius * 0.74f;
        int numTeeth = 32;
        g.setColour(juce::Colour(0xff12161f));
        for (int i = 0; i < numTeeth; ++i) {
            float angle = (i * juce::MathConstants<float>::twoPi) / numTeeth;
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);
            float x1 = centre.x + (knobRadius + 2.0f) * cosA;
            float y1 = centre.y + (knobRadius + 2.0f) * sinA;
            float x2 = centre.x + (knobRadius - 2.0f) * cosA;
            float y2 = centre.y + (knobRadius - 2.0f) * sinA;
            g.drawLine(x1, y1, x2, y2, 1.2f);
        }

        // 5. Machined Aluminum Anodized Body
        juce::ColourGradient knobGrad(juce::Colour(0xff333d4e), centre.x - knobRadius, centre.y - knobRadius,
                                      juce::Colour(0xff161a22), centre.x + knobRadius, centre.y + knobRadius, false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

        // 6. Polished Metallic Rim Bevel (Specular Highlight)
        juce::ColourGradient rimGrad(juce::Colour(0xff64748b).withAlpha(0.7f), centre.x, centre.y - knobRadius,
                                     juce::Colour(0xff0f172a).withAlpha(0.8f), centre.x, centre.y + knobRadius, false);
        g.setGradientFill(rimGrad);
        g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.5f);

        // 7. Inner Recessed Cap with Radial Brushed Sheen
        float capRadius = knobRadius * 0.72f;
        juce::ColourGradient capGrad(juce::Colour(0xff1a202c), centre.x, centre.y - capRadius,
                                     juce::Colour(0xff0d1117), centre.x, centre.y + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
        g.setColour(juce::Colour(0xff2d3748));
        g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // 8. Luminous Indicator Needle & Illuminated Tip Dot
        juce::Path needle;
        needle.startNewSubPath(centre.x, centre.y - knobRadius * 0.95f);
        needle.lineTo(centre.x, centre.y - capRadius * 0.30f);
        needle.applyTransform(juce::AffineTransform::rotation(toAngle, centre.x, centre.y));

        g.setColour(juce::Colour(0xff00f0ff).withAlpha(0.4f));
        g.strokePath(needle, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colours::white);
        g.strokePath(needle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Tip Dot
        float tipX = centre.x + (knobRadius * 0.85f) * std::sin(toAngle);
        float tipY = centre.y - (knobRadius * 0.85f) * std::cos(toAngle);
        g.setColour(juce::Colour(0xff00f0ff));
        g.fillEllipse(tipX - 1.5f, tipY - 1.5f, 3.0f, 3.0f);
    }
};

} // namespace conductor
