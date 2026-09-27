#include "asn1viewer/ber.hpp"
#include "asn1viewer/schema.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace asn1viewer;

static std::shared_ptr<Asn1Node> one(std::initializer_list<std::uint8_t> bytes) {
    BinaryReader reader{std::vector<std::uint8_t>(bytes)};
    auto nodes = BerDecoder(reader).decodeAll();
    assert(nodes.size() == 1);
    return nodes.front();
}

static std::filesystem::path projectFile(const char* name) {
    const auto current = std::filesystem::current_path();
    for (const auto& candidate : {current / name, current / "specs" / name,
                                  current.parent_path() / name, current.parent_path() / "specs" / name,
                                  current.parent_path().parent_path() / name,
                                  current.parent_path().parent_path() / "specs" / name})
        if (std::filesystem::exists(candidate)) return candidate;
    return {};
}

int main() {
    {
        auto node = one({0x02, 0x03, 0x00, 0x01, 0xA4});
        assert(node->tagNumber == 2 && node->encodedRange.offset == 0 && node->encodedRange.length == 5);
        assert(node->decodedValue == "420");
        assert(node->rawHex == "02 03 00 01 A4");
    }
    {
        auto node = one({0x30, 0x06, 0x02, 0x01, 0x2A, 0x01, 0x01, 0xFF});
        assert(node->constructed && node->children.size() == 2);
        assert(node->children[0]->decodedValue == "42");
        assert(node->children[1]->decodedValue == "true");
    }
    {
        auto node = one({0x9F, 0x81, 0x01, 0x01, 0x7F});
        assert(node->tagClass == TagClass::ContextSpecific && node->tagNumber == 129);
        assert(node->rawHex == "9F 81 01 01 7F");
    }
    {
        auto node = one({0x30, 0x80, 0x02, 0x01, 0x01, 0x00, 0x00});
        assert(node->constructed && node->children.size() == 1);
        assert(node->encodedRange.length == 7 && node->contentRange.length == 3);
    }
    {
        auto node = one({0x02, 0x02, 0x01});
        assert(node->status == DecodeStatus::Malformed);
    }
    {
        const auto schemaPath = projectFile("HUAWEI_R9.asn");
        assert(!schemaPath.empty());
        SchemaRegistry registry;
        std::string error;
        assert(registry.loadFile(schemaPath.string(), error));
        auto root = one({0xBF, 0x4F, 0x12,
                         0x83, 0x08, 0x34, 0x12, 0x01, 0x21, 0x43, 0x65, 0x87, 0xF9,
                         0x97, 0x02, 0x04, 0x00,
                         0x98, 0x01, 0x03});
        annotateWithSchema({root}, registry, "PGWRecord");
        BinaryReader reader({0xBF, 0x4F, 0x12,
                             0x83, 0x08, 0x34, 0x12, 0x01, 0x21, 0x43, 0x65, 0x87, 0xF9,
                             0x97, 0x02, 0x04, 0x00,
                             0x98, 0x01, 0x03});
        applySchemaFormatting({root}, reader, registry);
        assert(root->displayName == "PGWRecord");
        assert(root->children.size() == 3);
        assert(root->children[0]->fieldName == "servedIMSI");
        assert(root->children[0]->schemaType == "IMSI");
        assert(root->children[0]->normalizedValue == "432110123456789");
        assert(root->children[1]->fieldName == "chargingCharacteristics");
        assert(root->children[1]->normalizedValue == "0x0400");
        assert(root->children[2]->fieldName == "chChSelectionMode");
        assert(root->children[2]->normalizedValue == "homeDefault (3)");
        auto second = one({0xBF, 0x4F, 0x04, 0x97, 0x02, 0x04, 0x00});
        annotateWithSchema({root, second}, registry, "PGWRecord");
        assert(second->displayName == "PGWRecord");
        assert(second->children[0]->fieldName == "chargingCharacteristics");
    }
    {
        auto root = one({0xBF, 0x4F, 0x04, 0x97, 0x02, 0x04, 0x00});
        const auto match = detectSchema({root});
        assert(match.matched && match.specification == "HUAWEI_R9.asn" && match.rootType == "PGWRecord");
    }
    {
        for (const auto* file : {"CS_CDR.asn", "huawei-cdr.asn", "HUAWEI_R9.asn",
                                 "nrtrde.asn", "nrtrde_v2.asn", "TAP-0312.asn"}) {
            const auto path = projectFile(file);
            assert(!path.empty());
            SchemaRegistry registry;
            std::string error;
            assert(registry.loadFile(path.string(), error));
            assert(!registry.names().empty());
        }
        const auto path = projectFile("HUAWEI_R9.asn");
        SchemaRegistry huawei;
        std::string error;
        assert(huawei.loadFile(path.string(), error));
        const auto* charging = huawei.resolveField("PGWRecord", TagClass::ContextSpecific, 23);
        assert(charging && charging->name == "chargingCharacteristics" && charging->typeName == "ChargingCharacteristics");
        const auto tapPath = projectFile("TAP-0312.asn");
        SchemaRegistry tap;
        assert(tap.loadFile(tapPath.string(), error));
        const auto* transferBatch = tap.find("TransferBatch");
        assert(transferBatch && transferBatch->tagClass && *transferBatch->tagClass == TagClass::Application &&
               transferBatch->tagNumber && *transferBatch->tagNumber == 1);
    }
    std::cout << "decoder tests passed\n";
}
