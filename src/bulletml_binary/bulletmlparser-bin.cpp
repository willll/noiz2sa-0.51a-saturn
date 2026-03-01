#include "bulletmlparser-bin.h"
#include "../bulletml/bulletmlerror.h"
#include <fstream>
#include <cstring>
#include <algorithm>

// Helper to safely check buffer bounds
#define CHECK_BOUNDS(size) \
    if (offset_ + (size) > data_size_) { \
        error_message_ = "Unexpected end of file"; \
        return false; \
    }

#define CHECK_BOUNDS_PTR(size) \
    if (offset_ + (size) > data_size_) { \
        error_message_ = "Unexpected end of file"; \
        return nullptr; \
    }

BulletMLParserBin::BulletMLParserBin(const std::string& filename)
    : data_(nullptr), data_size_(0), offset_(0), owns_data_(true),
      current_node_id_(0)
{
    setName(filename);
    
    // Read entire file into memory
    std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        error_message_ = "Failed to open file: " + filename;
        return;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    uint8_t* buffer = new uint8_t[size];
    if (!file.read(reinterpret_cast<char*>(buffer), size)) {
        error_message_ = "Failed to read file: " + filename;
        delete[] buffer;
        return;
    }
    
    data_ = buffer;
    data_size_ = static_cast<size_t>(size);
}

BulletMLParserBin::BulletMLParserBin(const uint8_t* data, size_t size)
    : data_(data), data_size_(size), offset_(0), owns_data_(false),
      current_node_id_(0)
{
    setName("(memory)");
}

BulletMLParserBin::~BulletMLParserBin() {
    if (owns_data_ && data_) {
        delete[] data_;
    }
}

uint8_t BulletMLParserBin::readUInt8() {
    if (offset_ + 1 > data_size_) {
        return 0;
    }
    return data_[offset_++];
}

uint16_t BulletMLParserBin::readUInt16() {
    if (offset_ + 2 > data_size_) {
        return 0;
    }
    // Little-endian
    uint16_t value = data_[offset_] | (data_[offset_ + 1] << 8);
    offset_ += 2;
    return value;
}

uint32_t BulletMLParserBin::readUInt32() {
    if (offset_ + 4 > data_size_) {
        return 0;
    }
    // Little-endian
    uint32_t value = data_[offset_] | 
                     (data_[offset_ + 1] << 8) |
                     (data_[offset_ + 2] << 16) |
                     (data_[offset_ + 3] << 24);
    offset_ += 4;
    return value;
}

std::string BulletMLParserBin::readString() {
    uint16_t length = readUInt16();
    if (offset_ + length > data_size_) {
        return "";
    }
    
    std::string str(reinterpret_cast<const char*>(data_ + offset_), length);
    offset_ += length;
    return str;
}

bool BulletMLParserBin::verifyHeader() {
    CHECK_BOUNDS(sizeof(BulletMLBinaryHeader));
    
    // Read header
    memcpy(&header_, data_ + offset_, sizeof(BulletMLBinaryHeader));
    offset_ += sizeof(BulletMLBinaryHeader);
    
    // Verify magic number
    if (memcmp(header_.magic, "BMLB", 4) != 0) {
        error_message_ = "Invalid magic number (not a BulletML binary file)";
        return false;
    }
    
    // Check version
    if (header_.version != 1) {
        error_message_ = "Unsupported version";
        return false;
    }
    
    // Verify file size
    if (header_.file_size != data_size_) {
        error_message_ = "File size mismatch";
        return false;
    }
    
    // Set orientation
    if (header_.orientation == 1) {
        setHorizontal();
    }
    
    return true;
}

bool BulletMLParserBin::loadStringTable() {
    CHECK_BOUNDS(4);
    
    offset_ = header_.string_table_offset;
    uint32_t string_count = readUInt32();
    
    string_table_.reserve(string_count);
    for (uint32_t i = 0; i < string_count; ++i) {
        string_table_.push_back(readString());
    }
    
    return true;
}

bool BulletMLParserBin::loadReferenceMaps() {
    offset_ = header_.refmap_offset;
    
    // Load bullet references
    CHECK_BOUNDS(4);
    uint32_t bullet_count = readUInt32();
    bullet_refs_.reserve(bullet_count);
    for (uint32_t i = 0; i < bullet_count; ++i) {
        CHECK_BOUNDS(8);
        BulletMLRefEntry entry;
        entry.label_id = readUInt32();
        entry.node_id = readUInt32();
        bullet_refs_.push_back(entry);
    }
    
    // Load action references
    CHECK_BOUNDS(4);
    uint32_t action_count = readUInt32();
    action_refs_.reserve(action_count);
    for (uint32_t i = 0; i < action_count; ++i) {
        CHECK_BOUNDS(8);
        BulletMLRefEntry entry;
        entry.label_id = readUInt32();
        entry.node_id = readUInt32();
        action_refs_.push_back(entry);
    }
    
    // Load fire references
    CHECK_BOUNDS(4);
    uint32_t fire_count = readUInt32();
    fire_refs_.reserve(fire_count);
    for (uint32_t i = 0; i < fire_count; ++i) {
        CHECK_BOUNDS(8);
        BulletMLRefEntry entry;
        entry.label_id = readUInt32();
        entry.node_id = readUInt32();
        fire_refs_.push_back(entry);
    }
    
    return true;
}

