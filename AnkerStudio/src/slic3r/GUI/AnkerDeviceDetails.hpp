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

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "AnkerMoveCommands.hpp"

class AnkerBtn;
class AnkerNavBar;

namespace Slic3r {
namespace GUI {

class AnkerDeviceDetails : public wxPanel
{
public:
    AnkerDeviceDetails(wxWindow* parent, wxWindowID id = wxID_ANY);

    // Retarget every control on this tab at one printer, and make that visible:
    // list highlight, header, and the G-code log all follow.
    void setCurrentDeviceSn(const std::string& sn);

    // Initial target, offered by the Device tab. Ignored once the user has picked a
    // printer here -- their explicit choice in this tab's own list wins, and must not
    // be silently overwritten on every tab switch.
    void seedSelection(const std::string& sn);

    // (Re)populate the printer list from the current device list.
    void refreshPrinterList();

private:
    void initUi();
    wxSizer* buildStepSelector(wxWindow* parent);
    wxSizer* buildJogPads(wxWindow* parent);
    wxSizer* buildAdjustments(wxWindow* parent);
    // Grouped setting rows. Auto-Level is live; the rest are placeholders whose
    // behaviour is still to be specified.
    wxSizer* buildSettings(wxWindow* parent);
    AnkerBtn* makeSettingRow(wxWindow* parent, const wxString& label,
                             const wxString& value = wxEmptyString, bool enabled = true);
    wxSizer* buildGcodeConsole(wxWindow* parent);

    AnkerBtn* makeRoundButton(wxWindow* parent, const wxString& label);
    // Logs raw mouse-down, so a dead button can be told from a dead command path.
    static void traceClicks(AnkerBtn* btn);
    void      setStep(double mm);
    void      appendLog(const wxString& line);
    // Name of the printer the controls currently drive, for labels and for the
    // confirmation on anything long-running or irreversible.
    wxString  currentPrinterName() const;
    // Right-click Cut/Copy/Paste/Select All, so the clipboard is reachable without
    // relying on menu-bar key equivalents.
    void      attachClipboardMenu(wxTextCtrl* ctrl, bool editable);
    // Jog buttons scale with the panel so they stay comfortably sized on any
    // window; recomputed on every resize.
    void      rescaleJogButtons();

    AnkerMoveCommands m_cmds{ std::string() };
    double            m_stepMm{ 10.0 };

    std::vector<AnkerBtn*> m_stepButtons;
    std::vector<double>    m_stepValues{ 1.0, 10.0, 20.0, 50.0 };

    AnkerNavBar*  m_printerList{ nullptr };
    wxStaticText* m_printerLabel{ nullptr };
    wxPanel*      m_movePanel{ nullptr };
    std::vector<AnkerBtn*> m_jogButtons;
    int           m_jogDiameter{ 64 };

    wxTextCtrl* m_gcodeInput{ nullptr };
    wxTextCtrl* m_gcodeLog{ nullptr };
    std::string m_sn;

    // One G-code history per printer, keyed by serial. Switching printers swaps the
    // console contents instead of showing another machine's commands.
    std::map<std::string, wxString> m_gcodeHistory;
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_AnkerDeviceDetails_hpp_
