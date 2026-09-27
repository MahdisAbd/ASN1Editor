#include "asn1viewer/ber.hpp"

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <cctype>

namespace asn1viewer {

BinaryReader::BinaryReader(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
std::uint64_t BinaryReader::size() const noexcept { return bytes_.size(); }
bool BinaryReader::canRead(std::uint64_t offset, std::uint64_t length) const noexcept {
    return offset <= bytes_.size() && length <= bytes_.size() - offset;
}
std::uint8_t BinaryReader::at(std::uint64_t offset) const {
    if (!canRead(offset, 1)) throw std::out_of_range("byte offset outside input");
    return bytes_[static_cast<std::size_t>(offset)];
}
std::vector<std::uint8_t> BinaryReader::copy(ByteRange range) const {
    if (!canRead(range.offset, range.length)) throw std::out_of_range("range outside input");
    auto begin = bytes_.begin() + static_cast<std::ptrdiff_t>(range.offset);
    auto end = begin + static_cast<std::ptrdiff_t>(range.length);
    return {begin, end};
}
const std::vector<std::uint8_t>& BinaryReader::bytes() const noexcept { return bytes_; }

std::string hex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream out;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i) out << ' ';
        out << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}

std::string tagClassName(TagClass tagClass) {
    switch (tagClass) {
    case TagClass::Universal: return "Universal";
    case TagClass::Application: return "Application";
    case TagClass::ContextSpecific: return "Context-specific";
    case TagClass::Private: return "Private";
    }
    return "Unknown";
}

std::string statusName(DecodeStatus status) {
    switch (status) {
    case DecodeStatus::Ok: return "ok";
    case DecodeStatus::Unknown: return "unknown";
    case DecodeStatus::Malformed: return "malformed";
    case DecodeStatus::Unsupported: return "unsupported";
    }
    return "unknown";
}

std::string universalTypeName(std::uint64_t tag) {
    switch (tag) {
    case 1: return "BOOLEAN"; case 2: return "INTEGER"; case 3: return "BIT STRING";
    case 4: return "OCTET STRING"; case 5: return "NULL"; case 6: return "OBJECT IDENTIFIER";
    case 10: return "ENUMERATED"; case 12: return "UTF8String"; case 16: return "SEQUENCE";
    case 17: return "SET"; case 18: return "NumericString"; case 19: return "PrintableString";
    case 22: return "IA5String"; case 23: return "UTCTime"; case 24: return "GeneralizedTime";
    case 26: return "VisibleString"; case 27: return "GeneralString";
    default: return "Universal tag " + std::to_string(tag);
    }
}

std::string valueTypeName(const Asn1Node& node) {
    if (node.tagClass != TagClass::Universal) return "Schema-defined / context-specific";
    return universalTypeName(node.tagNumber);
}

static std::string formatAsnTime(const std::string& value, bool generalized) {
    if ((!generalized && value.size() < 13) || (generalized && value.size() < 15)) return value;
    const auto digits = [&value](std::size_t start, std::size_t count) {
        return value.substr(start, count);
    };
    try {
        std::string year;
        std::size_t pos = 0;
        if (generalized) { year = digits(0, 4); pos = 4; }
        else { const auto shortYear = std::stoi(digits(0, 2)); year = shortYear >= 50 ? "19" : "20"; year += digits(0, 2); pos = 2; }
        const auto result = year + "-" + digits(pos, 2) + "-" + digits(pos + 2, 2) + " " +
            digits(pos + 4, 2) + ":" + digits(pos + 6, 2) + ":" + digits(pos + 8, 2);
        return result + (value.back() == 'Z' ? " UTC" : " " + value.substr(pos + 10));
    } catch (...) {
        return value;
    }
}

static std::string unsignedInteger(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > 8) return {};
    std::uint64_t value = 0;
    for (const auto byte : bytes) value = (value << 8) | byte;
    return std::to_string(value);
}

static std::string ipv4(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != 4) return {};
    return std::to_string(bytes[0]) + "." + std::to_string(bytes[1]) + "." +
           std::to_string(bytes[2]) + "." + std::to_string(bytes[3]);
}

static std::string tbcd(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) return {};
    std::string result;
    for (const auto byte : bytes) {
        const auto low = byte & 0x0F;
        const auto high = (byte >> 4) & 0x0F;
        if (low > 9 || high > 0x0F || (high > 9 && high != 0x0F)) return {};
        result += static_cast<char>('0' + low);
        if (high <= 9) result += static_cast<char>('0' + high);
    }
    if (!result.empty() && result.back() == 'F') result.pop_back();
    return result;
}

