#include "asn1viewer/schema.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <functional>

namespace asn1viewer {

static std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::vector<std::string> splitMembers(const std::string& body) {
    std::vector<std::string> members;
    std::size_t start = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '(') ++parenDepth;
        else if (body[i] == ')' && parenDepth > 0) --parenDepth;
        else if (body[i] == '{') ++braceDepth;
        else if (body[i] == '}' && braceDepth > 0) --braceDepth;
        else if (body[i] == ',' && parenDepth == 0 && braceDepth == 0) {
            members.push_back(trim(body.substr(start, i - start)));
            start = i + 1;
        }
    }
    const auto last = trim(body.substr(start));
    if (!last.empty()) members.push_back(last);
    return members;
}

static std::optional<SchemaDefinition::Field> parseField(const std::string& source) {
    auto text = trim(source);
    std::smatch match;
    if (!std::regex_search(text, match, std::regex(R"(^([A-Za-z][A-Za-z0-9-]*)\s+(.*)$)"))) return std::nullopt;
    SchemaDefinition::Field field;
    field.name = match[1].str();
    auto rest = trim(match[2].str());
    std::smatch tag;
    if (std::regex_search(rest, tag, std::regex(R"(^\[(APPLICATION\s+)?([0-9]+)\]\s*(.*)$)"))) {
        field.tagClass = tag[1].matched ? TagClass::Application : TagClass::ContextSpecific;
        field.tagNumber = std::stoull(tag[2].str());
        rest = trim(tag[3].str());
    }
    field.optional = rest.find("OPTIONAL") != std::string::npos;
    if (rest.rfind("SEQUENCE OF ", 0) == 0) { field.repeated = true; rest = trim(rest.substr(12)); }
    else if (rest.rfind("SET OF ", 0) == 0) { field.repeated = true; rest = trim(rest.substr(7)); }
    if (!std::regex_search(rest, match, std::regex(R"(^([A-Za-z][A-Za-z0-9-]*))"))) return std::nullopt;
    field.typeName = match[1].str();
    return field;
}

static void parseNamedValues(const std::string& body, std::map<std::int64_t, std::string>& values) {
    std::smatch match;
    const auto pattern = std::regex(R"(([A-Za-z][A-Za-z0-9-]*)\s*(?:\(\s*(-?[0-9]+)\s*\)|\s+(-?[0-9]+)))");
    auto begin = std::sregex_iterator(body.begin(), body.end(), pattern);
    const auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto number = (*it)[2].matched ? (*it)[2].str() : (*it)[3].str();
        values[std::stoll(number)] = (*it)[1].str();
    }
}

