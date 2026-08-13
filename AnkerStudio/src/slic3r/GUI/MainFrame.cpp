#include "MainFrame.hpp"
#include "AnkerDeviceDetails.hpp"

#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/listbook.h>
#include <wx/simplebook.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/progdlg.h>
#include <wx/tooltip.h>
#include <wx/filename.h>
#include <wx/debug.h>
#include <wx/protocol/http.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/log/trivial.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/PresetBundle.hpp"

#include "Tab.hpp"
// add by allen for ankerCfgDlg
#include "AnkerCfgTab.hpp"

#include "ProgressStatusBar.hpp"
#include "3DScene.hpp"
#include "PrintHostDialogs.hpp"
#include "wxExtensions.hpp"
#include "GUI_ObjectList.hpp"
#include "Mouse3DController.hpp"
#include "RemovableDriveManager.hpp"
#include "InstanceCheck.hpp"
#include "I18N.hpp"
#include "GLCanvas3D.hpp"
#include "../Utils/Process.hpp"
#include "format.hpp"

#include <fstream>
#include <string_view>

#include <jansson.h>
#include "GUI_App.hpp"
#include "UnsavedChangesDialog.hpp"
#include "MsgDialog.hpp"
#include "Notebook.hpp"
#include "GUI_Factories.hpp"
#include "GUI_ObjectList.hpp"
#include "Preferences.hpp"

#ifdef _WIN32
#include <dbt.h>
#include <shlobj.h>
#ifndef OPEN_SOURCE_MODE
#include "sentry.h"
#endif
#endif // _WIN32
#include "AnkerWebView.hpp"
#include <wx/stream.h>
#include <wx/msw/cursor.h>
#include <algorithm>
#include <wx/url.h>
#include <wx/variant.h>

#include "FilamentMaterialConvertor.hpp"
#include "FilamentMaterialManager.hpp"
#include "Common/AnkerDialog.hpp"
#include "Common/AnkerCopyrightDialog.hpp"
#include "Common/AnkerOTANotesBox.hpp"
#include "Common/AnkerComboBox.hpp"
#include "Common/AnkerSpinBox.hpp"
#include "Common/AnkerFeedbackDialog.hpp"
#include <slic3r/Utils/DataMangerUi.hpp>
#include "slic3r/GUI/Common/AnkerMsgDialog.hpp"
#include "AnkerNetBase.h"
#include "DeviceObjectBase.h"
#include "../../AnkerComFunction.hpp"
#include "Common/AnkerFont.hpp"
#include "slic3r/GUI/Network/MsgText.hpp"
#include "slic3r/GUI/Calibration/FlowCalibration.hpp"
#include "slic3r/GUI/Calibration/CalibrationMaxFlowrateDialog.hpp"
#include "slic3r/GUI/Calibration/CalibrationPresAdvDialog.hpp"
#include "slic3r/GUI/Calibration/CalibrationTempDialog.hpp"
#include "slic3r/GUI/Calibration/CalibrationRetractionDialog.hpp"
#include "slic3r/GUI/Calibration/CalibrationVfaDialog.hpp"
#include "slic3r/Config/AnkerCommonConfig.hpp"
#include "../AnkerComFunction.hpp"
#include "AnkerConfig.hpp"
#include <slic3r/Config/AnkerCommonConfig.hpp>
extern AnkerPlugin* pAnkerPlugin;

wxDEFINE_EVENT(wxCUSTOMEVT_ANKER_MAINWIN_MOVE, wxCommandEvent);
wxDEFINE_EVENT(wxCUSTOMEVT_ANKER_RELOAD_DATA, wxCommandEvent);
wxDEFINE_EVENT(wxCUSTOMEVT_ON_TAB_CHANGE, wxCommandEvent);

#define SHOW_ERR_DIALOG_CMD 1085
#define HIDE_ERR_DIALOG_CMD 1086

extern "C" void ToggleFullScreen(wxWindow * window);
using namespace AnkerNet;
StarCommentData       g_sliceCommentData;
namespace Slic3r {
namespace GUI {

enum class ERescaleTarget
{
    Mainframe,
    SettingsDialog
};

#ifdef __APPLE__
class AnkerStudioTaskBarIcon : public wxTaskBarIcon
{
public:
    AnkerStudioTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE) : wxTaskBarIcon(iconType) {}
    wxMenu *CreatePopupMenu() override {
        wxMenu *menu = new wxMenu;
        if(wxGetApp().app_config->get("single_instance") == "0") {
            // Only allow opening a new AnkerStudio instance on OSX if "single_instance" is disabled, 
            // as starting new instances would interfere with the locking mechanism of "single_instance" support.
            append_menu_item(menu, wxID_ANY, _L("Open new instance"), _L("Open a new M5 FDM Control instance"),
            [](wxCommandEvent&) { start_new_slicer(); }, "", nullptr);
        }
        append_menu_item(menu, wxID_ANY, _L("G-code preview") + dots, _L("Open G-code viewer"),
            [](wxCommandEvent&) { start_new_gcodeviewer_open_file(); }, "", nullptr);
        return menu;
    }
};
class GCodeViewerTaskBarIcon : public wxTaskBarIcon
{
public:
    GCodeViewerTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE) : wxTaskBarIcon(iconType) {}
    wxMenu *CreatePopupMenu() override {
        wxMenu *menu = new wxMenu;
        append_menu_item(menu, wxID_ANY, _L("Open M5 FDM Control"), _L("Open a new M5 FDM Control instance"),
            [](wxCommandEvent&) { start_new_slicer(nullptr, true); }, "", nullptr);
        append_menu_item(menu, wxID_ANY, _L("G-code preview") + dots, _L("Open new G-code viewer"),
            [](wxCommandEvent&) { start_new_gcodeviewer_open_file(); }, "", nullptr);
        return menu;
    }
};
#endif // __APPLE__

// Load the icon either from the exe, or from the ico file.
static wxIcon main_frame_icon(GUI_App::EAppMode app_mode)
{
#if _WIN32
    std::wstring path(size_t(MAX_PATH), wchar_t(0));
    int len = int(::GetModuleFileName(nullptr, path.data(), MAX_PATH));
    if (len > 0 && len < MAX_PATH) {
        path.erase(path.begin() + len, path.end());
        if (app_mode == GUI_App::EAppMode::GCodeViewer) {
            // Only in case the slicer was started with --gcodeviewer parameter try to load the icon from anker-gcodeviewer.exe
            // Otherwise load it from the exe.
            using namespace Slic3r::BrandConfig;
            for (const std::wstring_view exe_name : { std::wstring_view(StudioExeName), std::wstring_view(StudioConsoleExeName) })
                if (boost::iends_with(path, exe_name)) {
                    path.erase(path.end() - exe_name.size(), path.end());
                    path += GcodeViewerExeName;
                    break;
                }
        }
    }
    return wxIcon(path, wxBITMAP_TYPE_ICO);
#else // _WIN32
    return wxIcon(Slic3r::var(app_mode == GUI_App::EAppMode::Editor ? "AnkerStudio_128px.png" : "AnkerStudio-gcodeviewer_128px.png"), wxBITMAP_TYPE_PNG);
#endif // _WIN32
}

void MainFrame::ShowLoginedMenu()
{
    ANKER_LOG_INFO << "enter ShowLoginedMenu";
    auto ankerNet = AnkerNetInst();
    if (!ankerNet) {
        return;
    }
    if (!m_pLoginMenu)
    {
        ANKER_LOG_ERROR << "show logined menu error";
        return;
    }

    ClearLoingiMenu();
	
    wxString loginName = wxString::FromUTF8(ankerNet->GetNickName());
    if (loginName.IsEmpty())
    {
        ANKER_LOG_ERROR << "nick name format error.";
        loginName = ankerNet->GetUserEmail().substr(0, 3) + "***";
    }

	if (!wxFileExists(m_avatarPath))
	{
		auto userItem = append_menu_item(m_pLoginMenu, wxID_ANY, loginName, loginName,
			[this](wxCommandEvent&) {}, "defaultAvatar", nullptr,
			[]() {return true; }, this);

		userItem->Enable(false);
	}
    else
    {
        
		wxImage defaultAvatarImage(wxString::FromUTF8(Slic3r::var("Avatar.png")), wxBITMAP_TYPE_PNG);
        defaultAvatarImage.Rescale(16, 16, wxIMAGE_QUALITY_HIGH);

        wxMenuItem* userItem = new wxMenuItem(m_pLoginMenu, wxID_ANY, loginName);

		wxString wxStrImg = m_avatarPath;
		wxBitmap avatarImage;
		if (avatarImage.LoadFile(wxStrImg, wxBITMAP_TYPE_ANY))
		{
			wxImage image = avatarImage.ConvertToImage();
			image.Rescale(16, 16);
			userItem->SetBitmap(image);
		}
        else
        {
            userItem->SetBitmap(defaultAvatarImage);
        }

        m_pLoginMenu->Append(userItem);
	}
	append_menu_item(m_pLoginMenu,
		wxID_ANY,
		_L("common_toptable_logout"),
		_L("Log Out eufyMake"),
		[=](wxCommandEvent&) {
            ANKER_LOG_INFO << "use log out click";
            auto* ankerNet = AnkerNetInst();
            if (ankerNet) {
                ankerNet->logoutToServer();
            }
            LogOut();

		});

    SetWebviewTestItem();
    ANKER_LOG_INFO << "Leave ShowLoginedMenu";
}

void MainFrame::LogOut()
{    
    ANKER_LOG_INFO << "log out start";
    
    auto* ankerNet = AnkerNetInst();
    if (ankerNet) {
        ankerNet->logout();
        ankerNet->closeVideoStream(VIDEO_CLOSE_BY_LOGOUT);
    }    
    ShowUnLoginMenu();
    onLogOut();

    if (m_MsgCentreDialog)
    {
        m_isMsgCenterIsShow = false;
        m_MsgCentreDialog->Hide();           
    }

    ShowUnLoginDevice();
}

void MainFrame::loginFinishHandle()
{
    ANKER_LOG_INFO << "loginFinishHandle enter";
    auto ankerNet = AnkerNetInst();
    if (!ankerNet) {
        return;
    }

    ANKER_LOG_INFO << "login back start";
    std::string url = ankerNet->GetAvatar();
    wxString filePath = wxString();

    setUserInfoForSentry();

    wxStandardPaths standarPaths = wxStandardPaths::Get();
    filePath = standarPaths.GetUserDataDir();
    filePath = filePath + "/cache/" + wxString::FromUTF8(ankerNet->GetUserId()) + ".png";

    auto appConfig = Slic3r::GUI::wxGetApp().app_config;
    if (nullptr == appConfig) {
        ANKER_LOG_INFO << "02mmPrinter  nohint for user set.";
        return;
    }
    appConfig->set("user_id", ankerNet->GetUserId());


#ifndef __APPLE__
    filePath.Replace("\\", "/");
#endif        
    // AnkerMake Studio Profile/cache
    m_avatarPath = filePath;

    //if avatart not exists
    if (!wxFileExists(m_avatarPath) && url.size() > 0) {
        ankerNet->AsyDownLoad(
            url,
            filePath.ToStdString(wxConvUTF8),
            this,
            onDownLoadFinishedCallBack,
            onDownLoadProgress, true);
    }

    {
        wxLogNull logNo;
        wxFile file(m_avatarPath);
        wxFileOffset size = 0;
        if (file.IsOpened()) {
            size = file.Length();
        }

        if (size <= 0 && url.size() > 0)
        {
            ankerNet->AsyDownLoad(
                url,
                filePath.ToStdString(wxConvUTF8),
                this,
                onDownLoadFinishedCallBack,
                onDownLoadProgress, true);
        }

        if (url.size() <= 0)
        {
            m_avatarPath = "nullptr";
        }

        ShowLoginedMenu();
        ANKER_LOG_INFO << "login back finish0";
    }
    updateCurrentEnvironment();
    updateBuryInfo();

    //wxGetApp().filamentMaterialManager()->AsyncUpdate();
    ANKER_LOG_INFO << "login back finish";

    //loginWebview->Close();
    
#ifdef _WIN32  // for windows
    ankerNet->ProcessWebLoginFinish();
#endif
    if (m_pDeviceWidget)
        m_pDeviceWidget->loadDeviceList();



    QueryDataShared(nullptr);
    ANKER_LOG_INFO << "loginFinishHandle leave";
}


void MainFrame::ShowUnLoginDevice()
{
    if (m_pDeviceWidget) {
        m_pDeviceWidget->showUnlogin();
    }
}

void MainFrame::ShowUnLoginMenu(bool dropSession)
{
    ShowUnLoginDevice();

	if (!m_pLoginMenu)
		return;

    ClearLoingiMenu();

    auto ankerNet = AnkerNetInst();
    if (ankerNet && dropSession) {
        ankerNet->logout();
    }
   
    updateCurrentEnvironment();

    updateBuryInfo();

	append_menu_item(m_pLoginMenu,
		wxID_ANY,
		_L("common_toptable_login"),
		_L("Sign In eufyMake"),
		[=](wxCommandEvent&) {                     
            ShowAnkerWebView("login menu button clicked");
		});

    SetWebviewTestItem();
}


void MainFrame::ClearLoingiMenu()
{
    if (!m_pLoginMenu) {
        return;
    }

	int count = m_pLoginMenu->GetMenuItemCount();

	for (int i = 0; i <= count; i++) {
		wxMenuItem* menuItem = m_pLoginMenu->FindItemByPosition(0);
		if (menuItem) {			
            m_pLoginMenu->Remove(menuItem);
            delete menuItem;
		}
	}
}

void MainFrame::onLogOut()
{
    m_isMsgCenterIsShow = false;
    m_MsgCentreDialog->Hide();    
    clearStarCommentData();
    RemovePrivacyChoices();
    if (m_loginWebview)
        m_loginWebview->onLogOut();
}


void MainFrame::OnMove(wxMoveEvent& event)
{
	wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_ANKER_MAINWIN_MOVE);
	ProcessEvent(evt);
}

wxMenu* MainFrame::GetHelpMenu()
{
    if (!m_menubar) {
        return nullptr;
    }
    int menuIndex = m_menubar->FindMenu(_L("common_menu_title_help"));
    if (menuIndex == wxNOT_FOUND) {
        ANKER_LOG_WARNING << "help menu not found";
        return nullptr;
    }
    return m_menubar->GetMenu(menuIndex);
}

void MainFrame::DealPrivacyChoices(const wxCommandEvent& event)
{
    wxIntPtr* clientData = static_cast<wxIntPtr*>(event.GetClientData());
    if (!clientData) {
        ANKER_LOG_WARNING << "client data or menubar is nullptr";
        return;
    }
    auto helpMenu = GetHelpMenu();
    if (!helpMenu) {
        ANKER_LOG_WARNING << "help menu is nullptr";
        return;
    }

    bool isShow = static_cast<bool>(*clientData);       
    auto item = helpMenu->FindItem(ID_PRIVACY_CHOICES_ITEM);

    ANKER_LOG_INFO << "get your privacy choices item: " << item <<", isShow: " << isShow;
    if (item && !isShow) {
        helpMenu->Remove(item);
    }
    if (!item && isShow) {
        const int privacyChoicesItemPos = 2;
        append_menu_item(helpMenu, ID_PRIVACY_CHOICES_ITEM, _L("common_menu_help_privacychoices"), "",
            [](wxCommandEvent&) {
                wxString url = wxString(Slic3r::UrlConfig::PrivacyRequestUrl);
                wxURI uri(url);
                url = uri.BuildURI();
                bool success = wxLaunchDefaultBrowser(url);
        }, "", nullptr, []() { return true; }, nullptr, privacyChoicesItemPos);
    }
    delete clientData;
}

void MainFrame::RemovePrivacyChoices()
{
    auto helpMenu = GetHelpMenu();
    if (!helpMenu) {
        ANKER_LOG_WARNING << "help menu is nullptr";
        return;
    }
    auto item = helpMenu->FindItem(ID_PRIVACY_CHOICES_ITEM);
    if (item) {
        helpMenu->Remove(item);
    }
}

void MainFrame::SetWebviewTestItem()
{
    auto testOpen = wxGetApp().app_config->get_bool("Debug", "open_webview_test");
    if (testOpen) {
        append_menu_item(m_pLoginMenu,
            wxID_ANY,
            "AnkerWeb Test",
            "AnkerWeb Test",
            [=](wxCommandEvent&) {
                TestAnkerWebview();
            });

        append_menu_item(m_pLoginMenu,
            wxID_ANY,
            "Local Browser Test",
            "Local Browser Test",
            [=](wxCommandEvent&) {
                TestLoacalBrowser();
            });
    }
}

