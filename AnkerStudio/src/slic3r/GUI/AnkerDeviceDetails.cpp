#include "AnkerDeviceDetails.hpp"

#include <wx/button.h>
#include <wx/statline.h>

#include "AnkerBtn.hpp"
#include "Common/AnkerGUIConfig.hpp"
#include "Common/AnkerDialog.hpp"
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"

namespace Slic3r {
namespace GUI {

// Same palette the Device tab uses.
static const wxColour kBackground   = wxColour("#18191B");
static const wxColour kPanel        = wxColour("#292A2D");
static const wxColour kControl      = wxColour("#0F1011");
static const wxColour kTextPrimary  = wxColour("#FFFFFF");
static const wxColour kTextMuted    = wxColour("#999999");
static const wxColour kSelected     = wxColour("#3E3F42");

AnkerDeviceDetails::AnkerDeviceDetails(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
{
    SetBackgroundColour(kBackground);
    initUi();
}

void AnkerDeviceDetails::setCurrentDeviceSn(const std::string& sn)
{
    m_sn = sn;
    m_cmds.setDeviceSn(sn);
}

AnkerBtn* AnkerDeviceDetails::makeRoundButton(wxWindow* parent, const wxString& label, int diameter)
{
    AnkerBtn* btn = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    btn->SetMinSize(wxSize(diameter, diameter));
    btn->SetMaxSize(wxSize(diameter, diameter));
    btn->SetText(label);
    btn->SetBackgroundColour(kControl);
    btn->SetTextColor(kTextPrimary);
    btn->SetRadius(diameter / 2.0);
    btn->SetFont(ANKER_BOLD_FONT_NO_1);
    return btn;
}

wxSizer* AnkerDeviceDetails::buildStepSelector(wxWindow* parent)
{
    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    m_stepButtons.clear();

    for (size_t i = 0; i < m_stepValues.size(); ++i) {
        const double mm = m_stepValues[i];
        AnkerBtn* b = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        b->SetMinSize(wxSize(120, 44));
        b->SetText(wxString::Format("%g mm", mm));
        b->SetBackgroundColour(mm == m_stepMm ? kSelected : kPanel);
        b->SetTextColor(mm == m_stepMm ? kTextPrimary : kTextMuted);
        b->SetRadius(8);
        b->SetFont(ANKER_BOLD_FONT_NO_1);
        b->Bind(wxEVT_BUTTON, [this, mm](wxCommandEvent&) { setStep(mm); });
        m_stepButtons.push_back(b);
        row->Add(b, 1, wxALL, 4);
    }
    return row;
}

void AnkerDeviceDetails::setStep(double mm)
{
    m_stepMm = mm;
    for (size_t i = 0; i < m_stepButtons.size() && i < m_stepValues.size(); ++i) {
        const bool on = m_stepValues[i] == mm;
        m_stepButtons[i]->SetBackgroundColour(on ? kSelected : kPanel);
        m_stepButtons[i]->SetTextColor(on ? kTextPrimary : kTextMuted);
        m_stepButtons[i]->Refresh();
    }
}

wxSizer* AnkerDeviceDetails::buildJogPads(wxWindow* parent)
{
    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);

    // ---- X/Y pad: up/left/home/right/down on a 3x3 grid ----
    wxBoxSizer* xyBox = new wxBoxSizer(wxVERTICAL);
    wxStaticText* xyLabel = new wxStaticText(parent, wxID_ANY, "X/Y");
    xyLabel->SetForegroundColour(kTextMuted);
    xyBox->Add(xyLabel, 0, wxLEFT | wxTOP, 12);

    wxGridSizer* xyGrid = new wxGridSizer(3, 3, 8, 8);
    auto addSpacer = [&]() { xyGrid->AddStretchSpacer(); };

    AnkerBtn* yUp    = makeRoundButton(parent, wxString::FromUTF8("↑"));
    AnkerBtn* xLeft  = makeRoundButton(parent, wxString::FromUTF8("←"));
    AnkerBtn* xyHome = makeRoundButton(parent, wxString::FromUTF8("⌂"));
    AnkerBtn* xRight = makeRoundButton(parent, wxString::FromUTF8("→"));
    AnkerBtn* yDown  = makeRoundButton(parent, wxString::FromUTF8("↓"));

    addSpacer(); xyGrid->Add(yUp, 0, wxALIGN_CENTER); addSpacer();
    xyGrid->Add(xLeft, 0, wxALIGN_CENTER);
    xyGrid->Add(xyHome, 0, wxALIGN_CENTER);
    xyGrid->Add(xRight, 0, wxALIGN_CENTER);
    addSpacer(); xyGrid->Add(yDown, 0, wxALIGN_CENTER); addSpacer();

    yUp->Bind(wxEVT_BUTTON,    [this](wxCommandEvent&) { m_cmds.jog(AnkerMoveCommands::Axis::Y,  m_stepMm); });
    yDown->Bind(wxEVT_BUTTON,  [this](wxCommandEvent&) { m_cmds.jog(AnkerMoveCommands::Axis::Y, -m_stepMm); });
    xLeft->Bind(wxEVT_BUTTON,  [this](wxCommandEvent&) { m_cmds.jog(AnkerMoveCommands::Axis::X, -m_stepMm); });
    xRight->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_cmds.jog(AnkerMoveCommands::Axis::X,  m_stepMm); });
    xyHome->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        m_cmds.home(AnkerMoveCommands::Axis::X);
        m_cmds.home(AnkerMoveCommands::Axis::Y);
    });

    xyBox->Add(xyGrid, 1, wxEXPAND | wxALL, 12);
    row->Add(xyBox, 2, wxEXPAND | wxRIGHT, 12);

    // ---- Z column ----
    wxBoxSizer* zBox = new wxBoxSizer(wxVERTICAL);
    wxStaticText* zLabel = new wxStaticText(parent, wxID_ANY, "Z");
    zLabel->SetForegroundColour(kTextMuted);
    zBox->Add(zLabel, 0, wxLEFT | wxTOP, 12);

    AnkerBtn* zUp   = makeRoundButton(parent, wxString::FromUTF8("↑"));
    AnkerBtn* zHome = makeRoundButton(parent, wxString::FromUTF8("⌂"));
    AnkerBtn* zDown = makeRoundButton(parent, wxString::FromUTF8("↓"));

    zUp->Bind(wxEVT_BUTTON,   [this](wxCommandEvent&) { m_cmds.jog(AnkerMoveCommands::Axis::Z,  m_stepMm); });
    zDown->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_cmds.jog(AnkerMoveCommands::Axis::Z, -m_stepMm); });
    zHome->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_cmds.home(AnkerMoveCommands::Axis::Z); });

    zBox->AddStretchSpacer();
    zBox->Add(zUp,   0, wxALIGN_CENTER | wxBOTTOM, 8);
    zBox->Add(zHome, 0, wxALIGN_CENTER | wxBOTTOM, 8);
    zBox->Add(zDown, 0, wxALIGN_CENTER | wxBOTTOM, 12);
    zBox->AddStretchSpacer();
    row->Add(zBox, 1, wxEXPAND);

    return row;
}