bool SchemaRegistry::loadFile(const std::string& path, std::string& error) {
    std::ifstream input(path);
    if (!input) { error = "cannot open schema: " + path; return false; }
    std::string line;
    std::uint32_t lineNumber = 0;
    std::string pending;
    std::regex definition(R"(^\s*([A-Za-z][A-Za-z0-9-]*)\s*::=\s*(.*)$)");
    std::regex applicationTag(R"(\[APPLICATION\s+([0-9]+)\])");
    std::regex contextTag(R"(\[([0-9]+)\])");
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find("--");
        if (comment != std::string::npos) line.resize(comment);
        line = trim(line);
        if (line.empty()) continue;
        if (moduleName_.empty()) {
            std::smatch module;
            if (std::regex_search(line, module, std::regex(R"(^([A-Za-z][A-Za-z0-9-]*)\s+DEFINITIONS)"))) moduleName_ = module[1].str();
        }
        if (pending.empty() && line.find("::=") == std::string::npos) continue;
        pending += (pending.empty() ? "" : " ") + line;
        const bool startsDefinition = line.find("::=") != std::string::npos;
        const bool closesDefinition = line.find('}') != std::string::npos;
        const bool startsStructured = startsDefinition &&
            (line.find("SEQUENCE") != std::string::npos || line.find("SET") != std::string::npos ||
             line.find("CHOICE") != std::string::npos) &&
            line.find(" OF") == std::string::npos;
        const bool hasOpeningBrace = line.find('{') != std::string::npos;
        if (!closesDefinition && (!startsDefinition || startsStructured || hasOpeningBrace)) continue;
        std::smatch match;
        if (!std::regex_search(pending, match, definition)) { pending.clear(); continue; }
        SchemaDefinition def;
        def.name = match[1].str(); def.sourceLine = lineNumber;
        const auto rhs = match[2].str();
        const auto typeHead = trim(rhs.substr(0, rhs.find('{')));
        static const std::vector<std::string> kinds = {"SEQUENCE OF", "SET OF", "SEQUENCE", "SET", "CHOICE", "ENUMERATED", "INTEGER", "OCTET STRING", "BIT STRING", "BOOLEAN", "NULL"};
        for (const auto& kind : kinds) if (typeHead.rfind(kind, 0) == 0) { def.kind = kind; break; }
        if (def.kind == "INTEGER" || def.kind == "OCTET STRING" || def.kind == "BIT STRING" ||
            def.kind == "BOOLEAN" || def.kind == "NULL") def.baseType = def.kind;
        if (def.kind.empty()) {
            std::smatch alias;
            if (std::regex_search(typeHead, alias, std::regex(R"(^\s*([A-Za-z][A-Za-z0-9-]*))"))) def.baseType = alias[1].str();
        }
        if (def.kind == "SEQUENCE OF" || def.kind == "SET OF") {
            std::smatch element;
            if (std::regex_search(typeHead, element, std::regex(R"(OF\s+([A-Za-z][A-Za-z0-9-]*))"))) def.baseType = element[1].str();
        }
        std::smatch tag;
        if (std::regex_search(typeHead, tag, applicationTag)) { def.tagClass = TagClass::Application; def.tagNumber = std::stoull(tag[1].str()); }
        else if (std::regex_search(typeHead, tag, contextTag)) { def.tagClass = TagClass::ContextSpecific; def.tagNumber = std::stoull(tag[1].str()); }
        if (def.tagClass) {
            const auto closeTag = typeHead.find(']');
            if (closeTag != std::string::npos) {
                const auto typeText = trim(typeHead.substr(closeTag + 1));
                if (!typeText.empty()) {
                    std::smatch typeMatch;
                    if (std::regex_search(typeText, typeMatch, std::regex(R"(^([A-Za-z][A-Za-z0-9-]*))")))
                        def.baseType = typeMatch[1].str();
                }
            }
        }
        std::string body = pending;
        const auto open = body.find('{'); const auto close = body.rfind('}');
        if (open != std::string::npos && close != std::string::npos && close > open) {
            const auto bodyText = body.substr(open + 1, close - open - 1);
            if (def.kind == "SEQUENCE" || def.kind == "SET" || def.kind == "CHOICE") {
                for (const auto& member : splitMembers(bodyText)) {
                    if (const auto parsed = parseField(member)) {
                        def.fields.push_back(parsed->name);
                        def.fieldDefinitions.push_back(*parsed);
                    }
                }
            } else if (def.kind == "ENUMERATED" || def.kind == "INTEGER" || def.kind == "BIT STRING") {
                parseNamedValues(bodyText, def.namedValues);
            }
        }
        definitions_[def.name] = std::move(def);
        pending.clear();
    }
    // Primitive named-value types may place the opening brace on the line
    // after the ::= declaration. Recover those blocks in a small second pass
    // without changing the streaming definition parser above.
    std::ifstream namedInput(path);
    std::ostringstream namedSource;
    while (std::getline(namedInput, line)) {
        const auto comment = line.find("--");
        if (comment != std::string::npos) line.resize(comment);
        namedSource << line << '\n';
    }
    const std::regex namedDefinition(
        R"(([A-Za-z][A-Za-z0-9-]*)\s*::=\s*(?:ENUMERATED|INTEGER|BIT STRING)\s*\{([\s\S]*?)\})");
    const auto namedText = namedSource.str();
    for (auto it = std::sregex_iterator(namedText.begin(), namedText.end(), namedDefinition);
         it != std::sregex_iterator(); ++it) {
        auto namedDefinitionIt = definitions_.find((*it)[1].str());
        if (namedDefinitionIt != definitions_.end()) parseNamedValues((*it)[2].str(), namedDefinitionIt->second.namedValues);
    }
    return true;
}