void MainFrame::TestAnkerWebview()
{
    wxString defaultUrl = "https://www.google.com";
    wxTextEntryDialog dialog(this, "Enter your url:", "Input Dialog", defaultUrl);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    defaultUrl = dialog.GetValue();
    ANKER_LOG_INFO << "load " << defaultUrl << " to webview for test";

    wxSize loginWebViewSize = AnkerSize(900, 700);
    wxPoint loginWebViewPos = wxPoint((GetSize().x - loginWebViewSize.x) / 2, (GetSize().y - loginWebViewSize.y) / 2);
    std::shared_ptr<AnkerWebView> loginWebview(new AnkerWebView(this, wxID_ANY,
        _L("common_toptable_login"), defaultUrl, loginWebViewPos, loginWebViewSize));

    loginWebview->SetWebViewSize(AnkerSize(900, 700));
    wxPoint winPoint;
    winPoint.x = this->GetRect().x + (GetRect().GetWidth() - loginWebview->GetRect().GetWidth()) / 2;
    winPoint.y = this->GetRect().y + (GetRect().GetHeight() - loginWebview->GetRect().GetHeight()) / 2;
    loginWebview->Move(winPoint);
    loginWebview->Clear();
    loginWebview->ShowModal();
}

void MainFrame::TestLoacalBrowser()
{
    auto url = getLoginUrl();
    ANKER_LOG_INFO << "loacal browser: " << url;
    std::string realUrl = Slic3r::UrlDecode(url.ToUTF8().data());
    wxLaunchDefaultBrowser(realUrl.c_str());
}

void MainFrame::InitDeviceWidget()
{
    DatamangerUi::GetInstance().SetMainWindow(this);
    if (m_pDeviceWidget) {
        m_pDeviceWidget->Init();
    }
}

void MainFrame::ShowAnkerWebView(const std::string& from)
{    
    ANKER_LOG_INFO << from;
    // download ankernet plugin
    if (!DatamangerUi::GetInstance().LoadNetLibrary()) {
        ANKER_LOG_ERROR << "load ankernet plugin failed";
        return;
    }
    InitDeviceWidget();

    ANKER_LOG_INFO << from << ", start call ShowAnkerWebView";
    {
        ANKER_LOG_INFO << "webview start to lock 01 ...";
        //std::unique_lock lock(m_ReadWriteMutex);
        if(m_bIsOpenWebview)
        {
            ANKER_LOG_INFO << "end  call ShowAnkerWebView for opened";
            return;
        }
        ANKER_LOG_INFO << "set webview is open to true";
        m_bIsOpenWebview = true;
    }

    if (m_loginWebview)
        m_loginWebview->SetForceClose(true);

    auto loginwebview = CreateWebView(false);

    ANKER_LOG_INFO<<"before call loginWebview->ShowModal()";
#ifdef _WIN32
    loginwebview->Show();
#else
    loginwebview->ShowModal();
#endif    
    loginwebview->SetShowErrorEnable(false);
    ANKER_LOG_INFO<<"end call loginWebview->ShowModal()";

    if (m_loginWebview) {
        ANKER_LOG_INFO << "before delete webview";
        delete m_loginWebview;
        m_loginWebview = nullptr;
        ANKER_LOG_INFO << "end delete webview";
    }
    m_loginWebview = std::move(loginwebview);
    {
        ANKER_LOG_INFO << "webview start to lock 02 ...";
        //std::unique_lock lock(m_ReadWriteMutex);
        ANKER_LOG_INFO << "set webview is open to false";
        m_bIsOpenWebview = false;
    }

    ANKER_LOG_INFO << from << ", end call ShowAnkerWebView";
}

MainFrame::MainFrame(const int font_point_size) :
DPIFrame(NULL, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "mainframe", font_point_size),
    m_printhost_queue_dlg(new PrintHostQueueDialog(this))
    , m_recent_projects(9)
    , m_settings_dialog(this)
{
    // Fonts were created by the DPIFrame constructor for the monitor, on which the window opened.
    wxGetApp().update_fonts(this);

    AnkerBase ankerBase;
    //Bind(wxEVT_MOVE, wxMoveEventHandler(MainFrame::OnMove));
    Connect(wxEVT_MOVE, wxMoveEventHandler(MainFrame::OnMove));

    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        // add by allen, we must new AnkerConfigDlg in here to solve the problem of item showed in taskbar;

        ANKER_LOG_DEBUG << "createAnkerCfgDlg start";
        createAnkerCfgDlg();
        ANKER_LOG_DEBUG << "createAnkerCfgDlg end";
    });
/*
#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList 
    this->SetFont(this->normal_font());
#endif
    // Font is already set in DPIFrame constructor
*/

#ifdef __APPLE__
    // Initialize the docker task bar icon.
    switch (wxGetApp().get_app_mode()) {
    default:
    case GUI_App::EAppMode::Editor:
        m_taskbar_icon = std::make_unique<AnkerStudioTaskBarIcon>(wxTBI_DOCK);
        m_taskbar_icon->SetIcon(wxIcon(Slic3r::var("AnkerStudio_128px.png"), wxBITMAP_TYPE_PNG), "AnkerStudio");
        break;
    case GUI_App::EAppMode::GCodeViewer:
        m_taskbar_icon = std::make_unique<GCodeViewerTaskBarIcon>(wxTBI_DOCK);
        m_taskbar_icon->SetIcon(wxIcon(Slic3r::var("AnkerStudio-gcodeviewer_128px.png"), wxBITMAP_TYPE_PNG), "G-code Viewer");
        break;
    }
#endif // __APPLE__

    // Load the icon either from the exe, or from the ico file.
    SetIcon(main_frame_icon(wxGetApp().get_app_mode()));
    ANKER_LOG_INFO << "init tab panel";
    initTabPanel();
    BindEvent();
    initAnkerUi();
}

MainFrame::~MainFrame()
{
    if (m_MsgCenterCfg)
    {
        delete m_MsgCenterCfg;
        m_MsgCenterCfg = nullptr;
    }

    if (m_MsgCenterErrCodeInfo)
    {
        delete m_MsgCenterErrCodeInfo;
        m_MsgCenterErrCodeInfo = nullptr;
    }

    // for crash when app exception exit
    if (!m_normalExit) {
        ANKER_LOG_INFO << "Abnormal program exit";
        this->shutdown();
    }
}

void MainFrame::initTabPanel() {
    // initialize status bar
//    m_statusbar = std::make_shared<ProgressStatusBar>(this);
//    m_statusbar->set_font(GUI::wxGetApp().normal_font());
//    if (wxGetApp().is_editor())
//        m_statusbar->embed(this);
//    m_statusbar->set_status_text(_L("Version") + " " +
//        SLIC3R_VERSION + " - " +
//       _L("Remember to check for updates at https://github.com/prusa3d/PrusaSlicer/releases"));

    // initialize tabpanel and menubar
    init_tabpanel();
    // The standalone G-code viewer mode went with the Slice tab.
    init_menubar_as_editor();

#if _WIN32
    // This is needed on Windows to fake the CTRL+# of the window menu when using the numpad
    wxAcceleratorEntry entries[6];
    entries[0].Set(wxACCEL_CTRL, WXK_NUMPAD1, wxID_HIGHEST + 1);
    entries[1].Set(wxACCEL_CTRL, WXK_NUMPAD2, wxID_HIGHEST + 2);
    entries[2].Set(wxACCEL_CTRL, WXK_NUMPAD3, wxID_HIGHEST + 3);
    entries[3].Set(wxACCEL_CTRL, WXK_NUMPAD4, wxID_HIGHEST + 4);
    entries[4].Set(wxACCEL_CTRL, WXK_NUMPAD5, wxID_HIGHEST + 5);
    entries[5].Set(wxACCEL_CTRL, WXK_NUMPAD6, wxID_HIGHEST + 6);
    wxAcceleratorTable accel(6, entries);
    SetAcceleratorTable(accel);
#endif // _WIN32

    // set default tooltip timer in msec
    // SetAutoPop supposedly accepts long integers but some bug doesn't allow for larger values
    // (SetAutoPop is not available on GTK.)
    wxToolTip::SetAutoPop(32767);

    m_loaded = true;

    // initialize layout
    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_main_sizer, 1, wxEXPAND);
    SetSizer(sizer);
    //init function panel

    //by Samuel, only Editor mode should show function tab Control 
    GUI::GUI_App* gui = dynamic_cast<GUI::GUI_App*>(GUI::GUI_App::GetInstance());
    if (gui->get_app_mode() != GUI::GUI_App::EAppMode::GCodeViewer)
    {
        wxSize panelSize = GetSize();
        panelSize.SetHeight(36);
        m_pFunctionPanel = new AnkerFunctionPanel(this, wxID_ANY);
        m_pFunctionPanel->Bind(wxCUSTOMEVT_FEEDBACK, [this](wxCommandEvent& event) {
            auto ankerNet = AnkerNetInst();
            if (!ankerNet || !ankerNet->IsLogined()) {
                wxGetApp().mainframe->ShowAnkerWebView("feedback button clicked");
            }
            else {
                wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
                wxSize mfSize = wxGetApp().mainframe->GetClientSize();
                wxSize dialogSize = AnkerSize(400, 404);
                wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2, mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);
                wxString title = _L("common_feedback_title");
                AnkerFeedbackDialog dialog(nullptr, title.ToStdString(), center, dialogSize);
                if (dialog.ShowModal() == wxID_OK) {
                    auto feedback = dialog.GetFeedBack();
                    ankerNet->AsyPostFeedBack(feedback);
                }
            }
            });
        m_pFunctionPanel->Bind(wxCUSTOMEVT_SHOW_DOC, [this](wxCommandEvent& event) {
            //std::string realUrl = "";
            //wxLaunchDefaultBrowser(realUrl.c_str());
            });
        m_pFunctionPanel->Bind(wxCUSTOMEVT_RELEASE_NOTE, [this](wxCommandEvent& event) {
            wxLaunchDefaultBrowser(Slic3r::UrlConfig::ReleaseUrl.c_str());
            });

        m_pFunctionPanel->SetMinSize(AnkerSize(0, 36));
        m_main_sizer->Add(m_pFunctionPanel, 0, wxEXPAND, 0);
        m_pFunctionPanel->Show();
        m_pFunctionPanel->SetPrintTab(m_printTabPanel);

        m_pFunctionPanel->Bind(wxCUSTOMEVT_SHOW_MSG_CENTRE, [=](wxCommandEvent& ev) {
            ANKER_LOG_INFO << "show msg centre";

            auto ankerNet = AnkerNetInst();
            if (!ankerNet || !ankerNet->IsLogined()) {
                wxGetApp().mainframe->ShowAnkerWebView("msg center request to loiginb ");
            }

            if(ankerNet->IsLogined())
            {
                wxPoint winPoint;
                winPoint.x = this->GetRect().x + (GetRect().GetWidth() - m_MsgCentreDialog->GetRect().GetWidth()) / 2;
                winPoint.y = this->GetRect().y + (GetRect().GetHeight() - m_MsgCentreDialog->GetRect().GetHeight()) / 2;

                wxVariant* pData = (wxVariant*)(ev.GetClientData());
                
                bool isShowOfficical = true;
                if (pData)
                {
                    wxVariantList list = pData->GetList();
                    isShowOfficical = list[0]->GetBool();                    
                }

                if (m_MsgCentreDialog)
                {                            
                    m_MsgCentreDialog->clearMsg();
                    m_MsgCentreDialog->Move(winPoint);                    
                    m_MsgCentreDialog->Raise();                    
                    m_isMsgCenterIsShow = true;                    
                    m_MsgCentreDialog->Show();
                    m_MsgCentreDialog->getMsgCenterRecords(true);
                }
            }
            });
    }
    m_sliceCommentDialog = new AnkerSliceCommentDialog(this, _L("Rate Your Experience"));
    m_sliceCommentDialog->SetMaxSize(AnkerSize(400,420));
    m_sliceCommentDialog->SetMinSize(AnkerSize(400, 420));
    m_sliceCommentDialog->SetSize(AnkerSize(400, 420));
    m_sliceCommentDialog->Hide();
    m_sliceCommentDialog->Bind(wxCUSTOMEVT_ANKER_COMMENT_NOT_ASK, [this](wxCommandEvent& event) {
        g_sliceCommentData.action = 3;        
        auto ankerNet = AnkerNetInst();
        if (ankerNet && ankerNet->IsLogined()) {
            ankerNet->reportCommentData(g_sliceCommentData);
        }
        });
    m_sliceCommentDialog->Bind(wxCUSTOMEVT_ANKER_COMMENT_SUBMIT, [this](wxCommandEvent& event) {
        g_sliceCommentData.action = 1;      
        wxVariant* pData = (wxVariant*)event.GetClientData();
        if (pData) {
            wxVariantList list = pData->GetList();
            g_sliceCommentData.rating = list[0]->GetInteger();
            g_sliceCommentData.reviewData = list[1]->GetString().utf8_str();
        }
        
        auto ankerNet = AnkerNetInst();
        if (ankerNet && ankerNet->IsLogined()) {
            ankerNet->reportCommentData(g_sliceCommentData);
        }
        });
    m_sliceCommentDialog->Bind(wxCUSTOMEVT_ANKER_COMMENT_CLOSE, [this](wxCommandEvent& event) {
        g_sliceCommentData.action = 2;        
        auto ankerNet = AnkerNetInst();
        if (ankerNet && ankerNet->IsLogined()) {
            ankerNet->reportCommentData(g_sliceCommentData);
        }
        });
    // initialize layout from config
    update_layout();
    sizer->SetSizeHints(this);
    Fit();

    const wxSize min_size = wxGetApp().get_min_size(); //wxSize(76*wxGetApp().em_unit(), 49*wxGetApp().em_unit());
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    SetSize(min_size/*wxSize(760, 490)*/);
    // mod by allen for setting the min size of mainframe
    SetMinSize(min_size/*wxSize(760, 490)*/);
#else

    SetMinSize(min_size/*wxSize(760, 490)*/);
    SetSize(GetMinSize());
#endif
    Layout();

    update_title();

    // declare events
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        // The gizmo-editing veto, the save-dirty-project prompt and the
        // acode-export interlock all went with the slicer. Nothing can be dirty
        // and nothing exports, so closing is unconditional.
        if (event.CanVeto() && !wxGetApp().check_print_host_queue()) {
            event.Veto();
            return;
        }

        //report: exit soft
        std::string durationStr = getWorkDuration();
        std::string errorCode = std::string("0");
        std::string errorMsg = std::string("exit soft");

        std::map<std::string, std::string> map;
        map.insert(std::make_pair(c_es_error_code, errorCode));
        map.insert(std::make_pair(c_es_error_msg, errorMsg));
        map.insert(std::make_pair(c_exit_startup_duration, durationStr));
        ANKER_LOG_INFO << "Report bury event is " << e_exit_soft;
        reportBuryEvent(e_exit_soft, map, true);

        this->shutdown();
        m_normalExit = true;
        // propagate event
        event.Skip();
        wxExit();
        });

    // EXPORT_FINISHED_SAFE_QUIT_APP is unreachable: nothing exports acode.

    //FIXME it seems this method is not called on application start-up, at least not on Windows. Why?
    // The same applies to wxEVT_CREATE, it is not being called on startup on Windows.
    Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& event) {
        event.Skip();
        });

    Bind(wxEVT_SIZE, &MainFrame::on_size, this);
    Bind(wxEVT_MOVE, &MainFrame::on_move, this);
    Bind(wxEVT_SHOW, &MainFrame::on_show, this);
    Bind(wxEVT_ICONIZE, &MainFrame::on_minimize, this);
    Bind(wxEVT_MAXIMIZE, &MainFrame::on_maximize, this);
    Bind(wxEVT_ACTIVATE, &MainFrame::on_Activate, this);

    //
    m_printTabPanel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& event) {
        int iSelectedPage = event.GetId();

        // Device is the only tab now, so it is page 0 (it used to be page 1, behind
        // the plater). Slice is gone.
        if (iSelectedPage == 1) {
            // Device Details follows whichever printer the Device tab has selected.
            if (m_pDeviceDetails) {
                // wxSimplebook does not reliably send wxEVT_SHOW to its pages, so
                // re-read the device list here too -- it is populated asynchronously.
                m_pDeviceDetails->refreshPrinterList();
                // Seed only. The Device tab's "current" serial is derived from its
                // widget list and can disagree with what is highlighted there, so
                // forcing it on every tab switch silently retargeted the controls at
                // a printer the user had not chosen.
                if (m_pDeviceWidget)
                    m_pDeviceDetails->seedSelection(m_pDeviceWidget->currentDeviceSn());
            }
        }
        else if (iSelectedPage == 0) {
            m_currentTabMode = TabMode::TAB_DEVICE;
            if (m_pMsgCentrePopWindow)
                m_pMsgCentrePopWindow->Hide();
            if (m_hasErrDialog)
            {
                ShowErrDialogByCenter();
            }
            auto ankerNet = AnkerNetInst();
            if (ankerNet && ankerNet->IsLogined()) {
                ankerNet->AsyRefreshDeviceList();
            }
        }

        m_printTabPanel->SetSelection(iSelectedPage);
        });

    // OSX specific issue:
    // When we move application between Retina and non-Retina displays, The legend on a canvas doesn't redraw
    // So, redraw explicitly canvas, when application is moved
    //FIXME maybe this is useful for __WXGTK3__ as well?
    // The Retina move-redraw workaround drove the GL canvas, which is gone.

    wxGetApp().persist_window_geometry(this, true);
    wxGetApp().persist_window_geometry(&m_settings_dialog, true);

    // update_ui_from_settings() removed with the plater/preset UI

    // The collapse toolbar and action buttons went with the 3D view; the preferences
    // dialog did not, and was only ever gated on the plater by accident of nesting.
    preferences_dialog = new PreferencesDialog(this);

    // bind events from DiffDlg

    // bind_diff_dialog() removed with the preset tabs
}

