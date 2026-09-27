#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace asn1viewer {

enum class TagClass { Universal, Application, ContextSpecific, Private };
enum class DecodeStatus { Ok, Unknown, Malformed, Unsupported };

struct ByteRange {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
};

struct Asn1Node {
    ByteRange encodedRange;
    ByteRange contentRange;
    TagClass tagClass = TagClass::Universal;
    std::uint64_t tagNumber = 0;
    bool constructed = false;
    std::vector<std::uint8_t> tagBytes;
    std::vector<std::uint8_t> lengthBytes;
    std::string rawHex;
    std::string rawValueHex;
    std::string decodedValue;
    std::string normalizedValue;
    std::string normalizer;
    DecodeStatus status = DecodeStatus::Ok;
    std::string error;
    std::string displayName;
    std::string fieldName;
    std::string parentType;
    std::string schemaType;
    std::string storageType;
    std::vector<std::shared_ptr<Asn1Node>> children;
};

class BinaryReader {
public:
    explicit BinaryReader(std::vector<std::uint8_t> bytes);

    std::uint64_t size() const noexcept;
    bool canRead(std::uint64_t offset, std::uint64_t length) const noexcept;
    std::uint8_t at(std::uint64_t offset) const;
    std::vector<std::uint8_t> copy(ByteRange range) const;
    const std::vector<std::uint8_t>& bytes() const noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};

class BerDecoder {
public:
    explicit BerDecoder(const BinaryReader& reader);
    std::vector<std::shared_ptr<Asn1Node>> decodeAll();

private:
    const BinaryReader& reader_;
    std::shared_ptr<Asn1Node> parseOne(std::uint64_t offset, std::uint64_t limit,
                                       bool stopAtEoc, std::uint64_t& next);
    std::shared_ptr<Asn1Node> malformed(std::uint64_t offset, std::uint64_t limit,
                                        const std::string& reason, std::uint64_t& next);
    std::uint64_t decodeChildren(const std::shared_ptr<Asn1Node>& node, std::uint64_t contentEnd,
                                 bool indefinite);
};

std::string tagClassName(TagClass tagClass);
std::string statusName(DecodeStatus status);
std::string hex(const std::vector<std::uint8_t>& bytes);
std::string universalTypeName(std::uint64_t tagNumber);
std::string valueTypeName(const Asn1Node& node);

} // namespace asn1viewer