const SchemaDefinition* SchemaRegistry::find(const std::string& name) const {
    const auto it = definitions_.find(name);
    return it == definitions_.end() ? nullptr : &it->second;
}

const SchemaDefinition::Field* SchemaRegistry::resolveField(const std::string& parentType,
                                                             TagClass tagClass, std::uint64_t tagNumber) const {
    const auto* definition = find(parentType);
    if (!definition) return nullptr;
    for (const auto& field : definition->fieldDefinitions) {
        if (field.tagClass && field.tagNumber && *field.tagClass == tagClass && *field.tagNumber == tagNumber) return &field;
        const auto* referenced = find(field.typeName);
        if (!field.tagClass && referenced && referenced->tagClass && referenced->tagNumber &&
            *referenced->tagClass == tagClass && *referenced->tagNumber == tagNumber) return &field;
    }
    return nullptr;
}

std::string SchemaRegistry::resolveStorageType(const std::string& semanticType) const {
    std::string current = semanticType;
    for (int depth = 0; depth < 32; ++depth) {
        const auto* definition = find(current);
        if (!definition || definition->baseType.empty() || definition->baseType == current) return current;
        current = definition->baseType;
    }
    return current;
}

const std::map<std::int64_t, std::string>* SchemaRegistry::namedValues(const std::string& typeName) const {
    const auto* definition = find(typeName);
    return definition && !definition->namedValues.empty() ? &definition->namedValues : nullptr;
}

const std::string& SchemaRegistry::moduleName() const noexcept { return moduleName_; }

std::vector<std::string> SchemaRegistry::names() const {
    std::vector<std::string> result;
    for (const auto& entry : definitions_) result.push_back(entry.first);
    return result;
}

void annotateWithSchema(const std::vector<std::shared_ptr<Asn1Node>>& nodes,
                        const SchemaRegistry& registry, const std::string& rootType) {
    const auto* root = registry.find(rootType);
    std::unordered_map<std::uint64_t, const SchemaDefinition*> applicationTags;
    std::unordered_map<std::uint64_t, bool> duplicateTags;
    for (const auto& name : registry.names()) {
        const auto* definition = registry.find(name);
        if (definition && definition->tagClass && *definition->tagClass == TagClass::Application) {
            const auto tag = definition->tagNumber.value();
            if (applicationTags.count(tag)) duplicateTags[tag] = true;
            else applicationTags[tag] = definition;
        }
    }
    std::function<void(const std::shared_ptr<Asn1Node>&)> globalVisit = [&](const auto& node) {
        if (node->tagClass == TagClass::Application && applicationTags.count(node->tagNumber) && !duplicateTags[node->tagNumber]) {
            const auto* definition = applicationTags[node->tagNumber];
            node->displayName = definition->name;
            node->schemaType = definition->baseType.empty() ? definition->name : definition->baseType;
            node->storageType = registry.resolveStorageType(node->schemaType);
        }
        for (const auto& child : node->children) globalVisit(child);
    };
    for (const auto& node : nodes) globalVisit(node);
    if (root && !nodes.empty()) {
        const auto observedClass = nodes.front()->tagClass;
        const auto observedTag = nodes.front()->tagNumber;
        for (const auto& node : nodes) {
            const bool matches = root->tagClass
                ? node->tagClass == *root->tagClass && node->tagNumber == *root->tagNumber
                : node->tagClass == observedClass && node->tagNumber == observedTag;
            if (matches) {
                node->displayName = root->name;
                node->schemaType = root->baseType.empty() ? root->name : root->baseType;
                node->storageType = registry.resolveStorageType(node->schemaType);
            }
        }
    }

    std::function<void(const std::shared_ptr<Asn1Node>&, const std::string&)> fieldVisit = [&](const auto& node, const auto& typeName) {
        node->parentType = typeName;
        const auto* definition = registry.find(typeName);
        if (!definition) return;
        for (const auto& child : node->children) {
            const auto* field = registry.resolveField(typeName, child->tagClass, child->tagNumber);
            if (field) {
                child->fieldName = field->name;
                child->displayName = field->name;
                child->schemaType = field->typeName;
                child->storageType = registry.resolveStorageType(field->typeName);
                fieldVisit(child, field->typeName);
            } else if (!child->schemaType.empty()) {
                fieldVisit(child, child->schemaType);
            }
        }
    };
    for (const auto& node : nodes) fieldVisit(node, node->schemaType);
}