void MainFrame::setUserInfoForSentry()
{    
    
#ifdef WIN32
    auto ankerNet = AnkerNetInst();
    if(ankerNet->IsLogined())
    {    
        std::string userEmail = ankerNet->GetUserEmail();
        std::string nickName = ankerNet->GetNickName();
      
#ifndef OPEN_SOURCE_MODE
        sentry_set_tag("email", userEmail.c_str());
        sentry_set_tag("nick_name", nickName.c_str());
#endif    
    }    

#endif
}


void MainFrame::createAnkerCfgDlg() {
    // The Anker config dialog was purely print/filament/printer preset editing,
    // which this fork does not do. Nothing is constructed, so m_ankerCfgDlg stays
    // null and every use of it elsewhere is already null-guarded and inert.
}

void MainFrame::OnDocumentLoaded(wxWebViewEvent& evt)
{
    //web load finisehd
}


void MainFrame::OnScriptMessage(wxCommandEvent& evt)
{
    //on rec web request
}




size_t MainFrame::onDownLoadFinishedCallBack(char* dest, size_t size, size_t nmemb, void* userp)
{
	return size * nmemb;    
}


void MainFrame::onDownLoadProgress(double dltotal, double dlnow, double ultotal, double ulnow)
{
    
}


std::string MainFrame::getAppName()
{
    std::string strAppname = std::string();

#ifdef _WIN32
	WCHAR buffer[MAX_PATH] = { 0 };
	if (GetCurrentDirectory(MAX_PATH, buffer))
	{
		std::wstring wstrPath = buffer;
		std::string strAppname(wstrPath.begin(), wstrPath.end());
		int pos = strAppname.find_last_of("\\");
        strAppname = strAppname.substr(pos + 1, strAppname.size());
	}
#endif
    return strAppname;
}

void MainFrame::updateCurrentEnvironment()
{
    auto ankerNet = AnkerNetInst();
    if (!ankerNet || !ankerNet->IsLogined()) 
    {
        wxString url = AnkerConfig::getankerDomainUrl();
        if (url.Contains(wxT("qa")))
            m_currentEnvir = QA_ENVIR;        
        else if (url.Contains(wxT("ci")))
            m_currentEnvir = CI_ENVIR;
        else
            m_currentEnvir = US_ENVIR;//DEFAULT
        return;
    }

    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj)
    {
        auto currentEnv = obj->getCurrentEnvironmentType();
        m_currentEnvir = ANKER_ENVIR(currentEnv);
    }

}
void MainFrame::updateBuryInfo()
{
    auto para = DatamangerUi::GetInstance().GetNetPara();
    std::string envir = "US";
    std::string userInfo = std::string();
    std::string userId = std::string();
    if (m_currentEnvir == EU_ENVIR)
        envir = "EU";
    else if (m_currentEnvir == QA_ENVIR)
        envir = "QA";
    else if(m_currentEnvir == US_ENVIR)
        envir = "US";
    else
        envir = "CI";
    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj)
    {
        userInfo = obj->GetUserInfo();
        userId = obj->GetUserId();
    }
    setPluginParameter(userInfo, envir, userId, para.Openudid);
}

#ifdef _MSW_DARK_MODE
static wxString pref() { return " [ "; }
static wxString suff() { return " ] "; }
static void append_tab_menu_items_to_menubar(wxMenuBar* bar, PrinterTechnology pt, bool is_mainframe_menu)
{
    if (is_mainframe_menu)
        bar->Append(new wxMenu(), pref() + _L("Plater") + suff());
    for (const wxString& title : { is_mainframe_menu    ? _L("Print Settings")       : pref() + _L("Print Settings") + suff(),
                                   pt == ptSLA          ? _L("Material Settings")    : _L("Filament Settings"),
                                   _L("Printer Settings") })
        bar->Append(new wxMenu(), title);
}

// update markers for selected/unselected menu items
static void update_marker_for_tabs_menu(wxMenuBar* bar, const wxString& title, bool is_mainframe_menu)
{
    if (!bar)
        return;
    size_t items_cnt = bar->GetMenuCount();
    for (size_t id = items_cnt - (is_mainframe_menu ? 4 : 3); id < items_cnt; id++) {
        wxString label = bar->GetMenuLabel(id);
        if (label.First(pref()) == 0) {
            if (label == pref() + title + suff())
                return;
            label.Remove(size_t(0), pref().Len());
            label.RemoveLast(suff().Len());
            bar->SetMenuLabel(id, label);
            break;
        }
    }
    if (int id = bar->FindMenu(title); id != wxNOT_FOUND)
        bar->SetMenuLabel(id, pref() + title + suff());
}

static void add_tabs_as_menu(wxMenuBar* bar, MainFrame* main_frame, wxWindow* bar_parent)
{
    PrinterTechnology pt = ptFFF;

    bool is_mainframe_menu = bar_parent == main_frame;
    if (!is_mainframe_menu)
        append_tab_menu_items_to_menubar(bar, pt, is_mainframe_menu);

    bar_parent->Bind(wxEVT_MENU_OPEN, [main_frame, bar, is_mainframe_menu](wxMenuEvent& event) {
        wxMenu* const menu = event.GetMenu();
        if (!menu || menu->GetMenuItemCount() > 0) {
            // If we are here it means that we open regular menu and not a tab used as a menu
            event.Skip(); // event.Skip() is verry important to next processing of the wxEVT_UPDATE_UI by this menu items.
                          // If wxEVT_MENU_OPEN will not be pocessed in next event queue then MenuItems of this menu will never caught wxEVT_UPDATE_UI 
                          // and, as a result, "check/radio value" will not be updated
            return;
        }

        // update tab selection

        const wxString& title = menu->GetTitle();
        if (title == _L("Plater"))
            main_frame->select_tab(size_t(0));
        // The preset settings tabs are gone, so there is nothing else to select.

        // update markers for selected/unselected menu items
        update_marker_for_tabs_menu(bar, title, is_mainframe_menu);
    });
}

void MainFrame::show_tabs_menu(bool show)
{
    if (show)
        append_tab_menu_items_to_menubar(m_menubar, ptFFF, true);
    else
        while (m_menubar->GetMenuCount() >= 8) {
            if (wxMenu* menu = m_menubar->Remove(7))
                delete menu;
        }
}
#endif // _MSW_DARK_MODE

void MainFrame::update_layout()
{
    auto restore_to_creation = [this]() {
        auto clean_sizer = [](wxSizer* sizer) {
            while (!sizer->GetChildren().IsEmpty()) {
                sizer->Detach(0);
            }
        };

        if (m_tabpanel->GetParent() != this)
            m_tabpanel->Reparent(this);

        if (m_printTabPanel->GetParent() != this)
            m_printTabPanel->Reparent(this);

        clean_sizer(m_main_sizer);
        clean_sizer(m_settings_dialog.GetSizer());

        if (m_settings_dialog.IsShown())
            m_settings_dialog.Close();

        m_tabpanel->Hide();

        Layout();
    };

    // Dlg is the only layout this fork has: the Old/New variants put the plater in
    // the settings notebook, and GCodeViewer was the standalone toolpath viewer.
    // Both went with the Slice tab.
    ESettingsLayout layout = ESettingsLayout::Dlg;

    if (m_layout == layout)
        return;

    wxBusyCursor busy;

    Freeze();

    // Remove old settings
    if (m_layout != ESettingsLayout::Unknown)
        restore_to_creation();

#ifdef __WXMSW__
    enum class State {
        noUpdate,
        fromDlg,
        toDlg
    };
    State update_scaling_state = //m_layout == ESettingsLayout::Unknown   ? State::noUpdate   : // don't scale settings dialog from the application start
                                 m_layout == ESettingsLayout::Dlg       ? State::fromDlg    :
                                 layout   == ESettingsLayout::Dlg       ? State::toDlg      : State::noUpdate;
#endif //__WXMSW__
    m_printTabPanel->Show();

    ESettingsLayout old_layout = m_layout;
    m_layout = layout;

    // From the very beginning the Print settings should be selected
    m_last_selected_tab = m_layout == ESettingsLayout::Dlg ? 0 : 1;
   
    // Set new settings
    switch (m_layout)
    {
    case ESettingsLayout::Unknown:
    {
        break;
    }
    case ESettingsLayout::Dlg:
    {
        // The Slice tab is gone: the plater is no longer a page of m_printTabPanel,
        // which leaves Device as the only page, at index 0. The Plater object is
        // still constructed and still reachable through wxGetApp().plater() -- a
        // great deal of code dereferences it -- it is simply never shown.
        m_currentTabMode = TAB_DEVICE;
        if(m_pMsgCentrePopWindow)
            m_pMsgCentrePopWindow->Hide();
        m_printTabPanel->SetSelection(0);
        //by Samuel, printTabPanel no need show default border
        m_main_sizer->Add(m_printTabPanel, 1, wxEXPAND | wxTOP, 0);
        m_printTabPanel->Show();
      
        m_tabpanel->Reparent(&m_settings_dialog);
        m_settings_dialog.GetSizer()->Add(m_tabpanel, 1, wxEXPAND | wxTOP, 2);
        m_tabpanel->Show();

#ifdef _MSW_DARK_MODE
        if (wxGetApp().tabs_as_menu())
            show_tabs_menu(false);
#endif
        break;
    }
    }

#ifdef _MSW_DARK_MODE
    // Sizer with buttons for mode changing
    //m_plater->sidebar().show_mode_sizer(wxGetApp().tabs_as_menu() || m_layout != ESettingsLayout::Old);
#endif

#ifdef __WXMSW__
    if (update_scaling_state != State::noUpdate)
    {
        int mainframe_dpi   = get_dpi_for_window(this);
        int dialog_dpi      = get_dpi_for_window(&m_settings_dialog);
        if (mainframe_dpi != dialog_dpi) {
            wxSize oldDPI = update_scaling_state == State::fromDlg ? wxSize(dialog_dpi, dialog_dpi) : wxSize(mainframe_dpi, mainframe_dpi);
            wxSize newDPI = update_scaling_state == State::toDlg   ? wxSize(dialog_dpi, dialog_dpi) : wxSize(mainframe_dpi, mainframe_dpi);

            if (update_scaling_state == State::fromDlg)
                this->enable_force_rescale();
            else
                (&m_settings_dialog)->enable_force_rescale();

            wxWindow* win { nullptr };
            if (update_scaling_state == State::fromDlg)
                win = this;
            else
                win = &m_settings_dialog;

#if wxVERSION_EQUAL_OR_GREATER_THAN(3,1,3)
            m_tabpanel->MSWUpdateOnDPIChange(oldDPI, newDPI);
            win->GetEventHandler()->AddPendingEvent(wxDPIChangedEvent(oldDPI, newDPI));
#else
            win->GetEventHandler()->AddPendingEvent(DpiChangedEvent(EVT_DPI_CHANGED_SLICER, newDPI, win->GetRect()));
#endif // wxVERSION_EQUAL_OR_GREATER_THAN
        }
    }
#endif //__WXMSW__

//#ifdef __APPLE__
//    // Using SetMinSize() on Mac messes up the window position in some cases
//    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
//    // So, if we haven't possibility to set MinSize() for the MainFrame, 
//    // set the MinSize() as a half of regular  for the m_plater and m_tabpanel, when settings layout is in slNew mode
//    // Otherwise, MainFrame will be maximized by height
//    if (m_layout == ESettingsLayout::New) {
//        wxSize size = wxGetApp().get_min_size();
//        size.SetHeight(int(0.5 * size.GetHeight()));
//        m_plater->SetMinSize(size);
//        m_tabpanel->SetMinSize(size);
//    }
//#endif

#ifdef __APPLE__
    //m_plater->sidebar().change_top_border_for_mode_sizer(m_layout != ESettingsLayout::Old);
#endif
    
    Layout();
    Thaw();
}


void MainFrame::setUrl(std::string webUrl)
{
    if (webUrl.empty())
        return;

    auto currentLanguage = Slic3r::GUI::wxGetApp().getCurrentLanguageType();
    wxString languageFlags = "en";
    //if (currentLanguage <= wxLANGUAGE_ENGLISH_ZIMBABWE && currentLanguage >= wxLANGUAGE_ENGLISH)  

    if (currentLanguage <= wxLANGUAGE_JAPANESE_JAPAN && currentLanguage >= wxLANGUAGE_JAPANESE)        
        languageFlags = "ja";
    else if ( (currentLanguage <= wxLANGUAGE_CHINESE_MACAU && currentLanguage >= wxLANGUAGE_CHINESE_SIMPLIFIED)||
              (currentLanguage <= wxLANGUAGE_CHINESE_TRADITIONAL_EXPLICIT && currentLanguage >= wxLANGUAGE_CHINESE)
            )
        languageFlags = "cn";
    

    m_loginUrl = webUrl+"?language="+ languageFlags;
    m_backloginUrl = webUrl + "?invisible=true";
}

// Called when closing the application and when switching the application language.
void MainFrame::shutdown(bool restart)
{
    ANKER_LOG_INFO << "MainFrame::shutdown() begin.";

#ifdef _WIN32
	if (m_hDeviceNotify) {
		::UnregisterDeviceNotification(HDEVNOTIFY(m_hDeviceNotify));
		m_hDeviceNotify = nullptr;
	}
 	if (m_ulSHChangeNotifyRegister) {
        SHChangeNotifyDeregister(m_ulSHChangeNotifyRegister);
        m_ulSHChangeNotifyRegister = 0;
 	}
#endif // _WIN32

    // add by allen for ankerCfgDlg
    if (m_ankerCfgDlg && m_ankerCfgDlg->IsShown()) {
        ;   // the Anker config dialog went with the preset tabs
    }

    // Plater teardown (job worker, canvas event unbinding, canvas volumes) went
    // with the 3D canvas -- there are no canvases to unbind or clear.

    // Weird things happen as the Paint messages are floating around the windows being destructed.
    // Avoid the Paint messages by hiding the main window.
    // Also the application closes much faster without these unnecessary screen refreshes.
    // In addition, there were some crashes due to the Paint events sent to already destructed windows.
    this->Show(false);

    if (m_settings_dialog.IsShown())
        // call Close() to trigger call to lambda defined into GUI_App::persist_window_geometry()
        m_settings_dialog.Close();

    // The 3DConnexion mouse controller drove the 3D canvas; it goes with it.

    // Stop the background thread of the removable drive manager, so that no new updates will be sent to the Plater.
    wxGetApp().removable_drive_manager()->shutdown();
	//stop listening for messages from other instances
	wxGetApp().other_instance_message_handler()->shutdown(this);
    // Save the slic3r.ini.Usually the ini file is saved from "on idle" callback,
    // but in rare cases it may not have been called yet.
    if (wxGetApp().app_config->dirty())
        wxGetApp().app_config->save();
//         if (m_plater)
//             m_plater->print = undef;
//         Slic3r::GUI::deregister_on_request_update_callback();

    // set to null tabs and a plater
    // to avoid any manipulations with them from App->wxEVT_IDLE after of the mainframe closing 
#if SHOW_OLD_SETTING_DIALOG
#endif
   
    // add by allen for ankerCfgDlg
    wxGetApp().ankerTabsList.clear();


    CloseVideoStream(VIDEO_CLOSE_BY_APP_QUIT);
    DatamangerUi::GetInstance().ResetMainObj();

    auto ankerNet = AnkerNetInst();
    if (ankerNet && restart == false) {
        ankerNet->UnInit();
    }
    ANKER_LOG_INFO << "MainFrame::shutdown() end.";
}



void MainFrame::update_title()
{
    // The title used to be prefixed with the project file name and a "*" dirty
    // marker. There are no projects to open, name or dirty, so the title is just
    // the application name.
    wxString title = wxEmptyString;

    std::string build_id = "M5 FDM Control";

    size_t 		idx_plus = build_id.find('+');
    if (idx_plus != build_id.npos) {
    	// Parse what is behind the '+'. If there is a number, then it is a build number after the label, and full build ID is shown.
    	int commit_after_label;
    	if (! boost::starts_with(build_id.data() + idx_plus + 1, "UNKNOWN") && 
            (build_id.at(idx_plus + 1) == '-' || sscanf(build_id.data() + idx_plus + 1, "%d-", &commit_after_label) == 0)) {
    		// It is a release build.
    		build_id.erase(build_id.begin() + idx_plus, build_id.end());    		
#if defined(_WIN32) && ! defined(_WIN64)
    		// People are using 32bit slicer on a 64bit machine by mistake. Make it explicit.
            build_id += " 32 bit";
#endif
    	}
    }

    title += wxString(build_id);
    // The "Based on PrusaSlicer" suffix is gone from the title bar. The upstream
    // attribution itself stays in the About dialog, the README and the LICENSE,
    // where AGPL-3.0 requires it.

    SetTitle(title);
}


