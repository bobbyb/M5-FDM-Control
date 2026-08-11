#include "AcodeArchive.hpp"

#include <chrono>
#include <regex>

#include <boost/filesystem.hpp>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/tarstrm.h>

#include "libslic3r/Utils.hpp"

namespace Slic3r {
namespace AcodeArchive {

namespace fs = boost::filesystem;

std::string extract_path()
{
    fs::path temp_path(wxStandardPaths::Get().GetTempDir().utf8_str().data());
    temp_path /= "acode_extract";
    return temp_path.string();
}

void clear_extract_path()
{
    fs::path directory(extract_path());
    if (fs::exists(directory) && fs::is_directory(directory)) {
        for (auto& file : fs::directory_iterator(directory)) {
            try {
                fs::remove_all(file);
            }
            catch (const std::exception& e) {
                ANKER_LOG_ERROR << "Error removing file: " << file.path().string() << " - " << e.what();
            }
        }
    }
}

bool extract_files_from_tar(const wxString& tarFilePath, const wxString& outputDir, std::string file_pattern_regex)
{
    bool ret = false;

    wxFileInputStream tarFile(tarFilePath);
    wxTarInputStream  tarStream(tarFile);

    const std::regex pattern_file(file_pattern_regex, std::regex::icase);
    if (tarFile.IsOk() && tarStream.IsOk()) {
        wxTarEntry* entry = nullptr;
        while ((entry = tarStream.GetNextEntry()) != nullptr) {
            wxString entryName = entry->GetName();

            if (std::regex_match(entryName.ToStdString(wxConvUTF8), pattern_file)) {
                wxString outputPath = wxFileName(outputDir, entryName).GetFullPath();

                if (entry->IsDir()) {
                    wxFileName::Mkdir(outputPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
                }
                else {
                    wxFileOutputStream outputFile(outputPath);
                    if (outputFile.IsOk()) {
                        outputFile.Write(tarStream);
                        ret = true;
                    }
                    else {
                        ANKER_LOG_ERROR << "Error creating output file: " << outputPath;
                    }
                }
            }
            delete entry;
        }
    }
    else {
        ANKER_LOG_ERROR << "Error opening tar file.";
    }

    return ret;
}

wxString extract_aigcode(const wxString& tarFilePath)
{
    auto RenameFile = [](const wxString& filePath, const wxString& newFileName) -> bool {
        wxFileName fileName(filePath);
        if (fileName.FileExists()) {
            wxString newPath = fileName.GetPath() + wxFILE_SEP_PATH + newFileName;
            wxRenameFile(filePath, newPath);
            return true;
        }
        ANKER_LOG_ERROR << "File does not exist:" << filePath;
        return false;
    };

    auto GetTimestampString = []() -> std::string {
        auto now       = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        return std::to_string(timestamp);
    };

    std::string acode_extract_path_str = extract_path();
    fs::path    acode_extract_path(acode_extract_path_str);
    if (!fs::exists(acode_extract_path)) {
        fs::create_directory(acode_extract_path);
    }

    std::string aiGcodeFile  = "aiGcode.gcode";
    wxString    aiGcodePath  = wxFileName(acode_extract_path_str, aiGcodeFile).GetFullPath();

    if (extract_files_from_tar(tarFilePath, acode_extract_path_str, aiGcodeFile)) {
        wxFileName  tarFile(tarFilePath);
        wxString    tarFileName       = tarFile.GetFullName();
        std::string currTimeStamp     = GetTimestampString();
        wxString    aiGcodeFileNewName = tarFileName + "_" + currTimeStamp + "_" + aiGcodeFile;

        if (RenameFile(aiGcodePath, aiGcodeFileNewName))
            return wxFileName(acode_extract_path_str, aiGcodeFileNewName).GetFullPath();
    }

    return "";
}

} // namespace AcodeArchive
} // namespace Slic3r
