#pragma once

#include "asn1viewer/ber.hpp"

#include <cstdint>
#include <optional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace asn1viewer {

struct SchemaDefinition {
    std::string name;
    std::string kind;
    std::string baseType;
    std::optional<TagClass> tagClass;
    std::optional<std::uint64_t> tagNumber;
    std::vector<std::string> fields;
    struct Field {
        std::string name;
        std::string typeName;
        std::optional<TagClass> tagClass;
        std::optional<std::uint64_t> tagNumber;
        bool optional = false;
        bool repeated = false;
    };
    std::vector<Field> fieldDefinitions;
    std::map<std::int64_t, std::string> namedValues;
    std::uint32_t sourceLine = 0;
};

struct SchemaMatch {
    bool matched = false;
    std::string specification;
    std::string rootType;
    std::string confidence;
    std::string reason;
};

class SchemaRegistry {
public:
    bool loadFile(const std::string& path, std::string& error);
    const SchemaDefinition* find(const std::string& name) const;
    const SchemaDefinition::Field* resolveField(const std::string& parentType,
                                                 TagClass tagClass, std::uint64_t tagNumber) const;
    std::string resolveStorageType(const std::string& semanticType) const;
    const std::map<std::int64_t, std::string>* namedValues(const std::string& typeName) const;
    const std::string& moduleName() const noexcept;
    std::vector<std::string> names() const;

private:
    std::unordered_map<std::string, SchemaDefinition> definitions_;
    std::string moduleName_;
};

// Applies only unambiguous top-level schema names/tags. Deep semantic matching is
// deliberately separate so generic decoding remains usable for incomplete schemas.
void annotateWithSchema(const std::vector<std::shared_ptr<Asn1Node>>& nodes,
                        const SchemaRegistry& registry, const std::string& rootType);

SchemaMatch detectSchema(const std::vector<std::shared_ptr<Asn1Node>>& roots);
void applySchemaFormatting(const std::vector<std::shared_ptr<Asn1Node>>& nodes,
                           const BinaryReader& reader, const SchemaRegistry& registry);

} // namespace asn1viewer
