#include "AnkerDeviceDetails.hpp"

#include <algorithm>
#include <cstdio>

#include <wx/button.h>
#include <wx/statline.h>

#include "AnkerBtn.hpp"
#include "Common/AnkerGUIConfig.hpp"
#include "Common/AnkerDialog.hpp"
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/DataMangerUi.hpp"
#include "DeviceObjectBase.h"
#include "AnkerNetBase.h"
#include "AnkerNavWidget.hpp"

namespace Slic3r {
namespace GUI {

// Same palette the Device tab uses.
static const wxColour kBackground   = wxColour("#18191B");
static const wxColour kPanel        = wxColour("#292A2D");
static const wxColour kControl      = wxColour("#0F1011");
static const wxColour kTextPrimary  = wxColour("#FFFFFF");
static const wxColour kTextMuted    = wxColour("#999999");
static const wxColour kSelected     = wxColour("#3E3F42");
// AnkerBtn only paints a hover/press state when these are set; without them a
// button looks identical pressed and idle, so a click gives no feedback at all.
static const wxColour kHover        = wxColour("#2A2B2E");
static const wxColour kPressed      = wxColour("#4A4B4F");

AnkerDeviceDetails::AnkerDeviceDetails(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
{
    SetBackgroundColour(kBackground);
    initUi();

    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        rescaleJogButtons();
        e.Skip();
    });
}

wxString AnkerDeviceDetails::currentPrinterName() const
{
    if (m_sn.empty())
        return wxEmptyString;
    if (auto dev = CurDevObject(m_sn))
        return wxString::FromUTF8(dev->GetStationName());
    return wxString::FromUTF8(m_sn);
}

void AnkerDeviceDetails::setCurrentDeviceSn(const std::string& sn)
{
    if (sn == m_sn)
        return;

    // Park the outgoing printer's console before swapping.
    if (m_gcodeLog && !m_sn.empty())
        m_gcodeHistory[m_sn] = m_gcodeLog->GetValue();

    m_sn = sn;
    m_cmds.setDeviceSn(sn);

    // Show which printer these controls will actually drive. Commands going to a
    // machine the user is not looking at is the worst failure this tab can have,
    // so the label always reflects m_sn -- including when nothing is selected.
    const wxString name = currentPrinterName();
    if (m_printerLabel) {
        m_printerLabel->SetLabel(name.IsEmpty()
            ? _L("No printer selected")
            : wxString::Format(_L("Controlling: %s"), name));
        m_printerLabel->SetForegroundColour(name.IsEmpty() ? kTextMuted : kTextPrimary);
    }
    if (m_printerList && !sn.empty())
        m_printerList->switchTabFromSn(sn);

    if (m_gcodeLog) {
        auto it = m_gcodeHistory.find(m_sn);
        m_gcodeLog->ChangeValue(it == m_gcodeHistory.end() ? wxString() : it->second);
        m_gcodeLog->ShowPosition(m_gcodeLog->GetLastPosition());
    }
    Layout();
}

void AnkerDeviceDetails::seedSelection(const std::string& sn)
{
    // Only fills an empty selection. The Device tab's idea of "current" is derived
    // from its own widget list and does not always match what is highlighted there,
    // so it is a starting suggestion, never an override of an explicit choice here.
    if (m_sn.empty() && !sn.empty())
        setCurrentDeviceSn(sn);
}

AnkerBtn* AnkerDeviceDetails::makeRoundButton(wxWindow* parent, const wxString& label)
{
    AnkerBtn* btn = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->SetMinSize(wxSize(m_jogDiameter, m_jogDiameter));
    btn->SetMaxSize(wxSize(m_jogDiameter, m_jogDiameter));
    btn->SetText(label);
    btn->SetBackgroundColour(kControl);
    btn->SetTextColor(kTextPrimary);
    btn->SetRadius(m_jogDiameter / 2.0);
    btn->SetFont(ANKER_BOLD_FONT_NO_1);
    // Deliberately no BgNorColor: the idle fill stays SetBackgroundColour(), which
    // the step selector rewrites to show which step is active.
    btn->SetBgHoverColor(kHover);
    btn->SetBgPressedColor(kPressed);
    m_jogButtons.push_back(btn);
    return btn;
}

// Mouse-level probe. Separates "the click never reached the button" from "it
// reached the button but wxEVT_BUTTON never fired" -- the two cases look
// identical from the command layer.
void AnkerDeviceDetails::traceClicks(AnkerBtn* btn)
{
    btn->Bind(wxEVT_LEFT_DOWN, [btn](wxMouseEvent& e) {
        std::fprintf(stderr, "[DeviceDetails] LEFT_DOWN on '%s'\n",
                     btn->GetText().ToStdString().c_str());
        e.Skip();   // let AnkerBtn::OnPressed run and post wxEVT_BUTTON
    });
}

void AnkerDeviceDetails::rescaleJogButtons()
{
    if (m_jogButtons.empty() || !m_movePanel)
        return;

    // Four button rows plus padding have to fit the Move frame; clamp so the pad
    // stays usable on a small window and doesn't get silly on a large one.
    const wxSize s = m_movePanel->GetClientSize();
    const int byHeight = (s.GetHeight() - 80) / 4;
    const int byWidth  = (s.GetWidth() / 2 - 40) / 3;
    const int d = std::max(48, std::min(120, std::min(byHeight, byWidth)));
    if (d == m_jogDiameter)
        return;

    m_jogDiameter = d;
    for (AnkerBtn* b : m_jogButtons) {
        b->SetMinSize(wxSize(d, d));
        b->SetMaxSize(wxSize(d, d));
        b->SetRadius(d / 2.0);
    }
    m_movePanel->Layout();
}

wxSizer* AnkerDeviceDetails::buildStepSelector(wxWindow* parent)
{
    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    m_stepButtons.clear();

    for (size_t i = 0; i < m_stepValues.size(); ++i) {
        const double mm = m_stepValues[i];
        AnkerBtn* b = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        b->SetMinSize(wxSize(60, 40));
        b->SetText(wxString::Format("%g mm", mm));
        b->SetBackgroundColour(mm == m_stepMm ? kSelected : kControl);
        b->SetTextColor(mm == m_stepMm ? kTextPrimary : kTextMuted);
        b->SetRadius(8);
        b->SetFont(ANKER_BOLD_FONT_NO_1);
        b->SetBgHoverColor(kHover);
        b->SetBgPressedColor(kPressed);
        traceClicks(b);
        b->Bind(wxEVT_BUTTON, [this, mm](wxCommandEvent&) { setStep(mm); });
        m_stepButtons.push_back(b);
        row->Add(b, 1, wxEXPAND | wxALL, 3);
    }
    return row;
}

void AnkerDeviceDetails::setStep(double mm)
{
    m_stepMm = mm;
    for (size_t i = 0; i < m_stepButtons.size() && i < m_stepValues.size(); ++i) {
        const bool on = m_stepValues[i] == mm;
        m_stepButtons[i]->SetBackgroundColour(on ? kSelected : kControl);
        m_stepButtons[i]->SetTextColor(on ? kTextPrimary : kTextMuted);
        m_stepButtons[i]->Refresh();
    }
}

wxSizer* AnkerDeviceDetails::buildJogPads(wxWindow* parent)
{
    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);

