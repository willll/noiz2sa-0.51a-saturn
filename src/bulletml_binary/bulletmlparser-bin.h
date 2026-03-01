/// BulletML Binary Parser
/**
 * Fast binary parser for BulletML files.
 * Reads .bmlb (BulletML Binary) files instead of XML.
 * See BINARY_FORMAT.md for format specification.
 */

#ifndef BULLETMLPARSER_BIN_H_
#define BULLETMLPARSER_BIN_H_

#include "../bulletml/bulletmlparser.h"
#include <vector>
#include <string>
#include <cstdint>

/// Binary format header structure
struct BulletMLBinaryHeader {
    char magic[4];           // "BMLB"
    uint16_t version;        // Format version (currently 1)
    uint8_t orientation;     // 0=vertical, 1=horizontal
    uint8_t flags;           // Reserved flags
    uint32_t string_table_offset;
    uint32_t refmap_offset;
    uint32_t tree_offset;
    uint32_t file_size;
};

/// Reference entry in reference maps
struct BulletMLRefEntry {
    uint32_t label_id;       // Index into string table
    uint32_t node_id;        // Node ID in tree
};

/// Node header in binary format
struct BulletMLNodeHeader {
    uint8_t node_type;       // BulletMLNode::Name
    uint8_t value_type;      // BulletMLNode::Type
    uint16_t child_count;    // Number of children
    uint32_t ref_id;         // Reference ID (0xFFFFFFFF if none)
    uint32_t value_string_id; // String table index (0xFFFFFFFF if none)
    uint32_t label_string_id; // String table index (0xFFFFFFFF if none)
};

/// Binary BulletML Parser
class BulletMLParserBin : public BulletMLParser {
public:
    /// Constructor from file path
    DECLSPEC explicit BulletMLParserBin(const std::string& filename);
    
    /// Constructor from memory buffer
    DECLSPEC BulletMLParserBin(const uint8_t* data, size_t size);
    
    DECLSPEC virtual ~BulletMLParserBin();

    /// Parse the binary file (called by build())
    DECLSPEC virtual void parse();
    
    /// Get last error message
    DECLSPEC const std::string& getErrorMessage() const { return error_message_; }

private:
    /// Read primitives from buffer
    uint8_t readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    
    /// Read string from current position
    std::string readString();
    
    /// Verify header magic and version
    bool verifyHeader();
    
    /// Load string table
    bool loadStringTable();
    
    /// Load reference maps
    bool loadReferenceMaps();
    
    /// Parse node tree recursively
    BulletMLNode* parseNode();
    
    /// Convert binary node type to BulletMLNode::Name
    static BulletMLNode::Name convertNodeType(uint8_t type);
    
    /// Convert binary value type to BulletMLNode::Type
    static BulletMLNode::Type convertValueType(uint8_t type);
    
    /// Convert BulletMLNode::Name to string
    static std::string nodeTypeToString(uint8_t type);

private:
    const uint8_t* data_;        // Pointer to binary data
    size_t data_size_;           // Size of data
    size_t offset_;              // Current read offset
    bool owns_data_;             // True if we allocated data_
    
    BulletMLBinaryHeader header_;
    std::vector<std::string> string_table_;
    
    // Reference maps: label -> node_id
    std::vector<BulletMLRefEntry> bullet_refs_;
    std::vector<BulletMLRefEntry> action_refs_;
    std::vector<BulletMLRefEntry> fire_refs_;
    
    std::string error_message_;
    
    // Node ID counter for tracking nodes during parsing
    uint32_t current_node_id_;
    
    // Map of node_id to BulletMLNode* for resolving references
    std::vector<BulletMLNode*> node_map_;
};

#endif // BULLETMLPARSER_BIN_H_