static std::string decodePrimitive(const BinaryReader& reader, const Asn1Node& node) {
    const auto bytes = reader.copy(node.contentRange);
    if (node.tagClass != TagClass::Universal) {
        if (node.schemaType.find("IP") != std::string::npos) {
            if (const auto value = ipv4(bytes); !value.empty()) return value;
        }
        if (node.schemaType.find("TBCD") != std::string::npos ||
            node.schemaType.find("IMSI") != std::string::npos ||
            node.schemaType.find("MSISDN") != std::string::npos ||
            node.schemaType.find("IMEI") != std::string::npos) {
            if (const auto value = tbcd(bytes); !value.empty()) return value;
        }
        if (node.schemaType.find("INTEGER") != std::string::npos ||
            node.schemaType.find("CellId") != std::string::npos ||
            node.schemaType.find("LocationArea") != std::string::npos) {
            if (const auto value = unsignedInteger(bytes); !value.empty()) return value;
        }
        return {};
    }
    switch (node.tagNumber) {
    case 1:
        return bytes.size() == 1 ? (bytes[0] == 0 ? "false" : "true") : std::string();
    case 2:
    case 10: {
        if (bytes.empty() || bytes.size() > 8) return {};
        std::int64_t value = (bytes[0] & 0x80) ? -1 : 0;
        for (const auto b : bytes) value = (value << 8) | b;
        return std::to_string(value);
    }
    case 3: {
        if (bytes.empty() || bytes[0] > 7) return {};
        const auto bitCount = (bytes.size() - 1) * 8 - bytes[0];
        if (bitCount > 32) return "BIT STRING (" + std::to_string(bitCount) + " bits)";
        std::string bits;
        for (std::size_t i = 1; i < bytes.size(); ++i)
            for (int bit = 7; bit >= 0; --bit)
                if (!(i == bytes.size() - 1 && bit < bytes[0])) bits += ((bytes[i] >> bit) & 1) ? '1' : '0';
        return bits;
    }
    case 5:
        return bytes.empty() ? "NULL" : std::string();
    case 6: {
        if (bytes.empty()) return {};
        std::vector<std::uint64_t> arcs;
        std::uint64_t value = 0;
        for (const auto b : bytes) {
            if (value > (std::numeric_limits<std::uint64_t>::max() >> 7)) return {};
            value = (value << 7) | (b & 0x7F);
            if ((b & 0x80) == 0) { arcs.push_back(value); value = 0; }
        }
        if (arcs.empty() || (bytes.back() & 0x80)) return {};
        const auto first = arcs.front(); arcs.erase(arcs.begin());
        std::string result = std::to_string(std::min<std::uint64_t>(first / 40, 2)) + "." +
            std::to_string(first < 40 ? first : (first < 80 ? first - 40 : first - 80));
        for (const auto arc : arcs) result += "." + std::to_string(arc);
        return result;
    }
    case 4:
        if (bytes.empty()) return "OCTET STRING (empty)";
        if (std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t byte) { return byte >= 0x20 && byte <= 0x7E; }))
            return std::string(bytes.begin(), bytes.end());
        if (const auto value = ipv4(bytes); !value.empty()) return value;
        return "OCTET STRING (" + std::to_string(bytes.size()) + " bytes)";
    case 12: case 18: case 19: case 22: case 26: case 27:
        return std::string(bytes.begin(), bytes.end());
    case 23:
        return formatAsnTime(std::string(bytes.begin(), bytes.end()), false);
    case 24:
        return formatAsnTime(std::string(bytes.begin(), bytes.end()), true);
    default:
        return {};
    }
}

BerDecoder::BerDecoder(const BinaryReader& reader) : reader_(reader) {}

std::shared_ptr<Asn1Node> BerDecoder::malformed(std::uint64_t offset, std::uint64_t limit,
                                                 const std::string& reason, std::uint64_t& next) {
    auto node = std::make_shared<Asn1Node>();
    node->encodedRange = {offset, limit > offset ? limit - offset : 0};
    node->contentRange = {offset, 0};
    node->status = DecodeStatus::Malformed;
    node->error = reason;
    if (reader_.canRead(offset, node->encodedRange.length)) node->rawHex = hex(reader_.copy(node->encodedRange));
    next = limit;
    return node;
}

