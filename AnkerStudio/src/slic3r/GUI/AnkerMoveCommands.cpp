#include "AnkerMoveCommands.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

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

int AnkerMoveCommands::feedrateFor(Axis a)
{
    // mm/min. 3000 (50 mm/s) is a sane gantry jog; the same figure on Z asks a
    // leadscrew for 50 mm/s, which the firmware will clamp at best.
    return a == Axis::Z ? 600 : 3000;
}

bool AnkerMoveCommands::sendOne(const std::string& sn, const std::string& gcode)
{
    // stderr, matching the net layer: ANKER_LOG_INFO is filtered out of Release
    // builds, so anything logged through it is invisible when diagnosing.
    if (sn.empty()) {
        std::fprintf(stderr, "[MoveCommands] dropped, no device selected: %s\n", gcode.c_str());
        return false;
    }
    auto dev = CurDevObject(sn);
    if (!dev) {
        std::fprintf(stderr, "[MoveCommands] dropped, no device object for sn=%s: %s\n",
                     sn.c_str(), gcode.c_str());
        return false;
    }
    std::fprintf(stderr, "[MoveCommands] gcode -> %s : %s\n", sn.c_str(), gcode.c_str());
    dev->SendRawGcode(gcode);
    return true;
}

bool AnkerMoveCommands::send(const std::string& gcode)
{
    return sendOne(m_sn, gcode);
}

bool AnkerMoveCommands::jog(Axis axis, double mm)
{
    if (mm == 0.0)
        return false;
    if (m_sn.empty()) {
        std::fprintf(stderr, "[MoveCommands] jog dropped, no device selected\n");
        return false;
    }

    // G91 relative -> single move -> G90 back to absolute. Leaving the printer
    // in relative mode would corrupt any subsequent absolute-coordinate print.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "G1 %s%.3f F%d", axisLetter(axis), mm, feedrateFor(axis));

    // Spaced, not back-to-back. Homing (one self-contained G28) worked while jogging
    // did not, and the MQTT client publishes at QoS 0 -- fire-and-forget, no ack, no
    // retransmit -- so a burst of three messages in the same instant can lose one.
    // Losing the G91 leaves the G1 to run as an absolute move; losing the G90 leaves
    // the printer in relative mode, which would corrupt the next print. Spacing them
    // costs ~600ms per jog and is dispatched off the UI thread so the button stays
    // responsive. AnkerMqttClient::publish is mutex-guarded, so this is safe.
    static const int kGapMs = 300;
    std::vector<std::string> lines{ "G91", buf, "G90" };
    std::thread([sn = m_sn, lines = std::move(lines)]() {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i)
                std::this_thread::sleep_for(std::chrono::milliseconds(kGapMs));
            sendOne(sn, lines[i]);
        }
    }).detach();
    return true;
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
