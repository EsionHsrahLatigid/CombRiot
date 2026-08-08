#include "violent/plugins/CombRiotEditor.h"

#include <algorithm>
#include <cstdint>
#include <functional>

namespace violent::plugin
{
namespace
{
constexpr std::uint32_t accentColor = 0xffff5a1fu;
constexpr std::uint32_t triggerIdleColor = 0xff25130du;
constexpr std::uint32_t triggerActiveColor = 0xffff5a1fu;
constexpr std::uint32_t meterBackColor = 0xff171a1eu;
constexpr std::uint32_t meterFillColor = 0xff31d17c;
} // namespace

class CombRiotEditor::TriggerPad final : public yup::Component
{
public:
    explicit TriggerPad (CombRiotEditor& owner)
        : editor (owner)
    {
        setWantsKeyboardFocus (false);
        setClickingGrabFocus (false);
        setMouseCursor (yup::MouseCursor::Hand);
        text.setText ("TRIGGER", yup::dontSendNotification);
        text.setJustification (yup::Justification::center);
        text.setClickingGrabFocus (false);
        addAndMakeVisible (text);
    }

    void paint (yup::Graphics& graphics) override
    {
        const auto active = editor.combRiotProcessor.getStandaloneTriggerGate();
        graphics.setFillColor (active ? triggerActiveColor : triggerIdleColor);
        graphics.fillRect (getLocalBounds().to<float>());

    }

    void resized() override
    {
        text.setBounds (getLocalBounds());
    }

    void mouseDown (const yup::MouseEvent&) override
    {
        editor.setTriggerGate (editor.gateState.setMouseGateHeld (true));
        repaint();
    }

    void mouseUp (const yup::MouseEvent&) override
    {
        editor.setTriggerGate (editor.gateState.setMouseGateHeld (false));
        repaint();
    }