    // One place to log every jog, so a silent button is unambiguous.
    auto bindJog = [this](AnkerBtn* b, const char* name,
                          AnkerMoveCommands::Axis axis, double sign) {
        traceClicks(b);
        b->Bind(wxEVT_BUTTON, [this, name, axis, sign](wxCommandEvent&) {
            std::fprintf(stderr, "[DeviceDetails] jog %s, step=%g sn=%s\n",
                         name, m_stepMm, m_sn.c_str());
            m_cmds.jog(axis, sign * m_stepMm);
        });
    };
    auto bindHome = [this](AnkerBtn* b, const char* name, std::function<void()> fn) {
        traceClicks(b);
        b->Bind(wxEVT_BUTTON, [this, name, fn](wxCommandEvent&) {
            std::fprintf(stderr, "[DeviceDetails] home %s, sn=%s\n", name, m_sn.c_str());
            fn();
        });
    };

    // ---- X/Y pad, a 3x3 grid of round buttons ----
    wxBoxSizer* xyBox = new wxBoxSizer(wxVERTICAL);
    wxStaticText* xyLabel = new wxStaticText(parent, wxID_ANY, "X/Y");
    xyLabel->SetForegroundColour(kTextMuted);
    xyBox->Add(xyLabel, 0, wxLEFT | wxBOTTOM, 4);

