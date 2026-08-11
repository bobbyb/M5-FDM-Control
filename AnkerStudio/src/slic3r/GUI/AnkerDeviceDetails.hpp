#ifndef slic3r_AnkerDeviceDetails_hpp_
#define slic3r_AnkerDeviceDetails_hpp_

// The Device Details tab: manual control of the machine.
//
//   Move        step selector (1/10/20/50 mm), X/Y jog pad with home,
//               Z jog with home
//   Adjustments Auto-Level (confirmed, ~10 min)
//   G-code      a box to send a raw line, unfiltered, every line logged
//
// All movement goes through AnkerMoveCommands so the transport can be swapped
// without touching this file.

#include <string>
#include <vector>

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "AnkerMoveCommands.hpp"

class AnkerBtn;

namespace Slic3r {
namespace GUI {

class AnkerDeviceDetails : public wxPanel
{
public:
    AnkerDeviceDetails(wxWindow* parent, wxWindowID id = wxID_ANY);

    // Follows the device selected in the Device tab.
    void setCurrentDeviceSn(const std::string& sn);

private:
    void initUi();
    wxSizer* buildStepSelector(wxWindow* parent);
    wxSizer* buildJogPads(wxWindow* parent);
    wxSizer* buildAdjustments(wxWindow* parent);
    wxSizer* buildGcodeConsole(wxWindow* parent);

    AnkerBtn* makeRoundButton(wxWindow* parent, const wxString& label, int diameter = 56);
    void      setStep(double mm);
    void      appendLog(const wxString& line);

    AnkerMoveCommands m_cmds{ std::string() };
    double            m_stepMm{ 10.0 };

    std::vector<AnkerBtn*> m_stepButtons;
    std::vector<double>    m_stepValues{ 1.0, 10.0, 20.0, 50.0 };

    wxTextCtrl* m_gcodeInput{ nullptr };
    wxTextCtrl* m_gcodeLog{ nullptr };
    std::string m_sn;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_AnkerDeviceDetails_hpp_