void MainFrame::OnOtaTimer(wxTimerEvent& event)
{
    auto ankerNet = AnkerNetInst();
    if (!ankerNet) {
        return;
    }
    ANKER_LOG_INFO << "MainFrame::OnOtaTimer.";
    ankerNet->SetOtaCheckType(OtaCheckType_24Hours);
    ankerNet->queryOTAInformation();
}


void MainFrame::OnHttpConnectError(wxCommandEvent& event)
{
    wxVariant* pData = (wxVariant*)event.GetClientData();

    wxSize dialogSize = AnkerSize(400, 185);
    wxPoint parentCenterPoint(this->GetPosition().x + this->GetSize().GetWidth() / 2,
        this->GetPosition().y + this->GetSize().GetHeight() / 2);
    wxPoint dialogPos = wxPoint(parentCenterPoint.x - dialogSize.x / 2, parentCenterPoint.y - dialogSize.y / 2);
    wxString title = _L("common_http_connect_error_title");
    wxString content = _L("common_http_connect_error_content");
    //if (pData)
    //{
    //    wxVariantList list = pData->GetList();
    //    auto key = list[0]->GetString().ToStdString();
    //    content += "\nError Code = " + key;
    //}

    AnkerDialog dialog(this, wxID_ANY, title, content, dialogPos, dialogSize);
    auto result = dialog.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog);
}

void MainFrame::BindEvent()
{
#ifdef WIN32
    Bind(wxCUSTOMEVT_EDIT_ENTER, [this](wxCommandEvent& event) {
        this->SetFocus();
        });
#endif // DEBUG
}

void MainFrame::init_tabpanel()
{
    wxGetApp().update_ui_colours_from_appconfig();        
    m_otaTimer = new wxTimer(this, wxID_ANY);
    m_otaTimer->Start(24 * 60 * 60 * 1000); // 24 hours
    Bind(wxEVT_TIMER, &MainFrame::OnOtaTimer, this, m_otaTimer->GetId());
    m_extrusionTimer = new wxTimer(this, wxID_ANY);
    Bind(wxEVT_TIMER, [this](wxTimerEvent& event)
    {
            CallAfter([this]()
                {
                    ShowAnkerWebView("timer execute for show webview");
                });
    }, m_extrusionTimer->GetId());

    // wxNB_NOPAGETHEME: Disable Windows Vista theme for the Notebook background. The theme performance is terrible on Windows 10
    // with multiple high resolution displays connected.
#ifdef _MSW_DARK_MODE
    if (wxGetApp().tabs_as_menu()) {
        m_tabpanel = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
        m_printTabPanel = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
        wxGetApp().UpdateDarkUI(m_tabpanel);
        wxGetApp().UpdateDarkUI(m_printTabPanel);
    }
    else {
        m_tabpanel = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
        m_printTabPanel = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
    }
        
#else
    m_tabpanel = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
    m_printTabPanel = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
#endif

#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
    m_tabpanel->SetFont(Slic3r::GUI::wxGetApp().normal_font());
    m_printTabPanel->SetFont(Slic3r::GUI::wxGetApp().normal_font());
#endif
    m_tabpanel->Hide();
    m_printTabPanel->Hide();
    m_settings_dialog.set_tabpanel(m_tabpanel);

    // The page-changed handler for m_tabpanel is gone with the preset tabs: it
    // validated custom G-code on the outgoing Tab and filtered pages by printer
    // technology. m_tabpanel now never has any pages.

    // No Plater. It was still being constructed and hidden purely so that the
    // slicing-era code paths in GUI_App had something to point at; nothing renders
    // it, and the "rate us after N slices" prompt it drove has no meaning here.

    // Builds the Device and Device Details pages and binds the account/OTA/HTTP
    // handlers. Without this the tab bar still draws and the pages are empty.
    initDeviceTabs();
}

void MainFrame::getwebLoginDataBack(const std::string& from)
{
    if (!AnkerNetInst()) {
        return;
    }

    ANKER_LOG_INFO << from;
    if (!m_loginWebview) {
        ANKER_LOG_INFO << "create background webview";
        m_loginWebview = std::move(CreateWebView(true));
    }
    m_loginWebview->Hide();
}

AnkerWebView* MainFrame::CreateWebView(bool background)
{
    wxSize loginWebViewSize = AnkerSize(900, 700);
    wxPoint loginWebViewPos = wxPoint((GetSize().x - loginWebViewSize.x) / 2, (GetSize().y - loginWebViewSize.y) / 2);
    auto loginWebview = new AnkerWebView(this, wxID_ANY, _L("common_toptable_login"),
        getLoginUrl(), loginWebViewPos, loginWebViewSize, background);

    loginWebview->Bind(wxCUSTOMEVT_DEAL_PRIVACY_CHOICES, [this](wxCommandEvent& ev) {
        DealPrivacyChoices(ev);
        });

    //use old logic for mac 
    //login finish
    loginWebview->Bind(wxCUSTOMEVT_WEB_LOGIN_FINISH, [=](wxEvent& ev) {
#ifdef _WIN32
        wxObject* eventObject = ev.GetEventObject();
        AnkerWebView* dialog = dynamic_cast<AnkerWebView*>(eventObject);
        if (dialog) {
            ANKER_LOG_INFO << "START CALL WEBVIEW Hide 222";
            //dialog->SetForceClose(true);
            dialog->Hide();
            ANKER_LOG_INFO << "START CALL WEBVIEW Close";
            dialog->Close();
            ANKER_LOG_INFO << "END CALL WEBVIEW Close";
        }
        else {
            ANKER_LOG_INFO << "webview dialog is null";
        }
#endif

        auto ankerNet = AnkerNetInst();
        if (!ankerNet) {
            return;
        }

        ANKER_LOG_INFO << "login back start";
        std::string url = ankerNet->GetAvatar();
        wxString filePath = wxString();

        setUserInfoForSentry();

        wxStandardPaths standarPaths = wxStandardPaths::Get();
        filePath = standarPaths.GetUserDataDir();
        filePath = filePath + "/cache/" + wxString::FromUTF8(ankerNet->GetUserId()) + ".png";

        auto appConfig = Slic3r::GUI::wxGetApp().app_config;
        if (nullptr == appConfig) {
            ANKER_LOG_INFO << "02mmPrinter  nohint for user set.";
            return;
        }
        appConfig->set("user_id",ankerNet->GetUserId());

#ifndef __APPLE__
        filePath.Replace("\\", "/");
#endif        
        // AnkerMake Studio Profile/cache
        m_avatarPath = filePath;

        //if avatart not exists
        if (!wxFileExists(m_avatarPath) && url.size() > 0) {
            ankerNet->AsyDownLoad(
                url,
                filePath.ToStdString(wxConvUTF8),
                this,
                onDownLoadFinishedCallBack,
                onDownLoadProgress, true);
        }

        {
            wxLogNull logNo;
            wxFile file(m_avatarPath);
            wxFileOffset size = 0;
            if (file.IsOpened()) {
                size = file.Length();
            }

            if (size <= 0 && url.size() > 0)
            {
                ankerNet->AsyDownLoad(
                    url,
                    filePath.ToStdString(wxConvUTF8),
                    this,
                    onDownLoadFinishedCallBack,
                    onDownLoadProgress, true);
            }

            if (url.size() <= 0)
            {
                m_avatarPath = "nullptr";
            }

            ShowLoginedMenu();
            ANKER_LOG_INFO << "login back finish0";
        }

        updateCurrentEnvironment();
        updateBuryInfo();

        if (m_pDeviceWidget)
            m_pDeviceWidget->loadDeviceList();

        //wxGetApp().filamentMaterialManager()->AsyncUpdate();
        ANKER_LOG_INFO << "login back finish";
        },
        loginWebview->GetId());

    loginWebview->Bind(wxCUSTOMEVT_WEB_LOGOUT_FINISH, [=](wxEvent& ev) {
        ANKER_LOG_INFO << "BEGIN INVOKE wxCUSTOMEVT_WEB_LOGOUT_FINISH";
        ShowUnLoginMenu();
        ShowUnLoginDevice();
        onLogOut();
        ANKER_LOG_INFO << "END INVOKE wxCUSTOMEVT_WEB_LOGOUT_FINISH";
        }, loginWebview->GetId());

    if (background == false)
    {
        loginWebview->SetWebViewSize(AnkerSize(900, 700));
        wxPoint winPoint;
        winPoint.x = this->GetRect().x + (GetRect().GetWidth() - loginWebview->GetRect().GetWidth()) / 2;
        winPoint.y = this->GetRect().y + (GetRect().GetHeight() - loginWebview->GetRect().GetHeight()) / 2;
        loginWebview->Move(winPoint);
    }

    return loginWebview;
}

#ifdef WIN32
void MainFrame::register_win32_callbacks()
{
    //static GUID GUID_DEVINTERFACE_USB_DEVICE  = { 0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED };
    //static GUID GUID_DEVINTERFACE_DISK        = { 0x53f56307, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b };
    //static GUID GUID_DEVINTERFACE_VOLUME      = { 0x71a27cdd, 0x812a, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f };
    static GUID GUID_DEVINTERFACE_HID           = { 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 };

    // Register USB HID (Human Interface Devices) notifications to trigger the 3DConnexion enumeration.
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_HID;
    m_hDeviceNotify = ::RegisterDeviceNotification(this->GetHWND(), &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

// or register for file handle change?
//      DEV_BROADCAST_HANDLE NotificationFilter = { 0 };
//      NotificationFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
//      NotificationFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;

    // Using Win32 Shell API to register for media insert / removal events.
    LPITEMIDLIST ppidl;
    if (SHGetSpecialFolderLocation(this->GetHWND(), CSIDL_DESKTOP, &ppidl) == NOERROR) {
        SHChangeNotifyEntry shCNE;
        shCNE.pidl       = ppidl;
        shCNE.fRecursive = TRUE;
        // Returns a positive integer registration identifier (ID).
        // Returns zero if out of memory or in response to invalid parameters.
        m_ulSHChangeNotifyRegister = SHChangeNotifyRegister(this->GetHWND(),        // Hwnd to receive notification
            SHCNE_DISKEVENTS,                                                       // Event types of interest (sources)
            SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED,
            //SHCNE_UPDATEITEM,                                                     // Events of interest - use SHCNE_ALLEVENTS for all events
            WM_USER_MEDIACHANGED,                                                   // Notification message to be sent upon the event
            1,                                                                      // Number of entries in the pfsne array
            &shCNE);                                                                // Array of SHChangeNotifyEntry structures that 
                                                                                    // contain the notifications. This array should 
                                                                                    // always be set to one when calling SHChnageNotifyRegister
                                                                                    // or SHChangeNotifyDeregister will not work properly.
        assert(m_ulSHChangeNotifyRegister != 0);    // Shell notification failed
    } else {
        // Failed to get desktop location
        assert(false); 
    }

    {
        static constexpr int device_count = 1;
        RAWINPUTDEVICE devices[device_count] = { 0 };
        // multi-axis mouse (SpaceNavigator, etc.)
        devices[0].usUsagePage = 0x01;
        devices[0].usUsage = 0x08;
        if (! RegisterRawInputDevices(devices, device_count, sizeof(RAWINPUTDEVICE)))
            BOOST_LOG_TRIVIAL(error) << "RegisterRawInputDevices failed";
    }
}
#endif // _WIN32

bool MainFrame::writeMsgCenterCfg(const std::string& cfgStr)
{
    bool res = true;

    std::ofstream outfile;
    // open file if it not exist and create it    
#ifdef _WIN32
    wchar_t appDataPath[MAX_PATH] = { 0 };
    auto hr = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataPath);
    char* path = new char[MAX_PATH];
    size_t pathLength;
    wcstombs_s(&pathLength, path, MAX_PATH, appDataPath, MAX_PATH);
    std::string filePath = path;
    std::string appName = "\\" + std::string(SLIC3R_APP_KEY " Profile");
    filePath = filePath + appName + "\\msgCenterCfgVersionInfo.json";

    outfile.open(filePath);
#elif __APPLE__
    outfile.open("/tmp/msgCenterCfgVersionInfo.json");
#else
    outfile.open("/tmp/msgCenterCfgVersionInfo.json");
#endif
    // check open status if it success
    if (!outfile.is_open())
    {
        std::cerr << "can't open the file." << std::endl;
    }
    else
    {
        // write something to file
        outfile << cfgStr;
    }
    // close the file
    outfile.close();
    return res;
}

bool MainFrame::loadMsgCenterCfg()
{
    bool res = true;
    std::ifstream ifs;    
#ifdef _WIN32
    wxStandardPaths standarPaths = wxStandardPaths::Get();
    wxString filePath = standarPaths.GetUserDataDir();
    //std::string appName = std::string("\\")+ SLIC3R_APP_KEY + std::string(" Profile");
    std::string appName = std::string("\\")+ SLIC3R_APP_KEY + std::string(" Profile");
    //filePath = filePath + appName+"\\msgCenterCfgVersionInfo.json";
    filePath = filePath +"\\msgCenterCfgVersionInfo.json";

    std::string cfgFilePath = filePath.ToStdString();
    ifs.open(cfgFilePath, std::ios::in);    
#elif __APPLE__
    ifs.open("/tmp/msgCenterCfgVersionInfo.json", std::ios::in);
#else
    ifs.open("/tmp/msgCenterCfgVersionInfo.json", std::ios::in);
#endif

    if (!ifs.is_open())
    {
        ANKER_LOG_WARNING << "read cfg fail." ;
        return false;
    }

    std::string cfgBuf = "";
    while (std::getline(ifs, cfgBuf))
    {        
    }
    ifs.close();

    json_error_t error;
    json_t* root = json_loads(cfgBuf.c_str(), 0, &error);

    if (!root) {
        ANKER_LOG_ERROR << "load loadMsgCenterCfg json fail: " + std::string(error.text);
        return false;
    }

    std::map<std::string, MsgCenterConfig> msgCenterConfigMap;
    if (auto paramsArray = json_object_get(root, "data")) {
        for (int i = 0; i < json_array_size(paramsArray); ++i)
        {
            MsgCenterConfig configItem;
            json_t* child = json_array_get(paramsArray, i);

            if (auto idObj = json_object_get(child, "id"))
                configItem.id = json_integer_value(idObj);

            if (auto errCodeObj = json_object_get(child, "error_code"))
                configItem.error_code = json_string_value(errCodeObj);

            if (auto errLevelObj = json_object_get(child, "alert_level"))
                configItem.error_level = json_string_value(errLevelObj);

            if (auto valueObj = json_object_get(child, "code_source"))
                configItem.code_source = json_integer_value(valueObj);

            if (auto articleArray = json_object_get(child, "article_list"))
            {
                for (int i = 0; i < json_array_size(articleArray); ++i)
                {
                    MsgCenterConfig::ArticleInfo articleInfoItem;
                    json_t* articleChild = json_array_get(articleArray, i);

                    if (auto languageObj = json_object_get(articleChild, "language"))
                        articleInfoItem.language = json_string_value(languageObj);

                    if (auto articleUrlObj = json_object_get(articleChild, "article_url"))
                        articleInfoItem.article_url = json_string_value(articleUrlObj);

                    if (auto articleTitleObj = json_object_get(articleChild, "article_title"))
                        articleInfoItem.article_title = json_string_value(articleTitleObj);

                    configItem.article_info.push_back(articleInfoItem);
                }

            }
            m_MsgCenterCfg->insert(std::make_pair(configItem.error_code, configItem));
        }
    }
    else
        res = false;
    return res;
}

bool MainFrame::writeMsgCenterMultiLanguageCfg(const std::string& cfgStr)
{
    bool res = true;
    std::ofstream outfile;
    // open file if it not exist and create it
#ifdef _WIN32
    wchar_t appDataPath[MAX_PATH] = { 0 };
    auto hr = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataPath);
    char* path = new char[MAX_PATH];
    size_t pathLength;
    wcstombs_s(&pathLength, path, MAX_PATH, appDataPath, MAX_PATH);
    std::string filePath = path;
    std::string appName = "\\" + std::string(SLIC3R_APP_KEY " Profile");
    filePath = filePath + appName + "\\msgCenterMultiLanguageCfg.json";
    outfile.open(filePath);
#elif __APPLE__
    outfile.open("/tmp/msgCenterMultiLanguageCfg.json");
#else
    outfile.open("/tmp/msgCenterMultiLanguageCfg.json");