    wxFlexGridSizer* xyGrid = new wxFlexGridSizer(3, 3, 10, 10);
    AnkerBtn* yUp    = makeRoundButton(parent, wxString::FromUTF8("↑"));
    AnkerBtn* xLeft  = makeRoundButton(parent, wxString::FromUTF8("←"));
    AnkerBtn* xyHome = makeRoundButton(parent, wxString::FromUTF8("⌂"));
    AnkerBtn* xRight = makeRoundButton(parent, wxString::FromUTF8("→"));
    AnkerBtn* yDown  = makeRoundButton(parent, wxString::FromUTF8("↓"));

    xyGrid->AddSpacer(1); xyGrid->Add(yUp, 0, wxALIGN_CENTER); xyGrid->AddSpacer(1);
    xyGrid->Add(xLeft,  0, wxALIGN_CENTER);
    xyGrid->Add(xyHome, 0, wxALIGN_CENTER);
    xyGrid->Add(xRight, 0, wxALIGN_CENTER);
    xyGrid->AddSpacer(1); xyGrid->Add(yDown, 0, wxALIGN_CENTER); xyGrid->AddSpacer(1);

    // The arrows are screen-relative, not axis-sign-relative: on the M5, screen-up
    // is Y-negative. Binding up to Y+ moved the plate the wrong way (confirmed on
    // hardware), so the Y pair is deliberately inverted here.
    bindJog(yUp,    "Y-", AnkerMoveCommands::Axis::Y, -1.0);
    bindJog(yDown,  "Y+", AnkerMoveCommands::Axis::Y,  1.0);
    bindJog(xLeft,  "X-", AnkerMoveCommands::Axis::X, -1.0);
    bindJog(xRight, "X+", AnkerMoveCommands::Axis::X,  1.0);
    bindHome(xyHome, "X/Y", [this] {
        m_cmds.home(AnkerMoveCommands::Axis::X);
        m_cmds.home(AnkerMoveCommands::Axis::Y);
    });

    xyBox->Add(xyGrid, 0, wxALIGN_CENTER_HORIZONTAL);
    row->Add(xyBox, 2, wxALIGN_CENTER_VERTICAL | wxALL, 10);

    // ---- Z column ----
    wxBoxSizer* zBox = new wxBoxSizer(wxVERTICAL);
    wxStaticText* zLabel = new wxStaticText(parent, wxID_ANY, "Z");
    zLabel->SetForegroundColour(kTextMuted);
    zBox->Add(zLabel, 0, wxLEFT | wxBOTTOM, 4);

    AnkerBtn* zUp   = makeRoundButton(parent, wxString::FromUTF8("↑"));
    AnkerBtn* zHome = makeRoundButton(parent, wxString::FromUTF8("⌂"));
    AnkerBtn* zDown = makeRoundButton(parent, wxString::FromUTF8("↓"));

    bindJog(zUp,   "Z+", AnkerMoveCommands::Axis::Z,  1.0);
    bindJog(zDown, "Z-", AnkerMoveCommands::Axis::Z, -1.0);
    bindHome(zHome, "Z", [this] { m_cmds.home(AnkerMoveCommands::Axis::Z); });

    zBox->Add(zUp,   0, wxALIGN_CENTER | wxBOTTOM, 10);
    zBox->Add(zHome, 0, wxALIGN_CENTER | wxBOTTOM, 10);
    zBox->Add(zDown, 0, wxALIGN_CENTER);
    row->Add(zBox, 1, wxALIGN_CENTER_VERTICAL | wxALL, 10);

    return row;
}

