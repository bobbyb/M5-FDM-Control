#include "AnkerMoveCommands.hpp"

#include <cstdio>

#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/DataMangerUi.hpp"
// DataMangerUi only forward-declares DeviceObjectBasePtr's element type; the
// full definition is needed to call through the pointer.
#include "DeviceObjectBase.h"

namespace Slic3r {
namespace GUI {

const char* AnkerMoveCommands::axisLetter(Axis a)
{
    switch (a) {
    case Axis::X: return "X";
    case Axis::Y: return "Y";
    case Axis::Z: return "Z";
    }
    return "X";
}

bool AnkerMoveCommands::send(const std::string& gcode)
{
    if (m_sn.empty()) {
        ANKER_LOG_WARNING << "move command dropped, no device selected: " << gcode;
        return false;
    }
    auto dev = CurDevObject(m_sn);
    if (!dev) {
        ANKER_LOG_WARNING << "move command dropped, device object missing: " << gcode;
        return false;
    }
    // Every line that reaches the printer is logged, including the manual box.
    ANKER_LOG_INFO << "gcode -> " << m_sn << " : " << gcode;
    dev->SendRawGcode(gcode);
    return true;
}

bool AnkerMoveCommands::jog(Axis axis, double mm)
{
    if (mm == 0.0)
        return false;

    // G91 relative -> single move -> G90 back to absolute. Leaving the printer
    // in relative mode would corrupt any subsequent absolute-coordinate print.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "G1 %s%.3f F3000", axisLetter(axis), mm);

    bool ok = send("G91");
    ok = send(buf) && ok;
    ok = send("G90") && ok;
    return ok;
}

bool AnkerMoveCommands::home(Axis axis)
{
    return send(std::string("G28 ") + axisLetter(axis));
}

bool AnkerMoveCommands::homeAll()
{
    return send("G28");
}

bool AnkerMoveCommands::autoLevel()
{
    return send("G29");
}

bool AnkerMoveCommands::sendRaw(const std::string& gcode)
{
    if (gcode.empty())
        return false;
    return send(gcode);
}

} // namespace GUI
} // namespace Slic3r
