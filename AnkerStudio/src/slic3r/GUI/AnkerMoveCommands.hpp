#ifndef slic3r_AnkerMoveCommands_hpp_
#define slic3r_AnkerMoveCommands_hpp_

// Every physical movement the Device Details tab can ask for, in one place.
//
// There are two possible transports:
//
//   1. Raw Marlin G-code through vendor commandType 1043 (GCODE_COMMAND).
//      This is what the app already uses for extrude/retract and temperature,
//      so it is the known-good path. It is what is implemented here.
//
//   2. The vendor's own move commands -- 1024 MOVE_STEP, 1025 MOVE_DIRECTION,
//      1026 MOVE_ZERO, 1007 AUTO_LEVELING. These are named in
//      Ankermgmt/ankermake-m5-protocol (libflagship/mqtt.py) and in that repo's
//      COMMUNICATION-PATHS.md, but *none of their payload fields are documented*
//      -- every one is listed as "value=?", and neither repo contains a capture
//      showing one being sent.
//
// Guessing those payloads is not safe: the same guesswork already cost this
// project once, when 1026 was assumed to be "stop print" and turned out to be
// MOVE_ZERO (home axes), which is why Stop appeared to do nothing mid-print.
// An invented payload here would move a real gantry.
//
// To switch to transport 2, capture the official AnkerMake app jogging a printer
// using capture/mqtt_mitm.py from the protocol repo, note the JSON fields, and
// reimplement the three methods below. Nothing outside this class needs to
// change -- that is the whole point of it existing.

#include <string>

namespace Slic3r {
namespace GUI {

class AnkerMoveCommands
{
public:
    enum class Axis { X, Y, Z };

    explicit AnkerMoveCommands(std::string deviceSn) : m_sn(std::move(deviceSn)) {}

    void setDeviceSn(const std::string& sn) { m_sn = sn; }

    // Relative jog, millimetres, sign gives direction.
    bool jog(Axis axis, double mm);

    // Home a single axis, or all three.
    bool home(Axis axis);
    bool homeAll();

    // 49-point bed probe. Takes ~10 minutes and cannot be cleanly interrupted,
    // so callers are expected to confirm with the user first.
    bool autoLevel();

    // Raw line, exactly as typed by the user. Logged, not filtered.
    bool sendRaw(const std::string& gcode);

private:
    bool send(const std::string& gcode);
    // Resolves the device and publishes one line. Static and sn-keyed so it is safe
    // to call from the sequencing thread without touching this object.
    static bool sendOne(const std::string& sn, const std::string& gcode);
    static const char* axisLetter(Axis a);
    // Z is a leadscrew and far slower than the X/Y gantry; one feedrate for all
    // three axes is wrong for it. Tune here.
    static int feedrateFor(Axis a);

    std::string m_sn;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_AnkerMoveCommands_hpp_