wxSizer* AnkerDeviceDetails::buildAdjustments(wxWindow* parent)
{
    wxBoxSizer* box = new wxBoxSizer(wxVERTICAL);

    // No header here: buildSettings() already labels this group.
    AnkerBtn* level = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    level->SetMinSize(wxSize(-1, 52));
    level->SetText(_L("Auto-Level"));
    level->SetBackgroundColour(kPanel);
    level->SetTextColor(kTextPrimary);
    level->SetRadius(8);
    level->SetFont(ANKER_BOLD_FONT_NO_1);
    level->SetBgHoverColor(kHover);
    level->SetBgPressedColor(kPressed);
    traceClicks(level);
    level->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        const wxString printer = currentPrinterName();
        if (printer.IsEmpty()) {
            AnkerDialog warn(this, wxID_ANY, _L("Auto-Level"),
                _L("Select a printer first."), wxDefaultPosition, AnkerSize(400, 180));
            warn.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog);
            return;
        }
        // ~10 minutes of probing that cannot be cleanly interrupted, so confirm --
        // and name the machine. With more than one printer paired, an unnamed prompt
        // makes it possible to tie up the wrong one for ten minutes.
        AnkerDialog dialog(this, wxID_ANY, _L("Auto-Level"),
            wxString::Format(
                _L("Auto-level \"%s\"?\n\n"
                   "It preheats to 180/60 C, probes 49 points, and takes about "
                   "10 minutes. It cannot be stopped once started."),
                printer),
            wxDefaultPosition, AnkerSize(420, 230));
        if (dialog.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog) == wxID_OK) {
            m_cmds.autoLevel();
            appendLog("G29");
        }
    });
    box->Add(level, 0, wxEXPAND);

    return box;
}

wxSizer* AnkerDeviceDetails::buildGcodeConsole(wxWindow* parent)
{
    wxBoxSizer* box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* header = new wxStaticText(parent, wxID_ANY, _L("Send G-code"));
    header->SetForegroundColour(kTextMuted);
    box->Add(header, 0, wxBOTTOM, 8);

    wxBoxSizer* inputRow = new wxBoxSizer(wxHORIZONTAL);
    m_gcodeInput = new wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                  wxSize(-1, 34), wxTE_PROCESS_ENTER);
    m_gcodeInput->SetBackgroundColour(kControl);
    m_gcodeInput->SetForegroundColour(kTextPrimary);
    m_gcodeInput->SetHint("G1 X10 F3000");

    // G-code is conventionally upper case, and the firmware is easier to reason about
    // when what you typed is what goes out. Uppercase as you type, keeping the caret
    // where it was. ChangeValue() does not re-fire wxEVT_TEXT, so this cannot recurse.
    m_gcodeInput->Bind(wxEVT_TEXT, [this](wxCommandEvent& e) {
        const wxString v = m_gcodeInput->GetValue();
        const wxString up = v.Upper();
        if (up != v) {
            const long caret = m_gcodeInput->GetInsertionPoint();
            m_gcodeInput->ChangeValue(up);
            m_gcodeInput->SetInsertionPoint(caret);
        }
        e.Skip();
    });

    AnkerBtn* send = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    send->SetMinSize(wxSize(90, 34));
    send->SetText(_L("Send"));
    send->SetBackgroundColour(kSelected);
    send->SetTextColor(kTextPrimary);
    send->SetRadius(8);
    send->SetBgHoverColor(kHover);
    send->SetBgPressedColor(kPressed);
    traceClicks(send);

    auto doSend = [this](wxCommandEvent&) {
        wxString line = m_gcodeInput->GetValue().Trim().Trim(false);
        std::fprintf(stderr, "[DeviceDetails] send clicked, text='%s' sn=%s\n",
                     line.ToStdString().c_str(), m_sn.c_str());
        if (line.IsEmpty())
            return;
        // Sent exactly as typed -- this is a power-user tool, not a filtered one.
        m_cmds.sendRaw(line.ToStdString());
        appendLog(line);
        m_gcodeInput->Clear();
    };
    send->Bind(wxEVT_BUTTON, doSend);
    m_gcodeInput->Bind(wxEVT_TEXT_ENTER, doSend);

    inputRow->Add(m_gcodeInput, 1, wxEXPAND | wxRIGHT, 8);
    inputRow->Add(send, 0);
    box->Add(inputRow, 0, wxEXPAND | wxBOTTOM, 8);

    m_gcodeLog = new wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                wxSize(-1, 110), wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    m_gcodeLog->SetBackgroundColour(kControl);
    m_gcodeLog->SetForegroundColour(kTextMuted);
    box->Add(m_gcodeLog, 1, wxEXPAND);

    attachClipboardMenu(m_gcodeInput, true);
    attachClipboardMenu(m_gcodeLog, false);   // read-only: copy and select-all only

    return box;
}

