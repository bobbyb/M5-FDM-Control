#ifndef slic3r_AcodeArchive_hpp_
#define slic3r_AcodeArchive_hpp_

// Handling for AnkerMake's .acode container, which is a tar holding the real
// G-code as "aiGcode.gcode".
//
// These were static members of Plater purely by accident of where they were
// written -- none of them touch plater or slicer state, they are file utilities.
// They are needed by the *print* path (GcodeInfo reads filament out of .acode
// files), so they had to survive the plater's removal.

#include <string>
#include <wx/string.h>

namespace Slic3r {
namespace AcodeArchive {

// Temp directory the container is unpacked into.
std::string extract_path();

// Empties that directory, leaving it in place.
void clear_extract_path();

// Extracts entries matching `file_pattern_regex` from a tar into `outputDir`.
bool extract_files_from_tar(const wxString& tarFilePath,
                            const wxString& outputDir,
                            std::string     file_pattern_regex = ".*");

// Unpacks aiGcode.gcode out of a .acode (tar) and returns its path, uniquified
// with the source name and a timestamp. Empty string on failure.
wxString extract_aigcode(const wxString& tarFilePath);

} // namespace AcodeArchive
} // namespace Slic3r

#endif // slic3r_AcodeArchive_hpp_
