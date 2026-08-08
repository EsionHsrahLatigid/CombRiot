#pragma once

namespace violent::plugin
{

class StandaloneGateState
{
public:
    [[nodiscard]] bool setMouseGateHeld (bool shouldBeHeld) noexcept
    {
        mouseGateHeld = shouldBeHeld;
        return isHeld();
    }

    [[nodiscard]] bool setSpaceGateHeld (bool shouldBeHeld) noexcept
    {
        spaceGateHeld = shouldBeHeld;
        return isHeld();
    }

    [[nodiscard]] bool isHeld() const noexcept
    {
        return mouseGateHeld || spaceGateHeld;
    }

    [[nodiscard]] bool clear() noexcept
    {
        mouseGateHeld = false;
        spaceGateHeld = false;
        return false;
    }

private:
    bool mouseGateHeld = false;
    bool spaceGateHeld = false;
};

} // namespace violent::plugin