#endif

    // check open status if it success
    if (!outfile.is_open())
    {
        std::cerr << "can't open the file." << std::endl;
    }
    else
    {
        // write something to file
        outfile << cfgStr;
    }
    // close the file
    outfile.close();
    return res;
}
bool MainFrame::loadMsgCenterMultiLanguageCfg()
{
    bool res = true;
    std::ifstream ifs;
#ifdef _WIN32
    wxStandardPaths standarPaths = wxStandardPaths::Get();
    wxString fileDir = standarPaths.GetUserDataDir();

    std::string appName = std::string("\\") + SLIC3R_APP_KEY + std::string(" Profile");    
    //fileDir = fileDir + appName + "\\msgCenterMultiLanguageCfg.json";
    fileDir = fileDir + "\\msgCenterMultiLanguageCfg.json";

    std::string cfgFilePath = fileDir.ToStdString();
    ifs.open(cfgFilePath, std::ios::in);    
#elif __APPLE__
    ifs.open("/tmp/msgCenterMultiLanguageCfg.json");
#else
    ifs.open("/tmp/msgCenterMultiLanguageCfg.json");
#endif
    if (!ifs.is_open())
    {
        ANKER_LOG_WARNING << "read MultiLanguageCfg fail.";
        return false;
    }

    std::string cfgBuf = "";
    while (std::getline(ifs, cfgBuf))
    {
    }
    ifs.close();

    json_error_t error;
    json_t* root = json_loads(cfgBuf.c_str(), 0, &error);

    if (!root) {
        ANKER_LOG_ERROR << "load MultiLanguageCfg json fail: " + std::string(error.text);
        return false;
    }

    auto jsonDataObj = json_object_get(root, "data");
    if (!jsonDataObj)
    {
        ANKER_LOG_ERROR << "request GetMsgCenterCfgVersionInfo no data ";
        return false;
    }

    if (auto paramsArray = json_object_get(jsonDataObj, "text_2_data"))
    {
        for (int i = 0; i < json_array_size(paramsArray); ++i)
        {
            json_t* child = json_array_get(paramsArray, i);
            MsgErrCodeInfo Item;
            if (auto languageObj = json_object_get(child, "language"))
                Item.language = json_string_value(languageObj);

            if (auto versionObj = json_object_get(child, "version"))
                Item.version = json_string_value(versionObj);

            if (auto releaseVersionObj = json_object_get(child, "release_version"))
                Item.release_version = json_string_value(releaseVersionObj);

            if (auto text2DataObj = json_object_get(child, "text_2"))
            {
                const char* key;
                json_t* value;
                json_object_foreach(text2DataObj, key, value)
                {
                    if (json_is_string(value))
                    {
                        Item.errorCodeUrlMap[key] = json_string_value(value);
                    }
                    else
                    {
                        ANKER_LOG_ERROR << "Error: value for key " << key << " is not a string";
                    }
                }
            }
            m_MsgCenterErrCodeInfo->push_back(Item);
        }
    }
    else
        return false;

    return res;
}


void MainFrame::ShowErrDialogByCenter()
{
    wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
    wxSize mfSize = wxGetApp().mainframe->GetClientSize();
    wxSize dialogSize = m_pMsgCentrePopWindow->GetBestSize();
    wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2, mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);
    m_pMsgCentrePopWindow->Move(center);
    m_pMsgCentrePopWindow->Raise();
    m_pMsgCentrePopWindow->Show();
}
void MainFrame::showErrMsgDialog(const std::string& errorCode, const std::string& errorLevel, const std::string& sn, const int& cmdType)
{
    ANKER_LOG_INFO <<"msg code:"<< errorCode<<"msg level:"<< errorLevel;
    if (cmdType == HIDE_ERR_DIALOG_CMD)
    {        
        if (sn != m_pMsgCentrePopWindow->getDialogSn()|| errorCode!= m_pMsgCentrePopWindow->getDialogErrCode())
            return;
        m_pMsgCentrePopWindow->clearData();
        m_pMsgCentrePopWindow->Hide();
        return;
    }

    if (!m_MsgCenterCfg)
    {
        m_MsgCenterCfg = new std::map<std::string, MsgCenterConfig>();
        if (!loadMsgCenterCfg())
        {
            //load local cfg msgCenterCfgVersionInfo
            ANKER_LOG_ERROR << "no any msg center config fail ";
            return;
        }                          
    }

    //std::string currentLanguage = GetTranslateLanguage();
    std::string currentLanguage = "";
    int type = Slic3r::GUI::wxGetApp().getCurrentLanguageType();
    if (type == wxLanguage::wxLANGUAGE_JAPANESE)
    {
        currentLanguage = "ja";
    }
    else
    {
        currentLanguage = "en";//wxLanguage::wxLANGUAGE_ENGLISH
    }
    auto desCfg = m_MsgCenterCfg->find(errorCode);
    std::string realErrorCode = "fdm_news_center_" + errorCode +"_desc";
    std::string content = "";
    if (desCfg == m_MsgCenterCfg->end())
    {
        ANKER_LOG_ERROR << "no any msg center config for this errorCode: " << errorCode;
        return;
    }
    std::string dialogContent = "";
    if (!m_MsgCenterErrCodeInfo)
    {
        m_MsgCenterErrCodeInfo = new std::vector<MsgErrCodeInfo>();
        if (!loadMsgCenterMultiLanguageCfg())
        {
            //load local cfg msgCenterMultiLanguageCfg
            ANKER_LOG_ERROR << "no any msg center MultiLanguageCfg fail ";
            return;
        }
    }
    
    for (auto it = m_MsgCenterErrCodeInfo->begin(); it != m_MsgCenterErrCodeInfo->end(); ++it) {
        if ((*it).language == currentLanguage)
        {
            auto ErrCodeUrlMap = (*it).errorCodeUrlMap;
            auto resItem = ErrCodeUrlMap.find(realErrorCode);
            if (resItem != ErrCodeUrlMap.end())
                dialogContent = resItem->second;
        }
    }    

    auto articleList = desCfg->second.article_info;
    std::string cfgArticleUrl = "";
    std::string cfgArticleTitle = "";
    std::string cfgErrorLevel = desCfg->second.error_level;
    std::string cfgErrorCode = desCfg->second.error_code;

    //fdm_news_center_0xFE01030001_desc
    for (auto item : articleList)
    {
        if (item.language == currentLanguage)
        {
            cfgArticleUrl = item.article_url;
            //cfgArticleTitle = item.article_title;//server return ""
            break;
        }
    }   

    if (cmdType == SHOW_ERR_DIALOG_CMD)
    {   
        auto ankerNet = AnkerNetInst();
        if (!ankerNet) {
            return;
        }
        DeviceObjectBasePtr devceiObj = ankerNet->getDeviceObjectFromSn(sn);
        if (!devceiObj)
            return;

        cfgArticleTitle = devceiObj->GetStationName();
        wxString titleContent = wxString::FromUTF8(cfgArticleTitle.c_str());        
        wxString utfDialogContent = wxString::FromUTF8(dialogContent.c_str());
        if(cfgErrorLevel == LEVEL_S || cfgErrorLevel == LEVEL_P0)
           utfDialogContent = "[" + cfgErrorCode + "] " +  wxString::FromUTF8(dialogContent.c_str());
        m_pMsgCentrePopWindow->setValue(utfDialogContent, cfgArticleUrl,cfgErrorCode,cfgErrorLevel, sn, titleContent);
        if (m_pMsgCentrePopWindow->IsActive())
        {
            ankerNet->GetMsgCenterStatus();
        }
                                               
        if (cfgErrorLevel == LEVEL_S)
            ShowErrDialogByCenter();
        else
        {            
            if (m_currentTabMode == TAB_DEVICE)
                ShowErrDialogByCenter();
            else
                m_hasErrDialog = true;            
        }
    }
}

void MainFrame::InitAnkerDevice()
{    
    m_pDeviceWidget = new AnkerDevice(m_printTabPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    if (AnkerNetInst()) {
        DatamangerUi::GetInstance().SetMainWindow(this);
        m_pDeviceWidget->Init();
    }
    else {
        ShowUnLoginDevice();
    }
    Bind(wxCUSTOMEVT_DEVICE_LIST_UPDATE, [this](wxCommandEvent& event) {
        m_pDeviceWidget->loadDeviceList(true);
        });
    Bind(wxCUSTOMEVT_UPDATE_MACHINE, [this](wxCommandEvent& event) {
        wxVariant* pData = (wxVariant*)(event.GetClientData());
        if (pData)
        {
            wxVariantList list = pData->GetList();
            std::string snID = list[0]->GetString().ToStdString();
            std::string snIDEx = list[0]->GetString().ToStdString();
            int type = AKNMT_CMD_EVENT_NONE;
            if (list.size() >= 2) {
                list[1]->GetString().ToInt(&type);
            }
            m_pDeviceWidget->updateAboutMqttStatus(snID, (AnkerNet::aknmt_command_type_e)type);
            if (type == AnkerNet::AKNMT_CMD_Z_AXIS_RECOUP) {
                m_pDeviceWidget->updateAboutZoffsetStatus(snID);
            }

            auto ankerNet = AnkerNetInst();
            if (!ankerNet) {
                return;
            }
            DeviceObjectBasePtr devceiObj = ankerNet->getDeviceObjectFromSn(snID);
            if (!devceiObj)
                return;

            if (type == SHOW_ERR_DIALOG_CMD || type == HIDE_ERR_DIALOG_CMD)
            {
                //show error msg dialog
                std::string errorCode = "";
                std::string errorLevel = "";
                devceiObj->GetMsgCenterInfo(errorCode, errorLevel);    
                showErrMsgDialog(errorCode, errorLevel, snID, type);                
            }

        }
        });

    Bind(wxCUSTOMEVT_SHOW_MSG_DIALOG, [this](wxCommandEvent& event) {

        wxVariant* pData = (wxVariant*)event.GetClientData();
        if (pData)
        {
            wxVariantList list = pData->GetList();
            int i = 0;
            int haveCancel = list[i++]->GetInteger();
            int level = list[i++]->GetInteger();
            int clear = list[i++]->GetInteger();
            int type = list[i++]->GetInteger();
            auto sn = list[i++]->GetString().ToStdString();
            auto msgBoxTitle = list[i++]->GetString().ToStdString(wxConvUTF8);
            auto msgBoxContent = list[i++]->GetString().ToStdString(wxConvUTF8);
            auto btn1Text = list[i++]->GetString().ToStdString(wxConvUTF8);
            auto btn2Text = list[i++]->GetString().ToStdString(wxConvUTF8);
            auto imagePath = list[i++]->GetString().ToStdString(wxConvUTF8);
            auto filamentName = list[i++]->GetString().ToStdString(wxConvUTF8);
            
            NetworkMsg msg;
            msg.sn = sn;
            msg.title = msgBoxTitle;
            msg.context = msgBoxContent;
            msg.clear = clear == 0 ? false : true;
            msg.type = (GeneralException2Gui)type;
            msg.haveCancel = haveCancel == 1 ? true : false;
            msg.level = (NetworkMsgLevel)level;
            msg.btn1Text = btn1Text;
            msg.btn2Text = btn2Text;
            msg.imagePath = imagePath;
            msg.filamentName = filamentName;

            m_pDeviceWidget->showMsgLevelDialog(msg);
        }
        });

    Bind(wxCUSTOMEVT_TRANSFER_PROGRESS, [this](wxCommandEvent& event) {
        wxVariant* pData = (wxVariant*)event.GetClientData();
        if (pData) {
            wxVariantList list = pData->GetList();
            if (list.size() < 3) {
                return;
            }
            std::string snID = list[0]->GetString().ToStdString();
            int progress = list[1]->GetInteger();
            FileTransferResult result = (FileTransferResult)list[2]->GetInteger();
            m_pDeviceWidget->updateFileTransferStatus(snID, progress, result);
        }
        });
    Bind(wxCUSTOMEVT_GET_MSG_CENTER_CFG, [this](wxCommandEvent& event) {        

        std::map<std::string, MsgCenterConfig>* pData = (std::map<std::string, MsgCenterConfig>*)(event.GetClientData());
        if (pData)
        {
            int counts = 0;                
            counts = pData->size();
            if (m_MsgCenterCfg)
            {
                delete m_MsgCenterCfg;
                m_MsgCenterCfg = nullptr;
            }
            ANKER_LOG_INFO << "msg center config counts is: "<< counts;
            m_MsgCenterCfg = pData;         

            auto item = pData->begin();
            while (item != pData->end())
            {
                if (item->first == "originMsg")
                {
                    writeMsgCenterCfg(item->second.originMsg);
                    break;
                }
                ++item;
            }
        }
        });
    Bind(wxCUSTOMEVT_GET_MSG_CENTER_RECORDS, [this](wxCommandEvent& event) {

        std::vector<MsgCenterItem>* pData = (std::vector<MsgCenterItem>*)(event.GetClientData());
        if (pData)
        {
            int counts = 0;
            counts = pData->size();
            
            ANKER_LOG_INFO << "msg center config counts is: " << counts;
            
            //update content
            if (pData->size() <= 0)
                m_MsgCentreDialog->updateCurrentPage();

            updateMsgCenterItemContent(pData);
            m_MsgCentreDialog->updateMsg(pData);
        }
        });

    Bind(wxCUSTOMEVT_GET_MSG_CENTER_ERR_CODE_INFO, [this](wxCommandEvent& event) {

        std::vector<MsgErrCodeInfo>* pData = (std::vector<MsgErrCodeInfo>*)(event.GetClientData());
        if (pData)
        {
            int counts = 0;
            counts = pData->size();
            if (m_MsgCenterErrCodeInfo)
            {
                delete m_MsgCenterErrCodeInfo;
                m_MsgCenterErrCodeInfo = nullptr;
            }
            ANKER_LOG_INFO << "msg center config counts is: " << counts;
            m_MsgCenterErrCodeInfo = pData;


            auto item = pData->begin();
            while (item != pData->end())
            {
                if (!item->originMsg.empty())
                {
                    
                    writeMsgCenterMultiLanguageCfg(item->originMsg);
                    break;
                }
                ++item;
            }
        }
        });

    Bind(wxCUSTOMEVT_GET_MSG_CENTER_STATUS, [this](wxCommandEvent& event) {
        wxVariant* pData = (wxVariant*)(event.GetClientData());
        if (pData)
        {
            wxVariantList list = pData->GetList();
            int officicalNews = list[0]->GetInteger();
            int printerNews = list[1]->GetInteger();
            
            m_pFunctionPanel->setMsgItemRedPointStatus(officicalNews, printerNews);
        }
        });

    if (m_pDeviceWidget) {
        m_pDeviceWidget->Bind(wxCUSTOMEVT_LOGIN_CLCIKED, [this](wxCommandEvent& event) {
            ShowAnkerWebView("device login button clicked");
        });
    }

    if (m_pDeviceWidget) {
        m_printTabPanel->AddPage(m_pDeviceWidget, _L("Print"));
    }

    // Device Details -- manual move / adjustments / raw G-code. Page index must
    // match tabPanleType::type_details and the button order in AnkerFunctionPanel.
    m_pDeviceDetails = new AnkerDeviceDetails(m_printTabPanel);
    m_printTabPanel->AddPage(m_pDeviceDetails, _L("Device Details"));

    Bind(wxCUSTOMEVT_SWITCH_TO_PRINT_PAGE, [this](wxCommandEvent& event) {
        wxStringClientData* pData = static_cast<wxStringClientData*>(event.GetClientObject());
        if (pData) {
            // type_devcie, not pageCount-1: that meant "the Device page" only
            // while Device happened to be the last page.
            m_printTabPanel->ChangeSelection(type_devcie);
            std::string sn = pData->GetData().ToStdString();
            m_pDeviceWidget->switchDevicePage(sn);
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_ON_TAB_CHANGE);
            evt.SetId(type_devcie);
            wxPostEvent(m_pFunctionPanel, evt);
            //change  Tab  to device
            if (wxGetApp().mainframe != nullptr)
            {
                wxGetApp().mainframe->setTabMode(TAB_DEVICE);
            }
        }
        });

    // Default the startup view to the Device tab (rather than Slice/Plater).
    // Deferred so it runs once the tab panel and its page-changed handler are
    // fully wired; the Device page is the last page in m_printTabPanel.
    CallAfter([this]() {
        if (!m_printTabPanel)
            return;
        if (m_printTabPanel->GetPageCount() <= type_devcie)
            return;
        m_printTabPanel->SetSelection(type_devcie);
        setTabMode(TAB_DEVICE);
        wxCommandEvent evt(wxCUSTOMEVT_ON_TAB_CHANGE);
        evt.SetId(type_devcie);
        if (m_pFunctionPanel)
            wxPostEvent(m_pFunctionPanel, evt);
    });
}

