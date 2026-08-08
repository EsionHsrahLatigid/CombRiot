#pragma once

#include "violent/plugins/CombRiotPlugin.h"
#include "violent/plugins/StandaloneGateState.h"

#include <yup_gui/yup_gui.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace violent::plugin
{

class CombRiotEditor final
    : public yup::AudioProcessorEditor
    , private yup::Timer
{
public:
    explicit CombRiotEditor (CombRiotPlugin& processor);
    ~CombRiotEditor() override;

    bool isResizable() const override;
    bool shouldPreserveAspectRatio() const override;
    yup::Size<int> getPreferredSize() const override;
    void paint (yup::Graphics& graphics) override;
    void resized() override;
    void keyDown (const yup::KeyPress& key, const yup::Point<float>& position) override;
    void keyUp (const yup::KeyPress& key, const yup::Point<float>& position) override;
    void focusLost() override;

private:
    class TriggerPad;

    void timerCallback() override;
    void setTriggerGate (bool shouldBeOn) noexcept;

    CombRiotPlugin& combRiotProcessor;
    std::unique_ptr<yup::Label> titleLabel;
    std::unique_ptr<yup::Label> warningLabel;
    std::unique_ptr<TriggerPad> triggerPad;
    std::vector<yup::AudioParameter::Ptr> parameters;
    std::vector<std::unique_ptr<yup::Label>> labels;
    std::vector<std::unique_ptr<yup::Slider>> sliders;
    std::vector<std::unique_ptr<yup::Label>> valueLabels;

    StandaloneGateState gateState;
    float displayedPeak = 0.0f;
};

} // namespace violent::plugin
