#include "zipfile.h"

#include <zlib.h>

#include <cstdint>
#include <cstring>

namespace x52 {

namespace {
uint16_t u16(const std::string& s, size_t at) {
    return (uint16_t)((uint8_t)s[at] | ((uint8_t)s[at + 1] << 8));
}
uint32_t u32(const std::string& s, size_t at) {
    return (uint32_t)u16(s, at) | ((uint32_t)u16(s, at + 2) << 16);
}
void put16(std::string& s, uint16_t v) {
    s += (char)(v & 0xff);
    s += (char)(v >> 8);
}
void put32(std::string& s, uint32_t v) {
    put16(s, (uint16_t)(v & 0xffff));
    put16(s, (uint16_t)(v >> 16));
}
bool fail(std::string* error, const char* why) {
    if (error) *error = why;
    return false;
}

bool inflateRaw(const char* in, size_t inSize, size_t outSize, std::string& out) {
    out.assign(outSize, '\0');
    z_stream z;
    std::memset(&z, 0, sizeof z);
    if (inflateInit2(&z, -15) != Z_OK) return false;
    z.next_in = (Bytef*)in;
    z.avail_in = (uInt)inSize;
    z.next_out = (Bytef*)&out[0];
    z.avail_out = (uInt)outSize;
    int r = inflate(&z, Z_FINISH);
    bool ok = (r == Z_STREAM_END || (r == Z_OK && outSize == 0)) && z.total_out == outSize;
    inflateEnd(&z);
    return ok;
}

bool deflateRaw(const std::string& in, std::string& out) {
    z_stream z;
    std::memset(&z, 0, sizeof z);
    if (deflateInit2(&z, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
    out.assign(deflateBound(&z, (uLong)in.size()) + 16, '\0');
    z.next_in = (Bytef*)in.data();
    z.avail_in = (uInt)in.size();
    z.next_out = (Bytef*)&out[0];
    z.avail_out = (uInt)out.size();
    int r = deflate(&z, Z_FINISH);
    out.resize(z.total_out);
    deflateEnd(&z);
    return r == Z_STREAM_END;
}
}  // namespace

bool readZip(const std::string& b, std::vector<ZipEntry>& out, std::string* error) {
    out.clear();
    if (b.size() < 22) return fail(error, "not a zip file");
    // End of central directory: the last 22 bytes, unless the zip has a comment.
    size_t eocd = std::string::npos;
    size_t lowest = b.size() > 22 + 65535 ? b.size() - 22 - 65535 : 0;
    for (size_t i = b.size() - 22 + 1; i-- > lowest;)
        if (u32(b, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd == std::string::npos) return fail(error, "not a zip file");
    size_t count = u16(b, eocd + 10), at = u32(b, eocd + 16);
    if (u16(b, eocd + 8) != count) return fail(error, "zip files in several parts are not supported");
    for (size_t n = 0; n < count; ++n) {
        if (at + 46 > b.size() || u32(b, at) != 0x02014b50) return fail(error, "damaged zip file");
        uint16_t flags = u16(b, at + 8), method = u16(b, at + 10);
        uint32_t crc = u32(b, at + 16), csize = u32(b, at + 20), usize = u32(b, at + 24);
        size_t nameLen = u16(b, at + 28), extraLen = u16(b, at + 30), commentLen = u16(b, at + 32);
        size_t local = u32(b, at + 42);
        if (at + 46 + nameLen > b.size()) return fail(error, "damaged zip file");
        std::string name = b.substr(at + 46, nameLen);
        at += 46 + nameLen + extraLen + commentLen;
        if (csize == 0xffffffffu || usize == 0xffffffffu) return fail(error, "zip64 files are not supported");
        if (flags & 1) return fail(error, "encrypted zip files are not supported");
        if (!name.empty() && name.back() == '/') continue;
        if (local + 30 > b.size() || u32(b, local) != 0x04034b50) return fail(error, "damaged zip file");
        size_t start = local + 30 + u16(b, local + 26) + u16(b, local + 28);
        if (start + csize > b.size()) return fail(error, "damaged zip file");
        ZipEntry e;
        e.name = name;
        if (method == 0) {
            if (csize != usize) return fail(error, "damaged zip file");
            e.data = b.substr(start, csize);
        } else if (method == 8) {
            if (!inflateRaw(b.data() + start, csize, usize, e.data)) return fail(error, "damaged zip file");
        } else {
            return fail(error, "unsupported zip compression");
        }
        if ((uint32_t)crc32(0, (const Bytef*)e.data.data(), (uInt)e.data.size()) != crc) return fail(error, "damaged zip file");
        out.push_back(e);
    }
    return true;
}

std::string writeZip(const std::vector<ZipEntry>& entries) {
    std::string out, central;
    for (const ZipEntry& e : entries) {
        std::string packed;
        uint16_t method = 8;
        if (!deflateRaw(e.data, packed) || packed.size() >= e.data.size()) {
            packed = e.data;
            method = 0;
        }
        uint32_t crc = (uint32_t)crc32(0, (const Bytef*)e.data.data(), (uInt)e.data.size());
        uint32_t offset = (uint32_t)out.size();
        const uint16_t dosTime = 0, dosDate = 0x0021;   // 1980-01-01: the same bytes every time
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0);
        put16(out, method);
        put16(out, dosTime);
        put16(out, dosDate);
        put32(out, crc);
        put32(out, (uint32_t)packed.size());
        put32(out, (uint32_t)e.data.size());
        put16(out, (uint16_t)e.name.size());
        put16(out, 0);
        out += e.name;
        out += packed;
        put32(central, 0x02014b50);
        put16(central, 20);
        put16(central, 20);
        put16(central, 0);
        put16(central, method);
        put16(central, dosTime);
        put16(central, dosDate);
        put32(central, crc);
        put32(central, (uint32_t)packed.size());
        put32(central, (uint32_t)e.data.size());
        put16(central, (uint16_t)e.name.size());
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, offset);
        central += e.name;
    }
    uint32_t centralAt = (uint32_t)out.size();
    out += central;
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, (uint16_t)entries.size());
    put16(out, (uint16_t)entries.size());
    put32(out, (uint32_t)central.size());
    put32(out, centralAt);
    put16(out, 0);
    return out;
}

}  // namespace x52