// Was create_preset_tabs(). Renamed because the old name cost a regression: the
// five preset tabs it once built are gone, but this is where the Device and Device
// Details pages are created and the account/OTA/HTTP-error handlers are bound, so
// dropping the call alongside the plater left a window with tabs and no content.
void MainFrame::initDeviceTabs()
{
    InitAnkerDevice();

    Bind(wxCUSTOMEVT_GET_COMMENT_FLAGS, [this](wxCommandEvent& event) {
        wxVariant* pData = (wxVariant*)event.GetClientData();
        if (pData) {
            wxVariantList list = pData->GetList();

            m_showCommentWebView = list[0]->GetBool();
            if (m_showCommentWebView)
            {
                g_sliceCommentData.reviewNameID = list[1]->GetString().ToStdString();
                g_sliceCommentData.reviewName = list[2]->GetString().ToStdString(wxConvUTF8);
                g_sliceCommentData.appVersion = list[3]->GetString().ToStdString();
                g_sliceCommentData.country = list[4]->GetString().ToStdString();
                g_sliceCommentData.sliceCount = list[5]->GetString().ToStdString();
            
            }
            ANKER_LOG_INFO << "get conment flags success";
        }
    });

    Bind(wxCUSTOMEVT_ACCOUNT_LOGOUT, [this](wxCommandEvent& event) {
        LogOut();
    });

    Bind(wxCUSTOMEVT_ACCOUNT_EXTRUSION, [this](wxCommandEvent& event) {
        
        static bool accountShowed = false;
        if (accountShowed) {
            return;
        }

        CloseVideoStream(VIDEO_CLOSE_BY_LOGOUT);

        accountShowed = true;
        wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
        wxSize mfSize = wxGetApp().mainframe->GetClientSize();
        wxSize dialogSize = AnkerSize(400, 250);
        wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2, mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);
        AnkerDialog dialog(this, wxID_ANY, _AnkerL("common_popup_titlenotice"),
            _AnkerL("common_popup_content_accountsqueezed"),
            center, dialogSize);
        dialog.CenterOnParent();
        auto res = dialog.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog);
        accountShowed = false;
        ShowUnLoginMenu();
        onLogOut();
        if(wxID_CLOSE != res)
        {
            m_extrusionTimer->Start(100, wxTIMER_ONE_SHOT);
        } 
        });

    Bind(wxCUSTOMEVT_HTTP_CONNECT_ERROR,&MainFrame::OnHttpConnectError,this);

    Bind(wxCUSTOMEVT_OTA_UPDATE, [this](wxCommandEvent& event) {
        auto ankerNet = AnkerNetInst();
        if (!ankerNet) {
            return;
        }

        OtaInfo* info = (OtaInfo*)(event.GetClientData());

        if (info)
        {
            wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
            wxSize mfSize = wxGetApp().mainframe->GetClientSize();

            if (info->noUpdate) {
                if (ankerNet->GetOtaCheckType() == OtaCheckType_Manual) {
                    wxSize dialogSize = AnkerSize(400, 180);
                    wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2,
                        mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);

                    AnkerDialog dialog(nullptr, wxID_ANY, _L("common_menu_settings_ota"),
                        _L("common_popup_ota_noticenew"), center, dialogSize);

                    int result = dialog.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog);
                    ANKER_LOG_INFO << "result: " << result;
                }
                return;
            }


            {
                int otaId = m_menubar->FindMenuItem(_L("common_menu_title_settings"), _L("common_menu_settings_ota"));
                wxMenu* tmpSettingsMenu = nullptr;
                wxMenuItem* otaItem = m_menubar->FindItem(otaId, &tmpSettingsMenu);
                if (otaItem) {            
                    wxImage image(AnkerBase::AnkerResourceIconPath + "ota_reddot.png", wxBITMAP_TYPE_PNG);
                    if (image.IsOk()) {
                        wxBitmap bitmap(image);
                        otaItem->SetBitmap(bitmap);
                    }
                }

                wxSize dialogSize = AnkerSize(500, 330);
                wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2,
                    mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);

                AnkerOtaNotesDialog dialog(nullptr, wxID_ANY, _L("common_menu_settings_ota"),
                    wxString::FromUTF8(info->version_name.c_str()), wxString::FromUTF8(info->release_note.c_str()), center, dialogSize);
                int result = 0;
                if (info->is_forced) {
                    result = dialog.ShowAnkerModal(OtaType_Forced);
                }
                else {
                    result = dialog.ShowAnkerModal(OtaType_Normal);
                }
                ANKER_LOG_INFO << "ota click result: " << result << " is_forced: " << info->is_forced;
                if (wxID_OK == result) {
                    wxString url = wxString::FromUTF8(info->download_path.c_str());
                    wxURI uri(url);
                    url = uri.BuildURI();
                    bool success = wxLaunchDefaultBrowser(url, wxBROWSER_NEW_WINDOW);
                    if (!success) {
                        ANKER_LOG_WARNING << "launch browser failed, url: " << url.c_str();
                    }
                }
            }
        }

        });

}


bool MainFrame::is_active_and_shown_tab(Tab* tab)
{
    int page_id = m_tabpanel->FindPage(tab);

    if (m_tabpanel->GetSelection() != page_id)
        return false;

    if (m_layout == ESettingsLayout::Dlg)
        return m_settings_dialog.IsShown();

    if (m_layout == ESettingsLayout::New)
        return m_main_sizer->IsShown(m_tabpanel);
    
    return true;
}

bool MainFrame::isActiveAndShownAnkerTab(AnkerTab* tab)
{
    if (!m_ankerCfgDlg)
        return false;

    int page_id = m_ankerCfgDlg->m_rightPanel->FindPage(tab);

    if (m_ankerCfgDlg->m_rightPanel->GetSelection() != page_id)
        return false;

    if (m_layout == ESettingsLayout::Dlg) {
        // add by allen for ankerCfgDlg
        // return m_settings_dialog.IsShown();
          return m_ankerCfgDlg->IsShown();
    }
        
    if (m_layout == ESettingsLayout::New)
        return m_main_sizer->IsShown(m_tabpanel);

    return true;
}




















void MainFrame::on_dpi_changed(const wxRect& suggested_rect)
{
    wxGetApp().update_fonts(this);
    this->SetFont(this->normal_font());

#ifdef _MSW_DARK_MODE
    //// update common mode sizer
    //if (!wxGetApp().tabs_as_menu())
    //    dynamic_cast<Notebook*>(m_tabpanel)->Rescale();
#endif


   //// update AnkerConfigDialog
   // if (m_ankerCfgDlg)
   //     m_ankerCfgDlg->msw_rescale();


    // Workarounds for correct Window rendering after rescale

    /* Even if Window is maximized during moving,
     * first of all we should imitate Window resizing. So:
     * 1. cancel maximization, if it was set
     * 2. imitate resizing
     * 3. set maximization, if it was set
     */
    const bool is_maximized = this->IsMaximized();
    if (is_maximized)
        this->Maximize(false);

    /* To correct window rendering (especially redraw of a status bar)
     * we should imitate window resizing.
     */
    const wxSize& sz = this->GetSize();
    this->SetSize(sz.x + 1, sz.y + 1);
    this->SetSize(sz);

    this->Maximize(is_maximized);
}

void MainFrame::handleErrMsgDialogRes(wxVariant* pData)
{
    if (!pData)
    {
        ANKER_LOG_WARNING << "handleErrMsgDialogRes no data to handle ";
        return;
    }

    std::string snID = "";
    std::string errorCode = "";
    std::string errorLevel = "";

    wxVariantList list = pData->GetList();
    snID = list[0]->GetString().ToStdString();
    errorCode = list[1]->GetString().ToStdString();
    errorLevel = list[2]->GetString().ToStdString();
    

    auto ankerNet = AnkerNetInst();
    if (!ankerNet) {
        return;
    }
    DeviceObjectBasePtr devceiObj = ankerNet->getDeviceObjectFromSn(snID);
    if (!devceiObj)
        return;
    devceiObj->SendErrWinResToMachine(errorCode, errorLevel);
}
void MainFrame::initAnkerUi()
{
    wxSize msgWebViewSize = AnkerSize(900, 700);
    wxPoint msgWebViewPos = wxPoint((GetSize().x - msgWebViewSize.x) / 2, (GetSize().y - msgWebViewSize.y) / 2);

    if (!m_MsgCentreDialog)
        m_MsgCentreDialog = new AnkerMsgCentreDialog(this);
    m_MsgCentreDialog->SetSize(AnkerSize(720, 700));  
    m_MsgCentreDialog->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& evt) { 
        m_isMsgCenterIsShow = false;            
        m_pFunctionPanel->setMsgEntrenceRedPointStatus(false);
        evt.Skip(); });
    m_pMsgCentrePopWindow = new AnkerCustomDialog(this);
    m_pMsgCentrePopWindow->SetMaxSize(AnkerSize(400, 240));
    m_pMsgCentrePopWindow->SetMinSize(AnkerSize(400, 240));
    m_pMsgCentrePopWindow->SetSize(AnkerSize(400, 240));
    m_pMsgCentrePopWindow->Bind(wxCUSTOMEVT_ANKER_CUSTOM_CLOSE, [this](wxCommandEvent& event) {

        wxVariant* pData = (wxVariant*)(event.GetClientData());
        handleErrMsgDialogRes(pData);
        m_hasErrDialog = false;
        m_pMsgCentrePopWindow->Hide();
        });
    m_pMsgCentrePopWindow->Bind(wxCUSTOMEVT_ANKER_CUSTOM_OK, [this](wxCommandEvent& event) {
       
        wxVariant* pData = (wxVariant*)(event.GetClientData());        
        handleErrMsgDialogRes(pData);
        m_hasErrDialog = false;
        m_pMsgCentrePopWindow->Hide();
        });
    m_pMsgCentrePopWindow->Bind(wxCUSTOMEVT_ANKER_CUSTOM_CANCEL, [this](wxCommandEvent& event) {

        wxVariant* pData = (wxVariant*)(event.GetClientData());
        handleErrMsgDialogRes(pData);
        m_hasErrDialog = false;
        m_pMsgCentrePopWindow->Hide();
        });
    m_pMsgCentrePopWindow->Bind(wxCUSTOMEVT_ANKER_CUSTOM_OTHER, [this](wxCommandEvent& event) {

        wxVariant* pData = (wxVariant*)(event.GetClientData());
        handleErrMsgDialogRes(pData);
        m_hasErrDialog = false;
        m_pMsgCentrePopWindow->Hide();
        });
    m_pMsgCentrePopWindow->Hide();
}

void MainFrame::on_sys_color_changed()
{
    wxBusyCursor wait;

    // update label colors in respect to the system mode
    wxGetApp().init_ui_colours();
    // but if there are some ui colors in appconfig, they have to be applied
    wxGetApp().update_ui_colours_from_appconfig();
#ifdef __WXMSW__
    wxGetApp().UpdateDarkUI(m_tabpanel);
 //   m_statusbar->update_dark_ui();
 // 
 // 
 // //TODO: need  do this ?
//#ifdef _MSW_DARK_MODE
//    // update common mode sizer
//    if (!wxGetApp().tabs_as_menu())
//        dynamic_cast<Notebook*>(m_tabpanel)->OnColorsChanged();
//#endif
#endif

    // add by allen for ankerCfgDlg
    for (auto tab : wxGetApp().ankerTabsList)
        tab->sys_color_changed();


    this->Refresh();
}


#ifdef _MSC_VER
    // \xA0 is a non-breaking space. It is entered here to spoil the automatic accelerators,
    // as the simple numeric accelerators spoil all numeric data entry.
//static const wxString sep = "\t\xA0";
static const wxString sep = "\t";
static const wxString sep_space = "\xA0";
#else
static const wxString sep = " - ";
static const wxString sep_space = "";
#endif

void DataSharedReport(bool isShareBuryPoint)
{
    std::map<std::string, std::string> buryMap;
    if (isShareBuryPoint) {
        buryMap.insert(std::make_pair(c_current_bury_shared, "1"));
    }
    else {
        buryMap.insert(std::make_pair(c_current_bury_shared, "0"));
    }
    ANKER_LOG_INFO << "Report bury event is " << e_report_bury_shared;
    reportBuryEvent(e_report_bury_shared, buryMap);
}

void SetBuryPointSwitch()
{
    bool burypointSwitch = wxGetApp().app_config->get_bool("burypoint_switch");
    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj) {
        if (!burypointSwitch) {
            obj->PostSetBuryPointSwitch(true);
        } else {
            obj->PostSetBuryPointSwitch(false);
        }
    }
}


void QueryDataShared(AnkerToggleBtn* dataSharedButton)
{
    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj) {
        std::vector<int> paramTypeList = { static_cast<int>(AnkerNet::eUserParams::DATA_SHARE_SWITCH) };
        auto result = obj->PostQueryDataShared(paramTypeList);
 
        if (result.empty()) {
           
            bool burypointSwitch = wxGetApp().app_config->get_bool("burypoint_switch");
            ANKER_LOG_INFO << "The query result is empty, "<< "burypoint switch is "<< burypointSwitch;
            if (!burypointSwitch) {
                DataSharedReport(false);
                if (dataSharedButton != nullptr) {
                    dataSharedButton->SetState(false);
                }
            } else {
                DataSharedReport(true);
                if (dataSharedButton != nullptr) {
                    dataSharedButton->SetState(true);
                }
            }

            return;
        }

        for (int i = 0; i < result.size(); ++i) {
            auto [paramType, paramValue] = result[i];
            ANKER_LOG_INFO << "Query burypoint switch is " << paramValue;
            if (paramValue == "0") {  
                wxGetApp().app_config->set("burypoint_switch", "0");
                DataSharedReport(false);
            }else {
                wxGetApp().app_config->set("burypoint_switch", "1");
                DataSharedReport(true);
            }
            SetBuryPointSwitch();
            if (dataSharedButton == nullptr) {
                return;
            }
            if (paramType == static_cast<int>(AnkerNet::eUserParams::DATA_SHARE_SWITCH)) {
                if (paramValue == "0") {
                    dataSharedButton->SetState(false);
                    return;
                }
                else {
                    dataSharedButton->SetState(true);
                    return;
                }
            }
        }
    }
    if (dataSharedButton != nullptr) {
        dataSharedButton->SetState(true);
    }
}

static std::tuple<int, std::string> PreHandleUpdateDataShared(AnkerToggleBtn* shareBuryPointButton)
{
    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj) {
        std::vector<std::tuple<std::string, int>> memberVec = obj->PostGetMemberType();
        for (int i = 0; i < memberVec.size(); ++i) {
            auto [stationSn, memberType] = memberVec[i];
            // memberType: 1 is sharer, 2 is owner
            if (memberType == 1) {
                continue;
            }

            std::vector<std::pair<int, std::string>> paramTypeList;
            if (shareBuryPointButton == nullptr) { 
                return std::make_tuple(-1, "-1"); 
            }
            bool btnState = shareBuryPointButton->GetState();
            if (btnState) {
                paramTypeList = { {static_cast<int>(AnkerNet::eUserParams::DATA_SHARE_SWITCH),"1"} };
            } else {
                paramTypeList = { {static_cast<int>(AnkerNet::eUserParams::DATA_SHARE_SWITCH),"0"} };
            }
            auto [code, _1] = obj->PostUpdateDataShared(paramTypeList);
            return std::make_tuple(code, stationSn);
        }
    }
    return std::make_tuple(-1, "-1");
}

static void OnShareBuryPointButtonClick(AnkerToggleBtn* shareBuryPointButton, int code, const std::string& stationSn)
{
    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj) {
        if (code == 0) {
            bool shareBuryPointButtonStatus = shareBuryPointButton->GetState();
            ANKER_LOG_INFO << "Update burypoint switch is " << shareBuryPointButtonStatus;
            shareBuryPointButton->SetState(shareBuryPointButtonStatus);
            DataSharedReport(shareBuryPointButtonStatus);
            DeviceObjectBasePtr deviceObjectPtr = obj->getDeviceObjectFromSn(stationSn);
            if (deviceObjectPtr) {
                bool state = shareBuryPointButtonStatus;
                deviceObjectPtr->SendSwitchInfoToDevice("shareAnalytics", state);
            }

            if (shareBuryPointButtonStatus) {
                wxGetApp().app_config->set("burypoint_switch", "1");
            }
            else {
                wxGetApp().app_config->set("burypoint_switch", "0");
            }
            SetBuryPointSwitch();
        }
        else {
            wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
            wxSize mfSize = wxGetApp().mainframe->GetClientSize();
            wxSize dialogSize = AnkerSize(380, 100);
            wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2,
                mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);
            AnkerDialog dialog(nullptr, wxID_ANY, _L("share_analytics_tips"), "", center, dialogSize);
            int result = dialog.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog);
            ANKER_LOG_INFO << "result: " << result;

            bool state = !shareBuryPointButton->GetState();
            if (state) {
                wxGetApp().app_config->set("burypoint_switch", "1");
            }
            else {
                wxGetApp().app_config->set("burypoint_switch", "0");
            }
            ANKER_LOG_INFO << "Update burypoint switch is " << state;
            SetBuryPointSwitch();
            shareBuryPointButton->SetState(state);
            DataSharedReport(state);
        }
    }