void AnkerDeviceDetails::attachClipboardMenu(wxTextCtrl* ctrl, bool editable)
{
    enum { ID_CUT = wxID_HIGHEST + 4101, ID_COPY, ID_PASTE, ID_SELALL };

    ctrl->Bind(wxEVT_CONTEXT_MENU, [ctrl, editable](wxContextMenuEvent&) {
        wxMenu menu;
        if (editable)
            menu.Append(ID_CUT, _L("Cut") + "\tCtrl+X")->Enable(ctrl->CanCut());
        menu.Append(ID_COPY, _L("Copy") + "\tCtrl+C")->Enable(ctrl->CanCopy());
        if (editable)
            menu.Append(ID_PASTE, _L("Paste") + "\tCtrl+V")->Enable(ctrl->CanPaste());
        menu.AppendSeparator();
        menu.Append(ID_SELALL, _L("Select All") + "\tCtrl+A");

        menu.Bind(wxEVT_MENU, [ctrl](wxCommandEvent& e) {
            switch (e.GetId()) {
            case ID_CUT:    ctrl->Cut();       break;
            case ID_COPY:   ctrl->Copy();      break;
            case ID_PASTE:  ctrl->Paste();     break;
            case ID_SELALL: ctrl->SelectAll(); break;
            }
        });
        ctrl->PopupMenu(&menu);
    });

    // The keyboard side is the menu bar's job: MainFrame carries an Edit menu whose
    // stock wxID_CUT/COPY/PASTE/SELECTALL items wx routes to the focused control.
    // Handling Cmd-C here as well would risk running both paths for one keypress.
}

void AnkerDeviceDetails::appendLog(const wxString& line)
{
    if (!m_gcodeLog)
        return;
    // No "> " prefix: the log is selected and copied to paste commands back, and a
    // decoration in the text would come with it. Every line here is a sent command.
    m_gcodeLog->AppendText(line + "\n");
    // Keep the per-printer copy current, so a switch away and back preserves it.
    if (!m_sn.empty())
        m_gcodeHistory[m_sn] = m_gcodeLog->GetValue();
}

void AnkerDeviceDetails::refreshPrinterList()
{
    if (!m_printerList)
        return;
    auto ankerNet = AnkerNetInst();
    if (!ankerNet)
        return;

    std::string firstSn;
    for (auto& dev : ankerNet->GetDeviceList()) {
        if (!dev)
            continue;
        const std::string sn = dev->GetSn();
        if (firstSn.empty())
            firstSn = sn;
        if (!m_printerList->checkTabExist(sn))
            m_printerList->addItem(dev->GetStationName(), sn);
    }
    m_printerList->showEmptyPanel(m_printerList->getCount() <= 0);

    // Never leave the controls pointing at nothing once printers are known, and never
    // leave the highlight disagreeing with what the buttons will actually drive.
    if (m_sn.empty() && !firstSn.empty())
        setCurrentDeviceSn(firstSn);
    else if (!m_sn.empty() && m_printerList)
        m_printerList->switchTabFromSn(m_sn);
}

AnkerBtn* AnkerDeviceDetails::makeSettingRow(wxWindow* parent, const wxString& label,
                                             const wxString& value, bool enabled)
{
    AnkerBtn* row = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    row->SetMinSize(wxSize(-1, 46));
    row->SetText(value.IsEmpty() ? label : (label + "     " + value));
    row->SetBackgroundColour(kPanel);
    row->SetTextColor(enabled ? kTextPrimary : kTextMuted);
    row->SetRadius(8);
    row->SetFont(ANKER_BOLD_FONT_NO_1);
    row->Enable(enabled);
    return row;
}