BulletMLNode::Name BulletMLParserBin::convertNodeType(uint8_t type) {
    return static_cast<BulletMLNode::Name>(type);
}

BulletMLNode::Type BulletMLParserBin::convertValueType(uint8_t type) {
    return static_cast<BulletMLNode::Type>(type);
}

std::string BulletMLParserBin::nodeTypeToString(uint8_t type) {
    static const char* names[] = {
        "bulletml", "bullet", "action", "fire", "changeDirection", "changeSpeed",
        "accel", "wait", "repeat", "bulletRef", "actionRef", "fireRef",
        "vanish", "horizontal", "vertical", "term", "times", "direction",
        "speed", "param"
    };
    
    if (type < 20) {
        return names[type];
    }
    return "unknown";
}

BulletMLNode* BulletMLParserBin::parseNode() {
    CHECK_BOUNDS_PTR(16);
    
    // Read node header
    BulletMLNodeHeader node_header;
    node_header.node_type = readUInt8();
    node_header.value_type = readUInt8();
    node_header.child_count = readUInt16();
    node_header.ref_id = readUInt32();
    node_header.value_string_id = readUInt32();
    node_header.label_string_id = readUInt32();
    
    // Create node
    std::string node_name = nodeTypeToString(node_header.node_type);
    BulletMLNode* node = new BulletMLNode(node_name);
    
    // Set node ID for reference mapping
    uint32_t this_node_id = current_node_id_++;
    if (this_node_id >= node_map_.size()) {
        node_map_.resize(this_node_id + 1);
    }
    node_map_[this_node_id] = node;
    
    // Set type if present
    if (node_header.value_type != 0) {
        static const char* type_names[] = { "", "aim", "absolute", "relative", "sequence" };
        if (node_header.value_type < 5) {
            node->setType(type_names[node_header.value_type]);
        }
    }
    
    // Set ref ID if present
    if (node_header.ref_id != 0xFFFFFFFF) {
        node->setRefID(node_header.ref_id);
    }
    
    // Set value if present
    if (node_header.value_string_id != 0xFFFFFFFF) {
        if (node_header.value_string_id < string_table_.size()) {
            node->setValue(string_table_[node_header.value_string_id]);
        }
    }
    
    // Note: label is handled separately in reference maps
    
    // Parse children
    for (uint16_t i = 0; i < node_header.child_count; ++i) {
        BulletMLNode* child = parseNode();
        if (!child) {
            delete node;
            return nullptr;
        }
        node->addChild(child);
    }
    
    return node;
}

void BulletMLParserBin::parse() {
    if (!data_ || data_size_ == 0) {
        BulletMLError::doAssert(false, "No data to parse");
        return;
    }
    
    offset_ = 0;
    current_node_id_ = 0;
    node_map_.clear();
    
    // Verify header
    if (!verifyHeader()) {
        BulletMLError::doAssert(false, "Header verification failed: " + error_message_);
        return;
    }
    
    // Load string table
    if (!loadStringTable()) {
        BulletMLError::doAssert(false, "Failed to load string table: " + error_message_);
        return;
    }
    
    // Load reference maps
    if (!loadReferenceMaps()) {
        BulletMLError::doAssert(false, "Failed to load reference maps: " + error_message_);
        return;
    }
    
    // Parse tree
    offset_ = header_.tree_offset;
    bulletml_ = parseNode();
    
    if (!bulletml_) {
        BulletMLError::doAssert(false, "Failed to parse tree: " + error_message_);
        return;
    }
    
    // Build reference maps for BulletMLParser
    // This populates bulletMap_, actionMap_, fireMap_
    
    // Process bullet references
    for (size_t i = 0; i < bullet_refs_.size(); ++i) {
        uint32_t node_id = bullet_refs_[i].node_id;
        if (node_id < node_map_.size() && node_map_[node_id]) {
            if (i >= bulletMap_.size()) {
                bulletMap_.resize(i + 1);
            }
            bulletMap_[i] = node_map_[node_id];
        }
    }
    
    // Process action references
    for (size_t i = 0; i < action_refs_.size(); ++i) {
        uint32_t node_id = action_refs_[i].node_id;
        if (node_id < node_map_.size() && node_map_[node_id]) {
            if (i >= actionMap_.size()) {
                actionMap_.resize(i + 1);
            }
            actionMap_[i] = node_map_[node_id];
        }
    }
    
    // Process fire references
    for (size_t i = 0; i < fire_refs_.size(); ++i) {
        uint32_t node_id = fire_refs_[i].node_id;
        if (node_id < node_map_.size() && node_map_[node_id]) {
            if (i >= fireMap_.size()) {
                fireMap_.resize(i + 1);
            }
            fireMap_[i] = node_map_[node_id];
        }
    }
    
    // Find and populate top actions
    // Top actions are action nodes that are direct children of bulletml root
    if (bulletml_) {
        BulletMLNode::ChildIterator ite;
        for (ite = bulletml_->childBegin(); ite != bulletml_->childEnd(); ++ite) {
            if ((*ite)->getName() == BulletMLNode::action) {
                topActions_.push_back(*ite);
            }
        }
    }
}