#ifdef OldOnShareBuryPointButtonClick
    auto obj = DatamangerUi::GetInstance().getAnkerNetBase();
    if (obj) {
        std::vector<std::tuple<std::string, int>> memberVec = obj->PostGetMemberType();
        for (int i = 0; i < memberVec.size(); ++i) {
            auto [stationSn, memberType] = memberVec[i];
            if (memberType == 1) {
                bool btnState = !shareBuryPointButton->GetState();
                shareBuryPointButton->SetState(btnState);
                continue;
            }

            std::vector<std::pair<int, std::string>> paramTypeList;
            if (shareBuryPointButton->GetState()) {
                paramTypeList = { {static_cast<int>(AnkerNet::eUserParams::DATA_SHARE_SWITCH),"1"} };
            }
            else {
                paramTypeList = { {static_cast<int>(AnkerNet::eUserParams::DATA_SHARE_SWITCH),"0"} };
            }
            auto [code, _1] = obj->PostUpdateDataShared(paramTypeList);
            if (code == 0) {
                bool shareBuryPointButtonStatus = shareBuryPointButton->GetState();
                ANKER_LOG_INFO << "Update burypoint switch is " << shareBuryPointButtonStatus;
                shareBuryPointButton->SetState(shareBuryPointButtonStatus);
                DataSharedReport(shareBuryPointButtonStatus);
                DeviceObjectBasePtr deviceObjectPtr = obj->getDeviceObjectFromSn(stationSn);
                if (deviceObjectPtr) {
                    bool state = shareBuryPointButtonStatus;
                    deviceObjectPtr->SendSwitchInfoToDevice("shareAnalytics", state);
                }

                if (shareBuryPointButtonStatus) {
                    wxGetApp().app_config->set("burypoint_switch", "1");
                }
                else {
                    wxGetApp().app_config->set("burypoint_switch", "0");
                }
            }
            else {
                wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
                wxSize mfSize = wxGetApp().mainframe->GetClientSize();
                wxSize dialogSize = AnkerSize(380, 100);
                wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2,
                    mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);
                AnkerDialog dialog(nullptr, wxID_ANY, _L("share_analytics_tips"), "", center, dialogSize);
                int result = dialog.ShowAnkerModal(AnkerDialogType_DisplayTextOkDialog);
                ANKER_LOG_INFO << "result: " << result;

                bool state = !shareBuryPointButton->GetState();
                if (state) {
                    wxGetApp().app_config->set("burypoint_switch", "1");
                }
                else {
                    wxGetApp().app_config->set("burypoint_switch", "0");
                }
                ANKER_LOG_INFO << "Update burypoint switch is " << state;
                shareBuryPointButton->SetState(state);
                DataSharedReport(state);
            }
        }
    }
#endif // OldOnShareBuryPointButtonClick

}


static wxMenu* generate_help_menu()
{
    wxMenu* helpMenu = new wxMenu();
//    append_menu_item(helpMenu, wxID_ANY, _L("Anker 3D &Drivers"), _L("Open the Anker3D drivers download page in your browser"),
//        [](wxCommandEvent&) { wxGetApp().open_web_page_localized("https://www.prusa3d.com/downloads"); });
//    append_menu_item(helpMenu, wxID_ANY, _L("Software &Releases"), _L("Open the software releases page in your browser"),
//        [](wxCommandEvent&) { wxGetApp().open_browser_with_warning_dialog("https://github.com/prusa3d/PrusaSlicer/releases", nullptr, false); });
////#        my $versioncheck = $self->_append_menu_item($helpMenu, "Check for &Updates...", "Check for new Slic3r versions", sub{
////#            wxTheApp->check_version(1);
////#        });
////#        $versioncheck->Enable(wxTheApp->have_version_check);
//    append_menu_item(helpMenu, wxID_ANY, wxString::Format(_L("%s &Website"), SLIC3R_APP_NAME),
//        wxString::Format(_L("Open the %s website in your browser"), SLIC3R_APP_NAME),
//        [](wxCommandEvent&) { wxGetApp().open_web_page_localized("https://www.prusa3d.com/slicerweb"); });
////        append_menu_item(helpMenu, wxID_ANY, wxString::Format(_L("%s &Manual"), SLIC3R_APP_NAME),
////                                             wxString::Format(_L("Open the %s manual in your browser"), SLIC3R_APP_NAME),
////            [this](wxCommandEvent&) { wxGetApp().open_browser_with_warning_dialog("http://manual.slic3r.org/"); });
//    helpMenu->AppendSeparator();
   // append_menu_item(helpMenu, wxID_ANY, _L("System &Info"), _L("Show system information"),
   //     [](wxCommandEvent&) { wxGetApp().system_info(); });
    // append_menu_item(helpMenu, wxID_ANY, _L("Show &Configuration Folder"), _L("Show user configuration folder (datadir)"),
    //    [](wxCommandEvent&) { Slic3r::GUI::desktop_open_datadir_folder(); });
    //append_menu_item(helpMenu, wxID_ANY, _L("Report an I&ssue"), wxString::Format(_L("Report an issue on %s"), SLIC3R_APP_NAME),
    //    [](wxCommandEvent&) { wxGetApp().open_browser_with_warning_dialog("https://github.com/prusa3d/slic3r/issues/new", nullptr, false); });
    //if (wxGetApp().is_editor())
    //    append_menu_item(helpMenu, wxID_ANY, wxString::Format(_L("&About %s"), SLIC3R_APP_NAME), _L("Show about dialog"),
    //        [](wxCommandEvent&) { Slic3r::GUI::about(); });
    //else
    //    append_menu_item(helpMenu, wxID_ANY, wxString::Format(_L("&About %s"), GCODEVIEWER_APP_NAME), _L("Show about dialog"),
    //        [](wxCommandEvent&) { Slic3r::GUI::about(); });
    //append_menu_item(helpMenu, wxID_ANY, _L("Show Tip of the Day") 
//#if 0//debug
//        + "\tCtrl+Shift+T"
//#endif
//        ,_L("Opens Tip of the day notification in bottom right corner or shows another tip if already opened."),
//        [](wxCommandEvent&) { wxGetApp().plater()->get_notification_manager()->push_hint_notification(false); });
//    helpMenu->AppendSeparator();
   // append_menu_item(helpMenu, wxID_ANY, _L("Keyboard Shortcuts") + sep + "&?", _L("Show the list of the keyboard shortcuts"),
   //     [](wxCommandEvent&) { wxGetApp().keyboard_shortcuts(); });
#if ENABLE_THUMBNAIL_GENERATOR_DEBUG
    helpMenu->AppendSeparator();
    append_menu_item(helpMenu, wxID_ANY, "DEBUG gcode thumbnails", "DEBUG ONLY - read the selected gcode file and generates png for the contained thumbnails",
        [](wxCommandEvent&) { wxGetApp().gcode_thumbnails_debug(); });
#endif // ENABLE_THUMBNAIL_GENERATOR_DEBUG

    
    append_menu_item(helpMenu, wxID_ANY, _L("common_menu_help_privacy"), _L("Show privacy policy"),
        [](wxCommandEvent&) {
            wxString url = wxString(Slic3r::UrlConfig::PrivacyNoticeEn);
            if (MainFrame::currentSoftwareLanguageIsJapanese()) {
                url = wxString(Slic3r::UrlConfig::PrivacyNoticeJa);
            }

            wxURI uri(url);
            url = uri.BuildURI();

            bool success = wxLaunchDefaultBrowser(url);
            if (success) {
            }
            else {
            
            }
            });
    append_menu_item(helpMenu, wxID_ANY, _L("common_menu_help_copyright"), _L("Show copyright information"),
        [](wxCommandEvent&) {
            wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
            wxSize mfSize = wxGetApp().mainframe->GetClientSize();
            wxSize dialogSize = AnkerSize(400, 362);
            wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2, mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);
            wxString title = _AnkerL("common_popup_copyright_title");                            
            AnkerCopyrightDialog dialog(nullptr, wxID_ANY, title, "", center, dialogSize);
            dialog.ShowAnkerModal();
        });

    return helpMenu;
}


void MainFrame::init_menubar_as_editor()
{
#ifdef __APPLE__
    wxMenuBar::SetAutoWindowMenu(false);
#endif

    // File menu. Everything it used to hold -- new/open/save project, import
    // STL/SLA/ZIP, import config, export G-code/STL/AMF -- was slicing. The one
    // file operation this fork has is picking an already-sliced G-code to print,
    // which is the Device tab's "Start Printing" flow.
    wxMenu* fileMenu = new wxMenu;
    {
        append_menu_item(fileMenu, wxID_ANY, _L("Start Printing") + "\tCtrl+P", _L("Choose a G-code file and send it to the printer"),
            [this](wxCommandEvent&) { if (m_pDeviceWidget) m_pDeviceWidget->startPrintFlow(); }, "", nullptr,
            [this]() { return m_pDeviceWidget != nullptr; }, this);
    }

    // Edit menu removed: select all/deselect, delete, delete all, undo, redo,
    // copy/paste and reload-from-disk were all plater operations on the 3D
    // scene. With no scene there is nothing to select, undo or reload.

    // Window menu removed: it was built but never appended to the menubar (the
    // Append call was already commented out), and every item drove the plater --
    // plater/preset tabs, 3D and preview views, and the preset diff dialog.

    // View menu removed: it drove the 3D scene (view angles, object labels).
    wxMenu* viewMenu = nullptr;

    // Settings menu. Deliberately NOT gated on m_plater: it holds language
    // switching and the OTA check, which outlive the slicer.
    wxMenu* settingsMenu = nullptr;
    {
        settingsMenu = new wxMenu();
        wxMenu* languageMenu = new  wxMenu();
        //wxLanguage::wxLANGUAGE_CHINESE_CHINA = 130
       // wxLanguage::wxLANGUAGE_ENGLISH = 175
        //wxLanguage::wxLANGUAGE_JAPANESE = 428
        int type = wxGetApp().getCurrentLanguageType();
        ANKER_LOG_INFO << "Language type: " << type;
        std::string iconPath = "appbar_sure_icon";
        if (wxLanguage::wxLANGUAGE_ENGLISH == type) {
            append_menu_item(languageMenu, wxID_ANY, _L("common_menu_settings_languageen"), _L("Switch language to English"),
                [this](wxCommandEvent&) {  selectLanguage(GUI_App::AnkerLanguageType::AnkerLanguageType_English); },
                iconPath, nullptr, []() { return true; }, this);
        }
        else {
            append_menu_item(languageMenu, wxID_ANY, _L("common_menu_settings_languageen"), _L("Switch language to English"),
                [this](wxCommandEvent&) {
                    selectLanguage(GUI_App::AnkerLanguageType::AnkerLanguageType_English);
                }, "", nullptr, []() { return true; },this);
        }

        if (wxLanguage::wxLANGUAGE_JAPANESE_JAPAN == type ||
            wxLanguage::wxLANGUAGE_JAPANESE == type) {
            append_menu_item(languageMenu, wxID_ANY, _L("common_menu_settings_languagejp"), _L("Switch language to Japanese"),
                [this](wxCommandEvent&) { selectLanguage(GUI_App::AnkerLanguageType::AnkerLanguageType_Japanese); },
                iconPath, nullptr, []() { return true; }, this);
        }
        else {
            append_menu_item(languageMenu, wxID_ANY, _L("common_menu_settings_languagejp"), _L("Switch language to Japanese"),
                [this](wxCommandEvent&) {
                    selectLanguage(GUI_App::AnkerLanguageType::AnkerLanguageType_Japanese);
                }, "", nullptr, []() { return true; },this);
        }

        //add by Samuel, enable/disable muli-language by config 
        GUI::GUI_App* gui = dynamic_cast<GUI::GUI_App*>(GUI::GUI_App::GetInstance());
        if (gui != nullptr && false /* gui->app_config->get_bool("multi-language_enable")*/)
        {
            //comment by samuel, Due to Chinese translation problems, Chinese is blocked first
            if (wxLanguage::wxLANGUAGE_CHINESE_CHINA == type)
            {
                append_menu_item(languageMenu, wxID_ANY, _L("common_menu_settings_languagecn"), _L("Switch language to Chinese"),
                    [this](wxCommandEvent&) { selectLanguage(GUI_App::AnkerLanguageType::AnkerLanguageType_Chinese); },
                    iconPath, nullptr, []() { return true; }, this);
            }
            else {
                append_menu_item(languageMenu, wxID_ANY, _L("common_menu_settings_languagecn"), _L("Switch language to Chinese"),
                    [this](wxCommandEvent&) {
                        selectLanguage(GUI_App::AnkerLanguageType::AnkerLanguageType_Chinese);
                    },"", nullptr, []() { return true; },this);
            }
        }

        append_submenu(settingsMenu, languageMenu, wxID_ANY, _L("common_menu_settings_language"), "");

        append_menu_item(settingsMenu, wxID_ANY, _L("common_menu_settings_ota"), _L("Open update dialog"),
            [this](wxCommandEvent&) {
                auto ankerNet = AnkerNetInst();
                if (!ankerNet) {
                    return;
                }

                ankerNet->SetOtaCheckType(OtaCheckType_Manual);
                ankerNet->queryOTAInformation();
            });
    }

    // Calibration menu removed: every calibration loaded its test model into
    // the plater (GUI/Calibration/FlowCalibration.cpp), so it went with slicing.

    // Edit menu. The slicing-era Edit menu went away with the plater, but on macOS
    // the standard Cmd-X/C/V/A key equivalents are delivered *through the menu bar* --
    // without these items the clipboard is dead in every text field in the app,
    // including the Device Details G-code box. These use the stock wx ids, which wx
    // routes to whichever control has focus, so no handlers are needed here.
    auto editMenu = new wxMenu();
    editMenu->Append(wxID_UNDO,      _L("Undo") + "\tCtrl+Z");
    editMenu->Append(wxID_REDO,      _L("Redo") + "\tShift+Ctrl+Z");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_CUT,       _L("Cut") + "\tCtrl+X");
    editMenu->Append(wxID_COPY,      _L("Copy") + "\tCtrl+C");
    editMenu->Append(wxID_PASTE,     _L("Paste") + "\tCtrl+V");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_SELECTALL, _L("Select All") + "\tCtrl+A");

    // Help menu
    auto helpMenu = generate_help_menu();

    // menubar
    // assign menubar to frame after appending items, otherwise special items
    // will not be handled correctly
    m_menubar = new wxMenuBar();
    m_menubar->SetFont(this->normal_font());
    m_menubar->Append(fileMenu, _L("common_menu_title_file"));
    m_menubar->Append(editMenu, _L("Edit"));
    if(settingsMenu) m_menubar->Append(settingsMenu, _L("common_menu_title_settings"));
    // View and Calibration are gone. Note the Calibration append was not guarded
    // on its own menu, so leaving it would have passed a null wxMenu to Append.
    // Add additional menus from C++
    //wxGetApp().add_config_menu(m_menubar);
    m_menubar->Append(helpMenu, _L("common_menu_title_help"));
    
    m_pLoginMenu = new wxMenu();
    {
        // The plugin restores any persisted session in its Init(), which has already
        // run by this point. Populate the account menu to match, and never log out
        // here -- that would delete the cached login on every launch.
        auto ankerNet = AnkerNetInst();
        if (ankerNet && ankerNet->IsLogined()) {
            // ShowUnLoginMenu() does these two on the logged-out path.
            updateCurrentEnvironment();
            updateBuryInfo();
            ShowLoginedMenu();
        }
        else {
            ShowUnLoginMenu(false);
        }
    }
    m_menubar->Append(m_pLoginMenu, _L("common_menu_account"));

#ifdef _MSW_DARK_MODE
    if (wxGetApp().tabs_as_menu()) {
        // Add separator 
        m_menubar->Append(new wxMenu(), "          ");
        add_tabs_as_menu(m_menubar, this, this);
    }
#endif
    SetMenuBar(m_menubar);

#ifdef _MSW_DARK_MODE
    if (wxGetApp().tabs_as_menu())
        m_menubar->EnableTop(6, false);
#endif

#ifdef __APPLE__
    // This fixes a bug on Mac OS where the quit command doesn't emit window close events
    // wx bug: https://trac.wxwidgets.org/ticket/18328
    wxMenu* apple_menu = m_menubar->OSXGetAppleMenu();
    if (apple_menu != nullptr) {
        apple_menu->Bind(wxEVT_MENU, [this](wxCommandEvent &) {
            Close();
        }, wxID_EXIT);
    }
#endif // __APPLE__

    // No SLA printer technology to switch the menubar for.
}
void MainFrame::buryTime()
{
    m_buryTime = wxDateTime::Now();
}
std::string MainFrame::getWorkDuration()
{
    auto nowTime = wxDateTime::Now();
    auto timeDifference = nowTime - m_buryTime;
    std::string strTimeStamp = timeDifference.GetValue().ToString().ToStdString();
    return strTimeStamp;

}
void MainFrame::selectLanguage(GUI_App::AnkerLanguageType language)
{
    /* Before change application language, let's check unsaved changes on 3D-Scene
           * and draw user's attention to the application restarting after a language change
           */
    {
        wxPoint mfPoint = wxGetApp().mainframe->GetPosition();
        wxSize mfSize = wxGetApp().mainframe->GetClientSize();
        wxSize dialogSize = AnkerSize(400, 200);
        wxPoint center = wxPoint(mfPoint.x + mfSize.GetWidth() / 2 - dialogSize.GetWidth() / 2, mfPoint.y + mfSize.GetHeight() / 2 - dialogSize.GetHeight() / 2);

        AnkerDialog dialog(nullptr, wxID_ANY, _L("common_popup_language_title"),
            _L("common_popup_language_notice"), center, dialogSize);

        int result = dialog.ShowAnkerModal(AnkerDialogType_DisplayTextNoYesDialog);
        ANKER_LOG_INFO << "result: " << result;
        if (result != wxID_OK) {
            return;
        }
    }

    // No slicing export can be in flight to block a language switch.

    wxGetApp().switch_language(language);
    //by samuel,should upodate language type in  DataManger
    wxString languageCode = wxGetApp().current_language_code_safe();
    int index = languageCode.find('_');

    auto ankerNet = AnkerNetInst();
    if (ankerNet) {
        std::string Language  = languageCode.substr(0, index).ToStdString();
        std::string Country = languageCode.substr(index + 1, languageCode.Length() - index).ToStdString();
        ankerNet->ResetLanguage(Country, Language);
    }
}


