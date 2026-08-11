#ifndef slic3r_GcodeInfoScanner_hpp_
#define slic3r_GcodeInfoScanner_hpp_

// Standalone scanner for the summary comments a slicer writes into a G-code file.
//
// This is a lift of GCodeProcessor::process_file_ext() with no dependency on
// libslic3r. The original lives on GCodeProcessor, whose header pulls in
// PrintConfig.hpp, which drags in the whole preset/config subsystem -- but that
// dependency belongs to the *simulation* path (process_file()), not to this one.
// process_file_ext() only ever looks at lines beginning with ';', so nothing it
// does needs a print configuration.
//
// Keeping it separate lets the slicing core be removed while the app can still
// read print time, filament usage, and the preview thumbnail out of an
// already-sliced file before sending it to the printer.

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r {

// Mirrors the fields of Slic3r::GCodeProcessorResultExt.
struct GcodePrintInfo
{
    std::string          base64_str;               // preview thumbnail, base64
    std::string          speed;                    // max print speed, mm/s
    std::string          filament_used_weight_g;
    std::string          filament_used_length_mm;
    std::string          filament_used_cost;
    std::array<float, 3> boxSize{ 0.f, 0.f, 0.f }; // object bounding box, mm
    float                print_time = 0.f;         // seconds

    void reset();
};

class GcodeInfoScanner
{
public:
    // Which slicer wrote the file. The full list is kept even though only
    // AnkerMake / AnkerStudio / PrusaSlicer / Cura / OrcaSlicer carry comments we
    // read: a recognised-but-unsupported producer stops the scan on the spot,
    // whereas an unrecognised one would keep searching to the end of the file.
    enum class EProducer
    {
        Unknown,
        AnkerStudio,
        PrusaSlicer,
        Slic3rPE,
        Slic3r,
        SuperSlicer,
        Cura,
        Simplify3D,
        CraftWare,
        ideaMaker,
        KissSlicer,
        BambuStudio,
        AnkerMake,
        OrcaSlicer,
    };

    // Reads the summary comments out of a G-code / .acode file.
    // utf8Path must be UTF-8. Returns false if the file could not be opened.
    // A file whose producer we do not recognise returns true with `out` left empty.
    static bool Scan(const std::string& utf8Path, GcodePrintInfo& out);

    // Decodes a base64 thumbnail to raw image bytes. Empty when there was no
    // thumbnail; callers substitute a placeholder in that case.
    static std::vector<unsigned char> DecodeBase64(const std::string& base64);

    // Convenience wrapper mirroring the old GCodeThumbnails::base64ToImage.
    // Templated on the image/stream types so this header stays free of wxWidgets
    // and the scanner can be unit-tested on its own.
    template<typename ImageType, typename StreamType>
    static ImageType Base64ToImage(const std::string& base64)
    {
        ImageType image;
        std::vector<unsigned char> binary = DecodeBase64(base64);
        if (binary.empty())
            return image;

        StreamType stream(binary.data(), binary.size());
        if (ImageType::CanRead(stream))
            image = ImageType(stream);
        return image;
    }

private:
    // Largest thumbnail worth stopping the scan for. A file may carry several
    // (OrcaSlicer emits 48x48 then 300x300); we keep the biggest, and once one at
    // least this large is in hand there is no point reading further.
    static constexpr int PREFERRED_THUMBNAIL_AREA = 256 * 256;

    // Per-line comment matchers. Each takes the comment body (';' and surrounding
    // whitespace already stripped) and fills its target on a hit.
    static bool DetectProducer(std::string_view comment, EProducer& producer);

    // Matches "thumbnail begin WxH [bytes]", "thumbnail begin W H" (AnkerMake) and
    // the "thumbnail_PNG begin ..." variant, reporting W*H so the caller can keep
    // the largest. The old code matched one hardcoded 256x256 string, which meant
    // PrusaSlicer's 128x128 and OrcaSlicer's 300x300 previews were silently dropped.
    static bool SearchThumbnailBegin(std::string_view line, int& area);
    static bool SearchThumbnailEnd(std::string_view line);
    static bool SearchSpeed(std::string_view line, EProducer producer, std::string& speed);
    static bool SearchFilamentWeight(std::string_view line, EProducer producer, std::string& weight);
    static bool SearchFilamentLength(std::string_view line, EProducer producer, std::string& length);
    static bool SearchFilamentCost(std::string_view line, EProducer producer, std::string& cost);
    static bool SearchPrintTime(std::string_view line, EProducer producer, float& print_time);
    static bool SearchObjSize(std::string_view line, EProducer producer, std::array<float, 3>& boxSize);
};

} // namespace Slic3r

#endif // slic3r_GcodeInfoScanner_hpp_