std::shared_ptr<Asn1Node> BerDecoder::parseOne(std::uint64_t offset, std::uint64_t limit,
                                                bool stopAtEoc, std::uint64_t& next) {
    if (offset >= limit || !reader_.canRead(offset, 1)) return nullptr;
    if (stopAtEoc && reader_.at(offset) == 0 && offset + 1 < limit && reader_.at(offset + 1) == 0) {
        next = offset + 2;
        return nullptr;
    }

    const auto start = offset;
    const auto first = reader_.at(offset++);
    const auto cls = static_cast<TagClass>((first >> 6) & 0x03);
    const bool constructed = (first & 0x20) != 0;
    std::uint64_t tag = first & 0x1F;
    std::vector<std::uint8_t> tagBytes{first};
    if (tag == 0x1F) {
        tag = 0;
        bool terminated = false;
        for (int count = 0; count < 9 && offset < limit; ++count) {
            const auto b = reader_.at(offset++);
            tagBytes.push_back(b);
            if (tag > (std::numeric_limits<std::uint64_t>::max() >> 7))
                return malformed(start, limit, "high-tag-number overflow", next);
            tag = (tag << 7) | (b & 0x7F);
            if ((b & 0x80) == 0) { terminated = true; break; }
        }
        if (!terminated) return malformed(start, limit, "unterminated high-tag-number", next);
    }

    if (offset >= limit) return malformed(start, limit, "missing length octet", next);
    const auto lengthStart = offset;
    const auto lengthFirst = reader_.at(offset++);
    std::vector<std::uint8_t> lengthBytes{lengthFirst};
    bool indefinite = false;
    std::uint64_t contentLength = 0;
    if (lengthFirst == 0x80) {
        indefinite = true;
        if (!constructed) return malformed(start, limit, "indefinite length on primitive value", next);
    } else if ((lengthFirst & 0x80) == 0) {
        contentLength = lengthFirst;
    } else {
        const auto count = lengthFirst & 0x7F;
        if (count == 0 || count > 8 || !reader_.canRead(offset, count))
            return malformed(start, limit, "invalid long-form length", next);
        if (reader_.at(offset) == 0) return malformed(start, limit, "non-minimal long-form length", next);
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto b = reader_.at(offset++);
            lengthBytes.push_back(b);
            contentLength = (contentLength << 8) | b;
        }
    }

    const auto contentStart = offset;
    if (!indefinite && (contentLength > limit - contentStart || !reader_.canRead(contentStart, contentLength)))
        return malformed(start, limit, "content length exceeds enclosing boundary", next);

    auto node = std::make_shared<Asn1Node>();
    node->tagClass = cls; node->tagNumber = tag; node->constructed = constructed;
    node->tagBytes = std::move(tagBytes); node->lengthBytes = std::move(lengthBytes);
    node->contentRange = {contentStart, indefinite ? 0 : contentLength};
    if (constructed) {
        next = decodeChildren(node, indefinite ? limit : contentStart + contentLength, indefinite);
    } else {
        node->rawValueHex = hex(reader_.copy(node->contentRange));
        node->decodedValue = decodePrimitive(reader_, *node);
        next = contentStart + contentLength;
    }
    node->encodedRange = {start, next - start};
    node->rawHex = hex(reader_.copy(node->encodedRange));
    if (node->decodedValue.empty() && !constructed && node->tagClass == TagClass::Universal)
        node->status = DecodeStatus::Unknown;
    (void)lengthStart;
    return node;
}

std::uint64_t BerDecoder::decodeChildren(const std::shared_ptr<Asn1Node>& node, std::uint64_t contentEnd,
                                         bool indefinite) {
    auto cursor = node->contentRange.offset;
    while (cursor < contentEnd) {
        std::uint64_t next = cursor;
        const auto child = parseOne(cursor, contentEnd, indefinite, next);
        if (!child) {
            if (indefinite && next == cursor + 2) {
                node->contentRange.length = cursor - node->contentRange.offset;
                node->encodedRange.length = (cursor + 2) - node->encodedRange.offset;
            }
            return next;
        }
        node->children.push_back(child);
        if (next <= cursor) { node->status = DecodeStatus::Malformed; node->error = "decoder made no progress"; return cursor; }
        cursor = next;
    }
    if (indefinite) { node->status = DecodeStatus::Malformed; node->error = "missing end-of-contents marker"; }
    return cursor;
}

std::vector<std::shared_ptr<Asn1Node>> BerDecoder::decodeAll() {
    std::vector<std::shared_ptr<Asn1Node>> result;
    std::uint64_t cursor = 0;
    while (cursor < reader_.size()) {
        std::uint64_t next = cursor;
        auto node = parseOne(cursor, reader_.size(), false, next);
        if (!node) break;
        result.push_back(std::move(node));
        if (next <= cursor) break;
        cursor = next;
    }
    return result;
}

} // namespace asn1viewer