static std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

static std::string integerValue(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > 8) return {};
    std::int64_t value = (bytes[0] & 0x80) ? -1 : 0;
    for (const auto byte : bytes) value = (value << 8) | byte;
    return std::to_string(value);
}

static char digit(std::uint8_t nibble) {
    if (nibble <= 9) return static_cast<char>('0' + nibble);
    if (nibble == 0x0A) return '*';
    if (nibble == 0x0B) return '#';
    if (nibble == 0x0C) return 'a';
    if (nibble == 0x0D) return 'b';
    if (nibble == 0x0E) return 'c';
    return '\0';
}

static std::string tbcdValue(const std::vector<std::uint8_t>& bytes) {
    std::string value;
    for (const auto byte : bytes) {
        const auto low = byte & 0x0F;
        const auto high = (byte >> 4) & 0x0F;
        const auto first = digit(static_cast<std::uint8_t>(low));
        const auto second = digit(static_cast<std::uint8_t>(high));
        if (!first || (high != 0x0F && !second)) return {};
        value += first;
        if (high != 0x0F) value += second;
    }
    return value;
}

static std::string bcdValue(const std::vector<std::uint8_t>& bytes) {
    std::string value;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto byte = bytes[index];
        const auto first = digit(static_cast<std::uint8_t>((byte >> 4) & 0x0F));
        const auto secondNibble = byte & 0x0F;
        if (!first || (secondNibble == 0x0F && index != bytes.size() - 1)) return {};
        if (secondNibble == 0x0F) return value + first;
        const auto second = digit(static_cast<std::uint8_t>(secondNibble));
        if (!second) return {};
        value += first;
        value += second;
    }
    return value;
}

static std::string compactHex(const std::vector<std::uint8_t>& bytes) {
    std::string value = "0x";
    const char* digits = "0123456789ABCDEF";
    for (const auto byte : bytes) { value += digits[byte >> 4]; value += digits[byte & 0x0F]; }
    return value;
}

static std::string addressValue(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 2) return {};
    const auto nature = (bytes[0] >> 4) & 0x07;
    const auto digits = tbcdValue(std::vector<std::uint8_t>(bytes.begin() + 1, bytes.end()));
    if (digits.empty()) return {};
    return nature == 1 ? "+" + digits : digits;
}