    void mouseExit (const yup::MouseEvent&) override
    {
        editor.setTriggerGate (editor.gateState.setMouseGateHeld (false));
        repaint();
    }

private:
    CombRiotEditor& editor;
    yup::Label text;
};

CombRiotEditor::CombRiotEditor (CombRiotPlugin& processor)
    : combRiotProcessor (processor)
{
    const auto processorParameters = processor.getParameters();
    parameters.assign (processorParameters.begin(), processorParameters.end());

    titleLabel = std::make_unique<yup::Label>();
    titleLabel->setText ("CombRiot", yup::dontSendNotification);
    titleLabel->setJustification (yup::Justification::centerLeft);
    addAndMakeVisible (*titleLabel);

    warningLabel = std::make_unique<yup::Label>();
    warningLabel->setText ("Standalone trigger: hold the pad or Space. MIDI input remains active.", yup::dontSendNotification);
    warningLabel->setJustification (yup::Justification::centerLeft);
    addAndMakeVisible (*warningLabel);

    triggerPad = std::make_unique<TriggerPad> (*this);
    addAndMakeVisible (*triggerPad);

    labels.reserve (parameters.size());
    sliders.reserve (parameters.size());
    valueLabels.reserve (parameters.size());

    for (const auto& parameter : parameters)
    {
        auto label = std::make_unique<yup::Label>();
        label->setText (parameter->getName(), yup::dontSendNotification);
        label->setJustification (yup::Justification::center);
        addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        auto slider = std::make_unique<yup::Slider> (yup::Slider::RotaryVerticalDrag);
        slider->setRange (parameter->getMinimumValue(),
                          parameter->getMaximumValue(),
                          parameter->isStepped() ? 1.0 : 0.0);
        slider->setDefaultValue (parameter->getDefaultValue());
        slider->setValue (parameter->getValue(), yup::dontSendNotification);
        slider->setTextBoxStyle (yup::Slider::NoTextBox);
        slider->setPopupDisplayEnabled (false);
        slider->setMouseCursor (yup::MouseCursor::Hand);
        slider->setClickingGrabFocus (false);
        slider->onDragStart = [parameter] (const yup::MouseEvent&) { parameter->beginChangeGesture(); };
        slider->onValueChanged = [parameter] (double value)
        {
            parameter->setValueNotifyingHost (static_cast<float> (value));
        };
        slider->onDragEnd = [parameter] (const yup::MouseEvent&) { parameter->endChangeGesture(); };
        addAndMakeVisible (*slider);
        sliders.push_back (std::move (slider));

        auto valueLabel = std::make_unique<yup::Label>();
        valueLabel->setText (parameter->toString(), yup::dontSendNotification);
        valueLabel->setJustification (yup::Justification::center);
        addAndMakeVisible (*valueLabel);
        valueLabels.push_back (std::move (valueLabel));
    }

    setWantsKeyboardFocus (true);
    setSize (getPreferredSize().to<float>());
    startTimerHz (30);
}

CombRiotEditor::~CombRiotEditor()
{
    setTriggerGate (false);
}

bool CombRiotEditor::isResizable() const
{
    return true;
}

bool CombRiotEditor::shouldPreserveAspectRatio() const
{
    return true;
}

yup::Size<int> CombRiotEditor::getPreferredSize() const
{
    return { 940, 580 };
}

void CombRiotEditor::paint (yup::Graphics& graphics)
{
    graphics.setFillColor (0xff0a0b0du);
    graphics.fillAll();

    graphics.setFillColor (accentColor);
    graphics.fillRect (0.0f, 0.0f, getWidth(), 5.0f);

    constexpr float meterX = 180.0f;
    constexpr float meterY = 88.0f;
    const auto meterWidth = std::max (24.0f, getWidth() - meterX - 24.0f);
    graphics.setFillColor (meterBackColor);
    graphics.fillRect (meterX, meterY, meterWidth, 18.0f);

    graphics.setFillColor (meterFillColor);
    graphics.fillRect (meterX, meterY, meterWidth * std::clamp (displayedPeak, 0.0f, 1.0f), 18.0f);
}

void CombRiotEditor::resized()
{
    constexpr int columns = 5;
    constexpr float margin = 20.0f;
    constexpr float top = 130.0f;
    constexpr float gap = 12.0f;
    constexpr float labelHeight = 24.0f;
    constexpr float valueHeight = 24.0f;
    constexpr float controlGap = 4.0f;

    const auto bounds = getLocalBounds();
    const auto cellWidth = (bounds.getWidth() - 2.0f * margin - gap * (columns - 1)) / columns;
    const auto rows = std::max (1, static_cast<int> ((sliders.size() + columns - 1) / columns));
    const auto availableHeight = bounds.getHeight() - top - margin;
    const auto cellHeight = (availableHeight - gap * (rows - 1)) / rows;

    titleLabel->setBounds (24.0f, 12.0f, bounds.getWidth() - 48.0f, 30.0f);
    warningLabel->setBounds (24.0f, 43.0f, bounds.getWidth() - 48.0f, 24.0f);
    triggerPad->setBounds (24.0f, 78.0f, 132.0f, 38.0f);

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto column = static_cast<int> (i) % columns;
        const auto row = static_cast<int> (i) / columns;
        const auto x = margin + column * (cellWidth + gap);
        const auto y = top + row * (cellHeight + gap);
        const auto controlHeight = cellHeight - labelHeight - valueHeight - 2.0f * controlGap;
        const auto controlSize = std::max (20.0f, std::min (cellWidth - 8.0f, controlHeight));
        const auto controlX = x + 0.5f * (cellWidth - controlSize);
        const auto controlY = y + labelHeight + controlGap;

        labels[i]->setBounds (x, y, cellWidth, labelHeight);
        sliders[i]->setBounds (controlX, controlY, controlSize, controlSize);
        valueLabels[i]->setBounds (x, y + cellHeight - valueHeight, cellWidth, valueHeight);
    }
}

void CombRiotEditor::keyDown (const yup::KeyPress& key, const yup::Point<float>&)
{
    if (key.getKey() == yup::KeyPress::spaceKey)
        setTriggerGate (gateState.setSpaceGateHeld (true));
}

void CombRiotEditor::keyUp (const yup::KeyPress& key, const yup::Point<float>&)
{
    if (key.getKey() == yup::KeyPress::spaceKey)
        setTriggerGate (gateState.setSpaceGateHeld (false));
}

void CombRiotEditor::focusLost()
{
    setTriggerGate (gateState.clear());
}

void CombRiotEditor::timerCallback()
{
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        if (! sliders[i]->isCurrentlyBeingDragged())
            sliders[i]->setValue (parameters[i]->getValue(), yup::dontSendNotification);
        valueLabels[i]->setText (parameters[i]->toString(), yup::dontSendNotification);
    }

    const auto nextPeak = combRiotProcessor.consumeOutputPeak();
    displayedPeak = std::max (nextPeak, displayedPeak * 0.78f);
    triggerPad->repaint();
    repaint();
}

void CombRiotEditor::setTriggerGate (bool shouldBeOn) noexcept
{
    combRiotProcessor.setStandaloneTriggerGate (shouldBeOn);
}

} // namespace violent::plugin