wxSizer* AnkerDeviceDetails::buildAdjustments(wxWindow* parent)
{
    wxBoxSizer* box = new wxBoxSizer(wxVERTICAL);

    wxStaticText* header = new wxStaticText(parent, wxID_ANY, _L("Adjustments"));
    header->SetForegroundColour(kTextMuted);
    box->Add(header, 0, wxBOTTOM, 8);

    AnkerBtn* level = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    level->SetMinSize(wxSize(-1, 56));
    level->SetText(_L("Auto-Level"));
    level->SetBackgroundColour(kPanel);
    level->SetTextColor(kTextPrimary);
    level->SetRadius(8);
    level->SetFont(ANKER_BOLD_FONT_NO_1);
    level->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        // ~10 minutes of probing that cannot be cleanly interrupted, so confirm.
        AnkerDialog dialog(this, wxID_ANY, _L("Auto-Level"),
            _L("Auto-level probes 49 points and takes about 10 minutes. "
               "It cannot be stopped once started. Continue?"),
            wxDefaultPosition, AnkerSize(400, 200));
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
                                  wxSize(-1, 36), wxTE_PROCESS_ENTER);
    m_gcodeInput->SetBackgroundColour(kControl);
    m_gcodeInput->SetForegroundColour(kTextPrimary);
    m_gcodeInput->SetHint("G1 X10 F3000");

    AnkerBtn* send = new AnkerBtn(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    send->SetMinSize(wxSize(100, 36));
    send->SetText(_L("Send"));
    send->SetBackgroundColour(kSelected);
    send->SetTextColor(kTextPrimary);
    send->SetRadius(8);

    auto doSend = [this](wxCommandEvent&) {
        wxString line = m_gcodeInput->GetValue().Trim().Trim(false);
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
                                wxSize(-1, 120), wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
    m_gcodeLog->SetBackgroundColour(kControl);
    m_gcodeLog->SetForegroundColour(kTextMuted);
    box->Add(m_gcodeLog, 1, wxEXPAND);

    return box;
}

void AnkerDeviceDetails::appendLog(const wxString& line)
{
    if (m_gcodeLog)
        m_gcodeLog->AppendText("> " + line + "\n");
}

void AnkerDeviceDetails::initUi()
{
    wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);

    wxStaticText* moveHeader = new wxStaticText(this, wxID_ANY, _L("Move"));
    moveHeader->SetForegroundColour(kTextMuted);
    root->Add(moveHeader, 0, wxLEFT | wxTOP, 16);

    root->Add(buildStepSelector(this), 0, wxEXPAND | wxALL, 12);
    root->Add(buildJogPads(this),      1, wxEXPAND | wxLEFT | wxRIGHT, 12);
    root->Add(buildAdjustments(this),  0, wxEXPAND | wxALL, 16);
    root->Add(buildGcodeConsole(this), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    SetSizer(root);
    Layout();
}

} // namespace GUI
} // namespace Slic3r
