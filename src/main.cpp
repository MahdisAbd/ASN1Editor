#include "asn1viewer/ber.hpp"
#include "asn1viewer/schema.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>

using namespace asn1viewer;

static std::filesystem::path locateSchema(const std::string& filename) {
    const auto current = std::filesystem::current_path();
    for (const auto& candidate : {current / filename, current / "specs" / filename}) {
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

static void printNode(const std::shared_ptr<Asn1Node>& node, unsigned depth) {
    const auto name = !node->fieldName.empty() ? node->fieldName : (!node->displayName.empty() ? node->displayName : "Unknown field");
    std::string tag = "[" + std::to_string(node->tagNumber) + "]";
    if (node->tagClass == TagClass::Application) tag = "[APPLICATION " + std::to_string(node->tagNumber) + "]";
    else if (node->tagClass == TagClass::Universal) tag = "[UNIVERSAL " + std::to_string(node->tagNumber) + "]";
    std::string label = name + " " + tag;
    if (!node->schemaType.empty() && node->schemaType != name) label += " : " + node->schemaType;
    if (!node->constructed) label += " = " + (node->normalizedValue.empty() ? (node->decodedValue.empty() ? "<unknown type>" : node->decodedValue) : node->normalizedValue);
    std::cout << std::string(depth * 2, ' ')
              << label
              << " [" << tagClassName(node->tagClass) << ":" << node->tagNumber
              << (node->constructed ? ", constructed" : "") << "]"
              << " offset=" << node->encodedRange.offset
              << " length=" << node->encodedRange.length;
    if (!node->decodedValue.empty()) std::cout << " value=" << node->decodedValue;
    if (node->status != DecodeStatus::Ok) std::cout << " status=" << statusName(node->status) << " error=" << node->error;
    std::cout << '\n';
    for (const auto& child : node->children) printNode(child, depth + 1);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: asn1viewer decode <file> [--schema file] [--root type]\n";
        return 2;
    }
    const std::string command = argv[1];
    if (command != "decode") { std::cerr << "unknown command: " << command << '\n'; return 2; }
    std::string schemaPath, rootType;
    std::string inputPath;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--schema" && i + 1 < argc) schemaPath = argv[++i];
        else if (arg == "--root" && i + 1 < argc) rootType = argv[++i];
        else if (inputPath.empty()) inputPath = arg;
        else { std::cerr << "unexpected argument: " << arg << '\n'; return 2; }
    }
    if (inputPath.empty()) { std::cerr << "input file is required\n"; return 2; }
    std::ifstream file(inputPath, std::ios::binary);
    if (!file) { std::cerr << "cannot open input: " << inputPath << '\n'; return 1; }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    BinaryReader reader(std::move(bytes));
    BerDecoder decoder(reader);
    auto nodes = decoder.decodeAll();
    if (schemaPath.empty()) {
        const auto match = detectSchema(nodes);
        if (!match.specification.empty()) {
            std::cout << "Specification: " << match.specification << "\n"
                      << "Root type: " << match.rootType << "\n"
                      << "Match: " << match.confidence << "\n"
                      << "Reason: " << match.reason << "\n\n";
        }
        if (match.matched && match.specification.find("/") == std::string::npos) {
            const auto candidate = locateSchema(match.specification);
            SchemaRegistry registry; std::string error;
            if (registry.loadFile(candidate.string(), error)) {
                annotateWithSchema(nodes, registry, match.rootType);
                BinaryReader schemaReader = reader;
                applySchemaFormatting(nodes, schemaReader, registry);
            }
        }
    } else if (!rootType.empty()) {
        SchemaRegistry registry; std::string error;
        if (!registry.loadFile(schemaPath, error)) { std::cerr << error << '\n'; return 1; }
        annotateWithSchema(nodes, registry, rootType);
        BinaryReader schemaReader = reader;
        applySchemaFormatting(nodes, schemaReader, registry);
    }
    for (const auto& node : nodes) printNode(node, 0);
    return 0;
}