static std::string timestampValue(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != 9) return {};
    std::vector<std::uint8_t> dateDigits(bytes.begin(), bytes.begin() + 6);
    const auto date = bcdValue(dateDigits);
    if (date.size() != 12 || (bytes[6] != '+' && bytes[6] != '-')) return "Invalid TimeStamp";
    const auto toInt = [](const std::string& s) { return std::stoi(s); };
    try {
        const auto year = toInt(date.substr(0, 2));
        const auto yearText = (year >= 50 ? "19" : "20") + date.substr(0, 2);
        const auto month = toInt(date.substr(2, 2));
        const auto day = toInt(date.substr(4, 2));
        const auto hour = toInt(date.substr(6, 2));
        const auto minute = toInt(date.substr(8, 2));
        const auto second = toInt(date.substr(10, 2));
        const auto offset = bcdValue({bytes[7], bytes[8]});
        if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59 || offset.size() != 4)
            return "Invalid TimeStamp";
        return yearText + "-" + date.substr(2, 2) + "-" + date.substr(4, 2) + " " +
               date.substr(6, 2) + ":" + date.substr(8, 2) + ":" + date.substr(10, 2) +
               " " + std::string(1, static_cast<char>(bytes[6])) + offset.substr(0, 2) + ":" + offset.substr(2, 2);
    } catch (...) { return "Invalid TimeStamp"; }
}

static std::string ipv4Value(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != 4) return {};
    return std::to_string(bytes[0]) + "." + std::to_string(bytes[1]) + "." + std::to_string(bytes[2]) + "." + std::to_string(bytes[3]);
}

static std::string schemaValue(const std::string& semanticType, const std::string& storageType,
                               const SchemaRegistry& registry, const std::vector<std::uint8_t>& bytes,
                               std::string& normalizer) {
    const auto kind = upper(semanticType);
    const auto storage = upper(storageType);
    if (const auto* named = registry.namedValues(semanticType); named && !named->empty()) {
        const auto value = integerValue(bytes);
        if (!value.empty()) {
            const auto numeric = std::stoll(value);
            const auto found = named->find(numeric);
            normalizer = "ENUMERATED / named INTEGER";
            return found == named->end() ? "Unknown enum value (" + value + ")" : found->second + " (" + value + ")";
        }
    }
    if (kind.find("TIMESTAMP") != std::string::npos || kind == "TIMESTAMP" || kind == "TIME" || kind == "DATETIME") {
        const auto value = timestampValue(bytes);
        if (!value.empty()) { normalizer = "timestamp"; return value; }
    }
    if (kind.find("ADDRESSSTRING") != std::string::npos || kind.find("DIRECTORYNUMBER") != std::string::npos ||
        kind.find("MSISDN") != std::string::npos || kind.find("CALLINGNUMBER") != std::string::npos || kind.find("CALLEDNUMBER") != std::string::npos) {
        const auto value = addressValue(bytes);
        if (!value.empty()) { normalizer = "address/TBCD"; return value; }
    }
    if (kind.find("IMSI") != std::string::npos || kind.find("IMEI") != std::string::npos || kind.find("TBCD") != std::string::npos) {
        const auto value = tbcdValue(bytes);
        if (!value.empty()) { normalizer = "TBCD-STRING"; return value; }
    }
    if (kind.find("BCDSTRING") != std::string::npos) {
        const auto value = bcdValue(bytes);
        if (!value.empty()) { normalizer = "BCDString (high nibble first)"; return value; }
    }
    if (kind.find("IP") != std::string::npos || kind.find("ADDRESS") != std::string::npos && bytes.size() == 4) {
        const auto value = ipv4Value(bytes);
        if (!value.empty()) { normalizer = "IPv4"; return value; }
    }
    if (kind.find("INTEGER") != std::string::npos || storage == "INTEGER" || kind.find("CELLID") != std::string::npos ||
        kind.find("LOCATIONAREA") != std::string::npos || kind.find("DURATION") != std::string::npos || kind.find("CHARGINGID") != std::string::npos) {
        const auto value = integerValue(bytes);
        if (!value.empty()) { normalizer = "ASN.1 INTEGER"; return value; }
    }
    if (kind.find("ASCII") != std::string::npos || kind.find("NUMBERSTRING") != std::string::npos ||
        kind.find("UTF8") != std::string::npos || kind.find("PRINTABLE") != std::string::npos ||
        kind.find("IA5") != std::string::npos || kind.find("VISIBLE") != std::string::npos || kind.find("STRING") != std::string::npos) {
        if (std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t c) { return c >= 0x20 && c <= 0x7E; })) {
            normalizer = "text";
            return std::string(bytes.begin(), bytes.end());
        }
    }
    if (kind.find("HEXSTRING") != std::string::npos || kind.find("CHARGINGCHARACTERISTICS") != std::string::npos) {
        normalizer = "hex semantic value";
        return compactHex(bytes);
    }
    return {};
}