bool MainFrame::currentSoftwareLanguageIsJapanese()
{
    // wxLanguage::wxLANGUAGE_CHINESE_CHINA = 130
    // wxLanguage::wxLANGUAGE_ENGLISH = 175
    // wxLanguage::wxLANGUAGE_JAPANESE = 428
    int type = wxGetApp().getCurrentLanguageType();
    ANKER_LOG_INFO << "The current software language type is " << type;
    if (type == wxLanguage::wxLANGUAGE_JAPANESE) {
        return true;
    }
    return false;
}

bool MainFrame::languageIsJapanese()
{
#ifdef _WIN32
    int sysLanguage = wxLocale::GetSystemLanguage();
    wxString sysLanguageName = wxLocale::GetLanguageName(wxLANGUAGE_JAPANESE_JAPAN);
    ANKER_LOG_INFO << "MainFrame::languageIsJapanese: " << ", sysLanguage: " << sysLanguage << ", wxLANGUAGE_JAPANESE: " << wxLANGUAGE_JAPANESE_JAPAN;
    return (sysLanguage == wxLanguage::wxLANGUAGE_JAPANESE_JAPAN ||
        sysLanguage == wxLanguage::wxLANGUAGE_JAPANESE) ;
#elif __APPLE__
    FILE* fp = NULL;
    char buffer[1024];
    //Get Language info.
    memset(buffer, 0, 1024);
    fp = popen("defaults read NSGlobalDomain AppleLanguages", "r");
    if (fp == NULL) {
        ANKER_LOG_ERROR << "Failed to read NSGlobalDomain AppleLanguages.";
        return false;
    }

    ANKER_LOG_INFO << "Language info:";
    std::string languageStr = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        languageStr += std::string(buffer);
    }
    pclose(fp);
    ANKER_LOG_INFO << "Language info:" << languageStr;
    int index1 = languageStr.find('(');
    int index2 = languageStr.find(',');

    // Consider the case where there is only one language
    if (index2 == std::string::npos)
    {
        index2 = languageStr.find(')');
        if (index2 == std::string::npos)
        {
            ANKER_LOG_INFO << "system langage info formate exception,please check";
            return false;
        }
    }

    if (index1 != std::string::npos) {
        index1++;
        std::string defaultLanguage = languageStr.substr(index1, index2 - index1);
        ANKER_LOG_INFO << "defaultLanguage: " << defaultLanguage ;
        if (defaultLanguage.find("ja-") != std::string::npos) {
            return true;
        }
    }

    return false;
#endif

}


std::string MainFrame::GetTranslateLanguage()
{
    bool multi_language_enable = false;
    GUI::GUI_App* gui = dynamic_cast<GUI::GUI_App*>(GUI::GUI_App::GetInstance());
    if (gui != nullptr && gui->app_config->get_bool("multi-language_enable"))
        multi_language_enable = true;

#ifdef _WIN32
    int sysLanguage = wxLocale::GetSystemLanguage();
    wxString sysLanguageName = wxLocale::GetLanguageName(wxLANGUAGE_JAPANESE_JAPAN);
    ANKER_LOG_INFO << "MainFrame::languageIsJapanese: " << ", sysLanguage: " << sysLanguage << ", wxLANGUAGE_JAPANESE: " << wxLANGUAGE_JAPANESE_JAPAN;
    if ((sysLanguage == wxLanguage::wxLANGUAGE_JAPANESE_JAPAN || sysLanguage == wxLanguage::wxLANGUAGE_JAPANESE) && multi_language_enable)
        return std::string("ja");
    else  if (sysLanguage == wxLanguage::wxLANGUAGE_CHINESE_CHINA && multi_language_enable)
        return std::string("zh_CN");
    else
        return std::string("en");

#elif __APPLE__
    FILE* fp = NULL;
    char buffer[1024];
    //Get Language info.
    memset(buffer, 0, 1024);
    fp = popen("defaults read NSGlobalDomain AppleLanguages", "r");
    if (fp == NULL) {
        ANKER_LOG_ERROR << "Failed to read NSGlobalDomain AppleLanguages.";
        return std::string("en");
    }

    ANKER_LOG_INFO << "Language info:";
    std::string languageStr = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        languageStr += std::string(buffer);
    }
    pclose(fp);
    ANKER_LOG_INFO << "Language info:" << languageStr;
    int index1 = languageStr.find('(');
    int index2 = languageStr.find(',');
    // Consider the case where there is only one language
    if (index2 == std::string::npos)
    {
        index2 = languageStr.find(')');
        if (index2 == std::string::npos)
        {
            ANKER_LOG_INFO << "system langage info formate exception,please check";
            return std::string("en");
        }
    }

    if (index1 != std::string::npos) {
        index1++;
        std::string defaultLanguage = languageStr.substr(index1, index2 - index1);
        ANKER_LOG_INFO << "defaultLanguage: " << defaultLanguage;
        if (defaultLanguage.find("ja-") != std::string::npos && multi_language_enable) {
            return std::string("ja");
        }
        else if (defaultLanguage.find("zh-Hans-CN") != std::string::npos && multi_language_enable) {
            return std::string("zh_CN");
        }
        else
            return std::string("en");

    }

    return std::string("en");
#endif
}

void MainFrame::open_menubar_item(const wxString& menu_name,const wxString& item_name)
{
    if (m_menubar == nullptr)
        return;
    // Get menu object from menubar
    int     menu_index = m_menubar->FindMenu(menu_name);
    wxMenu* menu       = m_menubar->GetMenu(menu_index);
    if (menu == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "Mainframe open_menubar_item function couldn't find menu: " << menu_name;
        return;
    }
    // Get item id from menu
    int     item_id   = menu->FindItem(item_name);
    if (item_id == wxNOT_FOUND)
    {
        // try adding three dots char
        item_id = menu->FindItem(item_name + dots);
    }
    if (item_id == wxNOT_FOUND)
    {
        BOOST_LOG_TRIVIAL(error) << "Mainframe open_menubar_item function couldn't find item: " << item_name;
        return;
    }
    // wxEVT_MENU will trigger item
    wxPostEvent((wxEvtHandler*)menu, wxCommandEvent(wxEVT_MENU, item_id));
}


void MainFrame::update_menubar()
{
    // Nothing left to relabel. This retitled menu items between their FFF and SLA
    // wordings ("Export G-code"/"Export", "Filament"/"Material Settings Tab"). The
    // Export item lived in the File menu and the Filament/Printer items in the
    // Window menu; both menus are gone.
    //
    // It indexed m_changeable_menu_items by the MenuItems enum (miExport = 0 ...
    // miPrinterTab = 3), relying on the File menu pushing the first two entries.
    // With those gone the vector held two elements and [miMaterialTab] read past
    // the end -- a segfault at startup, via Plater::set_printer_technology().
    //
    // This fork is FFF-only (M5 / M5C), so there is no SLA wording to switch to.
}

#if 0
// To perform the "Quck Slice", "Quick Slice and Save As", "Repeat last Quick Slice" and "Slice to SVG".
#endif




// Load a config file containing a Print, Filament & Printer preset.

// Load a config file containing a Print, Filament & Printer preset from command line.


// Loading a config bundle with an external file name used to be used
// to auto - install a config bundle on a fresh user account,
// but that behavior was not documented and likely buggy.

// Load a provied DynamicConfig into the Print / Filament / Printer tabs, thus modifying the active preset.
// Also update the plater with the new presets.




void MainFrame::showAnkerCfgDlg() {
    if (m_ankerCfgDlg && !m_ankerCfgDlg->IsShown()) {
        m_ankerCfgDlg->CenterOnParent();
        m_ankerCfgDlg->ShowModal();
    }
}


// Set a camera direction, zoom to all objects.

// #ys_FIXME_to_delete

// #ys_FIXME_to_delete

void MainFrame::on_size(wxSizeEvent& event)
{

    event.Skip();
}

void MainFrame::on_move(wxMoveEvent& event)
{

    event.Skip();
}

void MainFrame::on_show(wxShowEvent& event)
{

    if (m_pDeviceWidget)
        m_pDeviceWidget->activate(event.IsShown());

    if (m_MsgCentreDialog)
    {
        if (m_isMsgCenterIsShow)
        {
            m_MsgCentreDialog->Raise();
            m_MsgCentreDialog->Show();
        }
    }

    event.Skip();
}

void MainFrame::on_minimize(wxIconizeEvent& event)
{

    if (m_pDeviceWidget)
        m_pDeviceWidget->activate(false);

    if (m_pMsgCentrePopWindow)
        m_MsgCentreDialog->Hide();
    event.Skip();
}

void MainFrame::on_Activate(wxActivateEvent& event)
{
    if (m_MsgCentreDialog)
    {
        if (m_isMsgCenterIsShow)
        {
            if (m_MsgCentreDialog->IsShown())
                return;
            m_MsgCentreDialog->Raise();
            m_MsgCentreDialog->Show();
        }
    }
    event.Skip();
}

void MainFrame::on_maximize(wxMaximizeEvent& event)
{

    if (m_pDeviceWidget)
        m_pDeviceWidget->activate(true);

    event.Skip();

    if (m_MsgCentreDialog)
    {
        if (m_isMsgCenterIsShow)
        {
            m_MsgCentreDialog->Raise();
            m_MsgCentreDialog->Show();
        }
    }
}



void MainFrame::add_to_recent_projects(const wxString& filename)
{
    if (wxFileExists(filename))
    {
        m_recent_projects.AddFileToHistory(filename);
        std::vector<std::string> recent_projects;
        size_t count = m_recent_projects.GetCount();
        for (size_t i = 0; i < count; ++i)
        {
            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
        }
        wxGetApp().app_config->set_recent_projects(recent_projects);
    }
}
void MainFrame::clearStarCommentData()
{
    m_showCommentWebView = false;
    
    g_sliceCommentData.reviewNameID = "";
    g_sliceCommentData.reviewName = "";
    g_sliceCommentData.appVersion = "";
    g_sliceCommentData.country = "";

    g_sliceCommentData.sliceCount = "";

    g_sliceCommentData.action = 2;
    g_sliceCommentData.rating = 0;
    g_sliceCommentData.reviewData = "";
    g_sliceCommentData.clientId = "";
}

//
// Called after the Preferences dialog is closed and the program settings are saved.
// Update the UI based on the current preferences.

std::string MainFrame::get_base_name(const wxString &full_name, const char *extension) const 
{
    boost::filesystem::path filename = boost::filesystem::path(full_name.wx_str()).filename();
    if (extension != nullptr)
		filename = filename.replace_extension(extension);
    return filename.string();
}

std::string MainFrame::get_dir_name(const wxString &full_name) const 
{
    return boost::filesystem::path(full_name.wx_str()).parent_path().string();
}

// add by allen for ankerCfgDlg

void MainFrame::updateMsgCenterItemContent(std::vector<MsgCenterItem>* pData)
{
    if (!pData || pData->size() <= 0)
        return;

    //std::string currentLanguage = GetTranslateLanguage();
    std::string currentLanguage = "";
    int type = Slic3r::GUI::wxGetApp().getCurrentLanguageType();
    if (type == wxLanguage::wxLANGUAGE_JAPANESE)
    {
        currentLanguage = "ja";
    }
    else
    {
        currentLanguage = "en";//wxLanguage::wxLANGUAGE_ENGLISH
    }

    if (!m_MsgCenterCfg)
    {
        m_MsgCenterCfg = new std::map<std::string, MsgCenterConfig>();
        if (!loadMsgCenterCfg())
        {
            //load local cfg msgCenterCfgVersionInfo
            ANKER_LOG_INFO << "no any msg center config fail ";
            return;
        }
    }       

    if (!m_MsgCenterErrCodeInfo)
    {
        m_MsgCenterErrCodeInfo = new std::vector<MsgErrCodeInfo>();
        if (!loadMsgCenterMultiLanguageCfg())
        {
            //load local cfg msgCenterMultiLanguageCfg
            ANKER_LOG_INFO << "no any msg center updateMsgCenterItemContent fail ";
            return;
        }
    }

    for (auto item = pData->begin(); item != pData->end(); ++item)
    {
       std::string errorCode = (*item).msgErrorCode;
       std::string realErrorCode = "fdm_news_center_" + (*item).msgErrorCode + "_desc";
       auto urlItem = m_MsgCenterCfg->find(errorCode);
       if (urlItem != m_MsgCenterCfg->end())
       {
           auto articleList = (*urlItem).second.article_info;
           for (auto articleListItem : articleList)
           {
               if (articleListItem.language == currentLanguage)
               {
                   (*item).msgUrl = articleListItem.article_url;//find url
                   (*item).msgLevel = (*urlItem).second.error_level;//find url
                   break;
               }
           }
       }

       for (auto it = m_MsgCenterErrCodeInfo->begin(); it != m_MsgCenterErrCodeInfo->end(); ++it) {
           if ((*it).language == currentLanguage)
           {
               auto ErrCodeUrlMap = (*it).errorCodeUrlMap;
               auto resItem = ErrCodeUrlMap.find(realErrorCode);
               if (resItem != ErrCodeUrlMap.end())
               {
                   //wxString::FromUTF8((*resItem).second).ToUTF8().data();
                   //(*item).msgContent = (*resItem).second;//find error content
                   (*item).msgContent = wxString::FromUTF8((*resItem).second).data();//find error content
               }
           }
       }       
    }
}


// ----------------------------------------------------------------------------
// SettingsDialog
// ----------------------------------------------------------------------------

SettingsDialog::SettingsDialog(MainFrame* mainframe)
:DPIFrame(NULL, wxID_ANY, wxString(SLIC3R_APP_NAME) + " - " + _L("Settings"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "settings_dialog", mainframe->normal_font().GetPointSize()),
//: DPIDialog(mainframe, wxID_ANY, wxString(SLIC3R_APP_NAME) + " - " + _L("Settings"), wxDefaultPosition, wxDefaultSize,
//        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX, "settings_dialog"),
    m_main_frame(mainframe)
{
    if (wxGetApp().is_gcode_viewer())
        return;

#if defined(__WXMSW__)
    // ys_FIXME! temporary workaround for correct font scaling
    // Because of from wxWidgets 3.1.3 auto rescaling is implemented for the Fonts,
    // From the very beginning set dialog font to the wxSYS_DEFAULT_GUI_FONT
    this->SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
#else
    this->SetFont(wxGetApp().normal_font());
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif // __WXMSW__

    // Load the icon either from the exe, or from the ico file.
#if _WIN32
    {
        TCHAR szExeFileName[MAX_PATH];
        GetModuleFileName(nullptr, szExeFileName, MAX_PATH);
        SetIcon(wxIcon(szExeFileName, wxBITMAP_TYPE_ICO));
    }
#else
    SetIcon(wxIcon(var("AnkerStudio_128px.png"), wxBITMAP_TYPE_PNG));
#endif // _WIN32

    this->Bind(wxEVT_SHOW, [this](wxShowEvent& evt) {

        auto key_up_handker = [this](wxKeyEvent& evt) {
            if ((evt.GetModifiers() & wxMOD_CONTROL) != 0) {
                switch (evt.GetKeyCode()) {
                // Ctrl+1..4 selected the plater and the print/filament/printer
                // preset tabs; all four are gone.
#ifdef __APPLE__
                case 'f':
#else /* __APPLE__ */
                case WXK_CONTROL_F:
#endif /* __APPLE__ */
                case 'F': break;   // plater search went with the object list
                default:break;
                }
            }
        };

        if (evt.IsShown()) {
            if (m_tabpanel != nullptr)
                m_tabpanel->Bind(wxEVT_KEY_UP, key_up_handker);
        }
        else {
            if (m_tabpanel != nullptr)
                m_tabpanel->Unbind(wxEVT_KEY_UP, key_up_handker);
        }
        });

    //just hide the Frame on closing
    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& evt) { this->Hide(); });

#ifdef _MSW_DARK_MODE
    if (wxGetApp().tabs_as_menu()) {
        // menubar
        m_menubar = new wxMenuBar();
        add_tabs_as_menu(m_menubar, mainframe, this);
        this->SetMenuBar(m_menubar);
    }
#endif

    // initialize layout
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->SetSizeHints(this);
    SetSizer(sizer);
    Fit();

    const wxSize min_size = wxSize(85 * em_unit(), 50 * em_unit());
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    SetSize(min_size);
#else
    SetMinSize(min_size);
    SetSize(GetMinSize());
#endif
    Layout();
}

void SettingsDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    if (wxGetApp().is_gcode_viewer())
        return;

    const int& em = em_unit();
    const wxSize& size = wxSize(85 * em, 50 * em);

#ifdef _MSW_DARK_MODE
    // update common mode sizer
    /*if (!wxGetApp().tabs_as_menu())
        dynamic_cast<Notebook*>(m_tabpanel)->Rescale();*/
#endif

    // add by allen for ankerCfgDlg
    for (auto tab : wxGetApp().ankerTabsList)
        tab->msw_rescale();

    SetMinSize(size);
    Fit();
    Refresh();
}


} // GUI
} // Slic3r
