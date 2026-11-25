// A small zip reader and writer, enough for Word files (.docx): stored and deflated entries, no encryption, no
// zip64. Uses the zlib that comes with macOS (link with -lz). Everything is in memory.
#pragma once
#include <string>
#include <vector>

namespace x52 {

struct ZipEntry {
    std::string name;   // path inside the zip, such as "word/document.xml"
    std::string data;   // the uncompressed bytes
};

// Reads every file of a zip, in the order of its central directory. False (with a reason in *error) for anything
// that is not a zip this reader understands. Folders (names ending in "/") are skipped.
bool readZip(const std::string& bytes, std::vector<ZipEntry>& out, std::string* error);

// A zip of the entries, in the given order (Word wants [Content_Types].xml first).
std::string writeZip(const std::vector<ZipEntry>& entries);

}  // namespace x52