void applySchemaFormatting(const std::vector<std::shared_ptr<Asn1Node>>& nodes,
                           const BinaryReader& reader, const SchemaRegistry& registry) {
    std::function<void(const std::shared_ptr<Asn1Node>&)> visit = [&](const auto& node) {
        if (!node->constructed && !node->schemaType.empty() && reader.canRead(node->contentRange.offset, node->contentRange.length)) {
            const auto storageType = registry.resolveStorageType(node->schemaType);
            node->storageType = storageType;
            std::string normalizer;
            const auto value = schemaValue(node->schemaType, storageType, registry, reader.copy(node->contentRange), normalizer);
            if (!value.empty()) {
                node->normalizedValue = value;
                node->decodedValue = value;
                node->normalizer = normalizer;
            }
        }
        for (const auto& child : node->children) visit(child);
    };
    for (const auto& node : nodes) visit(node);
}

SchemaMatch detectSchema(const std::vector<std::shared_ptr<Asn1Node>>& roots) {
    SchemaMatch result;
    if (roots.empty()) { result.reason = "no decoded root object"; return result; }
    const auto& root = roots.front();
    if (root->tagClass == TagClass::ContextSpecific) {
        if (root->tagNumber == 20) { result = {true, "HUAWEI_R9.asn", "SGSNPDPRecord", "exact", "root context tag 20 maps to SGSNPDPRecord"}; return result; }
        if (root->tagNumber == 78) { result = {true, "HUAWEI_R9.asn", "SGWRecord", "exact", "root context tag 78 maps to SGWRecord"}; return result; }
        if (root->tagNumber == 79) { result = {true, "HUAWEI_R9.asn", "PGWRecord", "exact", "root context tag 79 maps to PGWRecord"}; return result; }
    }
    if (root->tagClass == TagClass::Application && root->tagNumber == 1) {
        bool tapSignature = false;
        bool nrtrdeSignature = false;
        for (const auto& child : root->children) {
            if (child->tagClass != TagClass::Application) continue;
            tapSignature = tapSignature || child->tagNumber == 4 || child->tagNumber == 5 || child->tagNumber == 6 || child->tagNumber == 196;
            nrtrdeSignature = nrtrdeSignature || child->tagNumber == 2 || child->tagNumber == 3;
        }
        if (tapSignature) { result = {true, "TAP-0312.asn", "TransferBatch", "exact", "application tag 1 with TAP batch-control/network tags"}; return result; }
        if (nrtrdeSignature) { result = {true, "nrtrde_v2.asn", "Nrtrde", "exact", "application tag 1 with NRTRDE v2 event tags"}; return result; }
    }
    if (root->tagClass == TagClass::Universal && root->tagNumber == 16 && root->children.size() >= 2 &&
        root->children[0]->tagClass == TagClass::ContextSpecific && root->children[0]->tagNumber == 0 &&
        root->children[1]->tagClass == TagClass::ContextSpecific && root->children[1]->tagNumber == 1) {
        result.specification = "CS_CDR.asn / huawei-cdr.asn";
        result.rootType = "CallEventDataFile";
        result.confidence = "ambiguous";
        result.reason = "both supplied CDR specifications share this root and observed record tags";
        return result;
    }
    result.reason = "no supplied specification has a unique structural signature";
    return result;
}

} // namespace asn1viewer