wxSizer* AnkerDeviceDetails::buildSettings(wxWindow* parent)
{
    wxBoxSizer* box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* header = new wxStaticText(parent, wxID_ANY, _L("Settings"));
    header->SetForegroundColour(kTextMuted);
    box->Add(header, 0, wxBOTTOM, 8);

    // Auto-Level is the one live control here; everything below is reserved
    // space whose behaviour is still to be specified.
    box->Add(buildAdjustments(parent), 0, wxEXPAND | wxBOTTOM, 10);

    wxString deviceName;
    if (!m_sn.empty()) {
        if (auto dev = CurDevObject(m_sn))
            deviceName = wxString::FromUTF8(dev->GetStationName());
    }

    const struct { const char* label; bool live; } rows[] = {
        { "Accessories",     false },
        { "Wi-Fi Connection",false },
        { "AI Settings",     false },
        { "Share Printer",   false },
        { "Timelapses",      false },
        { "About Device",    false },
    };
    box->Add(makeSettingRow(parent, _L("Name"), deviceName, false), 0, wxEXPAND | wxBOTTOM, 6);
    for (const auto& r : rows)
        box->Add(makeSettingRow(parent, _L(r.label), wxEmptyString, r.live), 0, wxEXPAND | wxBOTTOM, 6);

    return box;
}

void AnkerDeviceDetails::initUi()
{
    // Root is two columns: the printer selector, then the controls.
    wxBoxSizer* root = new wxBoxSizer(wxHORIZONTAL);

    // ---- My Printer: the same list widget the Device tab uses ----
    m_printerList = new AnkerNavBar(this);
    m_printerList->SetMinSize(wxSize(280, -1));
    m_printerList->SetMaxSize(wxSize(280, -1));
    root->Add(m_printerList, 0, wxEXPAND);

    // Selecting here retargets the controls, exactly as the Device tab does.
    Bind(wxCUSTOMEVT_SWITCH_DEVICE, [this](wxCommandEvent& event) {
        if (auto* data = static_cast<wxStringClientData*>(event.GetClientObject()))
            setCurrentDeviceSn(data->GetData().ToStdString());
    });

    wxBoxSizer* content = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* headerRow = new wxBoxSizer(wxHORIZONTAL);
    m_printerLabel = new wxStaticText(this, wxID_ANY, _L("No printer selected"));
    m_printerLabel->SetForegroundColour(kTextPrimary);
    m_printerLabel->SetFont(ANKER_BOLD_FONT_NO_1);
    headerRow->Add(m_printerLabel, 0, wxALIGN_CENTER_VERTICAL);
    content->Add(headerRow, 0, wxLEFT | wxTOP | wxBOTTOM, 16);

    // Controls row: Move (with Settings beneath it) on the left, G-code on the right.
    wxBoxSizer* controlsRow = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* leftCol = new wxBoxSizer(wxVERTICAL);
    m_movePanel = new wxPanel(this, wxID_ANY);
    m_movePanel->SetBackgroundColour(kPanel);
    wxBoxSizer* moveSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* moveHeader = new wxStaticText(m_movePanel, wxID_ANY, _L("Move"));
    moveHeader->SetForegroundColour(kTextMuted);
    moveSizer->Add(moveHeader, 0, wxLEFT | wxTOP | wxBOTTOM, 12);
    moveSizer->Add(buildStepSelector(m_movePanel), 0, wxEXPAND | wxLEFT | wxRIGHT, 9);
    moveSizer->Add(buildJogPads(m_movePanel), 1, wxEXPAND | wxALL, 6);
    m_movePanel->SetSizer(moveSizer);

    leftCol->Add(m_movePanel, 3, wxEXPAND | wxBOTTOM, 16);
    // Reserved space for the settings rows, beneath Move.
    leftCol->Add(buildSettings(this), 4, wxEXPAND);
    controlsRow->Add(leftCol, 1, wxEXPAND | wxRIGHT, 16);

    controlsRow->Add(buildGcodeConsole(this), 1, wxEXPAND);

    content->Add(controlsRow, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    root->Add(content, 1, wxEXPAND);

    SetSizer(root);
    Layout();

    // The device list is populated asynchronously after login and the MQTT
    // subscribe, so it is normally still empty at construction. Re-read it every
    // time the tab is shown rather than only once here.
    Bind(wxEVT_SHOW, [this](wxShowEvent& e) {
        if (e.IsShown())
            refreshPrinterList();
        e.Skip();
    });
    refreshPrinterList();
}

} // namespace GUI
} // namespace Slic3r
