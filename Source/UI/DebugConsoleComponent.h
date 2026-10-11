#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DebugLog.h"

namespace conductor {

class DebugConsoleComponent : public juce::Component {
public:
    DebugConsoleComponent() {
        mLogEditor.setMultiLine(true);
        mLogEditor.setReadOnly(true);
        mLogEditor.setCaretVisible(false);
        mLogEditor.setScrollbarsShown(true);
        mLogEditor.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
        mLogEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090c10));
        mLogEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xff34d399)); // Emerald terminal green
        mLogEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1f2937));
        addAndMakeVisible(mLogEditor);

        mClearButton.setButtonText("CLEAR");
        mClearButton.onClick = [this]() {
            mLogEditor.clear();
        };
        addAndMakeVisible(mClearButton);

        mCopyButton.setButtonText("COPY ALL");
        mCopyButton.onClick = [this]() {
            juce::SystemClipboard::copyTextToClipboard(mLogEditor.getText());
        };
        addAndMakeVisible(mCopyButton);

        // Load existing logs
        auto existing = DebugLog::getInstance().getRecentLogs();
        for (const auto& line : existing) {
            mLogEditor.moveCaretToEnd();
            mLogEditor.insertTextAtCaret(juce::String(line) + "\n");
        }

        // Subscribe to live log events
        DebugLog::getInstance().setListener([this](const std::string& newMsg) {
            mLogEditor.moveCaretToEnd();
            mLogEditor.insertTextAtCaret(juce::String(newMsg) + "\n");
        });
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0d1117));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xff30363d));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto header = bounds.removeFromTop(28.0f).reduced(8.0f, 4.0f);
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.setColour(juce::Colour(0xff94a3b8));
        g.drawText("LIVE DEBUG CONSOLE // AUDIO & AI TRACE", header.removeFromLeft(300), juce::Justification::left);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(6);
        auto header = bounds.removeFromTop(28);

        mCopyButton.setBounds(header.removeFromRight(70).reduced(2));
        mClearButton.setBounds(header.removeFromRight(60).reduced(2));

        bounds.removeFromTop(4);
        mLogEditor.setBounds(bounds);
    }

private:
    juce::TextEditor mLogEditor;
    juce::TextButton mClearButton;
    juce::TextButton mCopyButton;
};

} // namespace conductor
