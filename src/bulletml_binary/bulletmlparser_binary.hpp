/// BulletML Binary Parser - Header-Only Implementation
/**
 * Fast binary parser for BulletML files.
 * Reads .blb (BulletML Binary) files instead of XML.
 * 
 * This is a header-only library that can be directly integrated into your project.
 * 
 * Usage:
 *   #include "bulletmlparser_binary.hpp"
 *   
 *   // From file:
 *   BulletMLParserBinary parser("pattern.blb");
 *   if (parser.build()) {
 *       // Use parser
 *   }
 *   
 *   // From memory:
 *   BulletMLParserBinary parser(data_ptr, data_size);
 *   if (parser.build()) {
 *       // Use parser
 *   }
 * 
 * Binary Format:
 *   - Magic: "BLB\x00" (4 bytes)
 *   - Version: 1 (uint16_t)
 *   - See tools/BINARY_FORMAT.md for complete specification
 */

#ifndef BULLETMLPARSER_BINARY_HPP_
#define BULLETMLPARSER_BINARY_HPP_

#include <cstdint>
#include <srl_cd.hpp>
#include <srl_log.hpp>

// Manual size_t definition (stddef.h not available on Saturn)
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

// Platform-specific export declarations
#ifndef DECLSPEC
#define DECLSPEC
#endif

// Maximum fixed sizes for static allocation
#define BULLETML_MAX_CHILDREN 32
#define BULLETML_MAX_FILENAME 256
#define BULLETML_MAX_STRINGS 1000
#define BULLETML_MAX_STRING_TABLE_SIZE 65536
#define BULLETML_MAX_NODES 2000
#define BULLETML_MAX_REFS 500

/// Minimal BulletMLNode definition - embedded for standalone use
class BulletMLNode {
public:
    enum Name {
        bulletml = 0, bullet, action, fire, changeDirection, changeSpeed,
        accel, wait, repeat, bulletRef, actionRef, fireRef,
        vanish, horizontal, vertical, term, times, direction,
        speed, param
    };
    
    enum Type {
        type_none = 0, type_aim, type_absolute, type_relative, type_sequence
    };
    
    explicit BulletMLNode(const char* name) 
        : name_(name), type_(type_none), value_(nullptr), refID_(0xFFFFFFFF), 
          child_count_(0), parent_(nullptr) {
        for (uint32_t i = 0; i < BULLETML_MAX_CHILDREN; ++i) {
            children_[i] = nullptr;
        }
    }
    
    ~BulletMLNode() {
        for (uint32_t i = 0; i < child_count_; ++i) {
            delete children_[i];
        }
    }
    
    void setType(const char* typeStr) {
        if (!typeStr) return;
        // Compare by first character for efficiency
        switch (typeStr[0]) {
            case 'a':
                if (typeStr[1] == 'i' && typeStr[2] == 'm' && typeStr[3] == '\0') {
                    type_ = type_aim;
                } else if (typeStr[1] == 'b' && typeStr[2] == 's') {
                    type_ = type_absolute;
                }
                break;
            case 'r':
                if (typeStr[1] == 'e' && typeStr[2] == 'l') {
                    type_ = type_relative;
                }
                break;
            case 's':
                if (typeStr[1] == 'e' && typeStr[2] == 'q') {
                    type_ = type_sequence;
                }
                break;
        }
    }
    
    void setValue(const char* value) { value_ = value; }
    void setRefID(uint32_t id) { refID_ = id; }
    
    void addChild(BulletMLNode* child) {
        if (child_count_ < BULLETML_MAX_CHILDREN) {
            children_[child_count_++] = child;
            child->parent_ = this;
        }
    }
    
    const char* getName() const { return name_; }
    Name getNameAsName() const {
        // Convert string name to enum
        if (!name_) return bulletml;
        switch (name_[0]) {
            case 'b':
                if (name_[1] == 'u' && name_[2] == 'l') {
                    if (name_[6] == 'm') return bulletml; // bulletml
                    if (name_[6] == 't') return bullet;   // bullet
                    if (name_[6] == 'R') return bulletRef; // bulletRef
                }
                break;
            case 'a':
                if (name_[1] == 'c' && name_[2] == 't') {
                    if (name_[6] == '\0') return action;  // action
                    if (name_[6] == 'R') return actionRef; // actionRef
                } else if (name_[1] == 'c' && name_[2] == 'c') {
                    return accel; // accel
                }
                break;
            case 'f':
                if (name_[1] == 'i' && name_[2] == 'r') {
                    if (name_[4] == '\0') return fire;    // fire
                    if (name_[4] == 'R') return fireRef;   // fireRef
                }
                break;
            case 'c':
                if (name_[7] == 'D') return changeDirection;
                if (name_[7] == 'S') return changeSpeed;
                break;
            case 'w': return wait;
            case 'r': return repeat;
            case 'v': 
                if (name_[1] == 'a') return vanish;
                if (name_[1] == 'e') return vertical;
                break;
            case 'h': return horizontal;
            case 't':
                if (name_[1] == 'e') return term;
                if (name_[1] == 'i') return times;
                break;
            case 'd': return direction;
            case 's':
                if (name_[1] == 'p') return speed;
                break;
            case 'p': return param;
        }
        return bulletml;
    }
    
    Type getType() const { return type_; }
    const char* getValue() const { return value_ ? value_ : ""; }
    uint32_t getRefID() const { return refID_; }
    
    // Iterator support (pointer-based)
    typedef BulletMLNode** ChildIterator;
    ChildIterator childBegin() { return children_; }
    ChildIterator childEnd() { return children_ + child_count_; }
    uint32_t getChildCount() const { return child_count_; }
    BulletMLNode* getChild(uint32_t index) const {
        return index < child_count_ ? children_[index] : nullptr;
    }
    
private:
    const char* name_;
    Type type_;
    const char* value_;
    uint32_t refID_;
    BulletMLNode* children_[BULLETML_MAX_CHILDREN];
    uint32_t child_count_;
    BulletMLNode* parent_;
    
    // Non-copyable
    BulletMLNode(const BulletMLNode&);
    BulletMLNode& operator=(const BulletMLNode&);
};

/// Binary format header structure (24 bytes, little-endian)
struct BulletMLBinaryHeader {
    char magic[4];           // "BLB\x00"
    uint16_t version;        // Format version (currently 1)
    uint8_t orientation;     // 0=vertical, 1=horizontal
    uint8_t flags;           // Reserved flags (currently unused)
    uint32_t string_table_offset;
    uint32_t refmap_offset;
    uint32_t tree_offset;
    uint32_t file_size;
};

/// Reference entry in reference maps (8 bytes)
struct BulletMLRefEntry {
    uint32_t label_id;       // Index into string table
    uint32_t node_id;        // Node ID in tree
};

/// Node header in binary format (16 bytes)
struct BulletMLNodeHeader {
    uint8_t node_type;       // BulletMLNode::Name
    uint8_t value_type;      // BulletMLNode::Type
    uint16_t child_count;    // Number of children
    uint32_t ref_id;         // Reference ID (0xFFFFFFFF if none)
    uint32_t value_string_id; // String table index (0xFFFFFFFF if none)
    uint32_t label_string_id; // String table index (0xFFFFFFFF if none)
};

// Forward declarations for dependency resolution
class BulletMLParserBinary;
class BulletMLState;

/// Type alias for compatibility
typedef BulletMLParserBinary BulletMLParser;

/// BulletML State - Minimal state object for BulletML execution
class BulletMLState {
public:
    explicit BulletMLState(BulletMLParser* parser = nullptr)
        : parser_(parser) {}
    
    virtual ~BulletMLState() {}
    
    BulletMLParser* getParser() const { return parser_; }
    void setParser(BulletMLParser* parser) { parser_ = parser; }

private:
    BulletMLParser* parser_;
};

/// BulletML Runner - Base class for BulletML execution
class BulletMLRunner {
public:
    /// Constructor from parser
    explicit BulletMLRunner(BulletMLParser* parser) 
        : parser_(parser), state_(nullptr) {}
    
    /// Constructor from state
    explicit BulletMLRunner(BulletMLState* state)
        : parser_(nullptr), state_(state) {}
    
    /// Virtual destructor
    virtual ~BulletMLRunner() {}
    
    /// Get bullet direction (degrees)
    virtual double getBulletDirection() { return 0.0; }
    
    /// Get aim direction (degrees)
    virtual double getAimDirection() { return 0.0; }
    
    /// Get bullet speed
    virtual double getBulletSpeed() { return 0.0; }
    
    /// Get default speed
    virtual double getDefaultSpeed() { return 1.0; }
    
    /// Get difficulty rank
    virtual double getRank() { return 0.0; }
    
    /// Create a simple bullet
    virtual void createSimpleBullet(double direction, double speed) { (void)direction; (void)speed; }
    
    /// Create a bullet
    virtual void createBullet(BulletMLNode* node, double direction, double speed) { (void)node; (void)direction; (void)speed; }
    
    /// Add a command
    virtual int addCommand(BulletMLNode* node) { (void)node; return 0; }
    
    /// Execute commands
    virtual void execute() {}
    
    /// Check if execution is finished
    virtual bool isEnd() { return true; }
    
    /// Run one execution cycle
    virtual void run() {}
    
    /// Get the parser
    BulletMLParser* getParser() const { return parser_; }
    
    /// Get the state
    BulletMLState* getState() const { return state_; }

protected:
    BulletMLParser* parser_;
    BulletMLState* state_;
};

/// Binary BulletML Parser - Standalone Implementation
class BulletMLParserBinary {
public:
    /// Constructor from memory buffer (filename for identification)
    DECLSPEC explicit BulletMLParserBinary(const char* filename, const uint8_t* data, uint32_t size)
                : data_(data), data_size_(size), offset_(0), owns_data_(false),
                    bulletml_(nullptr), 
          bulletMap_count_(0), actionMap_count_(0), fireMap_count_(0),
          topActions_count_(0), is_horizontal_(false)
    {
        setName(filename);
        error_message_[0] = '\0';
    }
    
    /// Constructor from memory buffer (unnamed)
    DECLSPEC explicit BulletMLParserBinary(const uint8_t* data, uint32_t size)
                : data_(data), data_size_(size), offset_(0), owns_data_(false),
                    bulletml_(nullptr),
          bulletMap_count_(0), actionMap_count_(0), fireMap_count_(0),
          topActions_count_(0), is_horizontal_(false)
    {
        setName("(memory)");
        error_message_[0] = '\0';
    }
    
    DECLSPEC virtual ~BulletMLParserBinary() {
        if (bulletml_) {
            delete bulletml_;
            bulletml_ = nullptr;
        }
        if (owns_data_ && data_) {
            delete[] data_;
            data_ = nullptr;
            data_size_ = 0;
            owns_data_ = false;
        }
    }
    
    /// Build the parser (calls parse internally)
    /// Returns true on success, false on failure
    DECLSPEC bool build() {
        return parse();
    }
    
    /// Get the root node
    DECLSPEC BulletMLNode* getRoot() const { return bulletml_; }
    
    /// Get bullet by reference ID
    DECLSPEC BulletMLNode* getBullet(uint32_t id) const {
        return id < bulletMap_count_ ? bulletMap_[id] : nullptr;
    }
    
    /// Get action by reference ID
    DECLSPEC BulletMLNode* getAction(uint32_t id) const {
        return id < actionMap_count_ ? actionMap_[id] : nullptr;
    }
    
    /// Get fire by reference ID  
    DECLSPEC BulletMLNode* getFire(uint32_t id) const {
        return id < fireMap_count_ ? fireMap_[id] : nullptr;
    }
    
    /// Get top-level actions
    DECLSPEC BulletMLNode** getTopActions(uint32_t* count) const {
        if (count) *count = topActions_count_;
        return const_cast<BulletMLNode**>(topActions_);
    }
    
    /// Check if pattern is horizontal
    DECLSPEC bool isHorizontal() const { return is_horizontal_; }
    
    /// Get parser name
    DECLSPEC const char* getName() const { return name_; }

    /// Parse the binary file (called by build())
    /// Returns true on success, false on failure
    DECLSPEC virtual bool parse() {
        if (!data_ || data_size_ == 0) {
            SRL::Logger::LogFatal("[BulletML] No data to parse for '%s'", name_);
            return false;
        }
        
        SRL::Logger::LogTrace("[BulletML] Starting parse for '%s', size=%d", name_, data_size_);
        
        offset_ = 0;
        current_node_id_ = 0;
        string_table_count_ = 0;
        bullet_refs_count_ = 0;
        action_refs_count_ = 0;
        fire_refs_count_ = 0;
        // Initialize node map (static array)
        for (uint32_t i = 0; i < BULLETML_MAX_NODES; ++i) {
            node_map_[i] = nullptr;
        }
        
        // Verify header
        SRL::Logger::LogTrace("[BulletML] Verifying header for '%s'", name_);
        if (!verifyHeader()) {
            SRL::Logger::LogFatal("[BulletML] Header verification failed for '%s'", name_);
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] Header verified for '%s'", name_);
        
        // Load string table
        SRL::Logger::LogTrace("[BulletML] Loading string table for '%s'", name_);
        if (!loadStringTable()) {
            SRL::Logger::LogFatal("[BulletML] Failed to load string table for '%s'", name_);
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] String table loaded for '%s'", name_);
        
        // Load reference maps
        SRL::Logger::LogTrace("[BulletML] Loading reference maps for '%s'", name_);
        if (!loadReferenceMaps()) {
            SRL::Logger::LogFatal("[BulletML] Failed to load reference maps for '%s'", name_);
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] Reference maps loaded for '%s'", name_);
        
        // Parse tree
        SRL::Logger::LogTrace("[BulletML] Parsing tree for '%s'", name_);
        offset_ = header_.tree_offset;
        bulletml_ = parseNode();
        
        if (!bulletml_) {
            SRL::Logger::LogFatal("[BulletML] Failed to parse tree for '%s'", name_);
            return false;
        }
        
        // Build reference maps
        bulletMap_count_ = 0;
        actionMap_count_ = 0;
        fireMap_count_ = 0;
        
        // Process bullet references
        for (uint32_t i = 0; i < bullet_refs_count_; ++i) {
            uint32_t node_id = bullet_refs_[i].node_id;
            if (node_id < BULLETML_MAX_NODES && node_map_[node_id]) {
                if (bulletMap_count_ < BULLETML_MAX_REFS) {
                    bulletMap_[bulletMap_count_++] = node_map_[node_id];
                }
            }
        }
        
        // Process action references
        for (uint32_t i = 0; i < action_refs_count_; ++i) {
            uint32_t node_id = action_refs_[i].node_id;
            if (node_id < BULLETML_MAX_NODES && node_map_[node_id]) {
                if (actionMap_count_ < BULLETML_MAX_REFS) {
                    actionMap_[actionMap_count_++] = node_map_[node_id];
                }
            }
        }
        
        // Process fire references
        for (uint32_t i = 0; i < fire_refs_count_; ++i) {
            uint32_t node_id = fire_refs_[i].node_id;
            if (node_id < BULLETML_MAX_NODES && node_map_[node_id]) {
                if (fireMap_count_ < BULLETML_MAX_REFS) {
                    fireMap_[fireMap_count_++] = node_map_[node_id];
                }
            }
        }
        
        // Find and populate top actions
        SRL::Logger::LogTrace("[BulletML] Finding top actions for '%s'", name_);
        topActions_count_ = 0;
        if (bulletml_) {
            BulletMLNode::ChildIterator ite;
            for (ite = bulletml_->childBegin(); ite != bulletml_->childEnd(); ++ite) {
                if ((*ite)->getNameAsName() == BulletMLNode::action) {
                    if (topActions_count_ < BULLETML_MAX_REFS) {
                        topActions_[topActions_count_++] = *ite;
                    }
                }
            }
        }
        
        SRL::Logger::LogTrace("[BulletML] Parse complete for '%s', found %d top actions", name_, topActions_count_);
        return true;
    }
    
    /// Get last error message (deprecated, use SRL logging instead)
    DECLSPEC const char* getErrorMessage() const { return error_message_; }

private:
    inline void setName(const char* name) {
        uint32_t i = 0;
        for (; i < BULLETML_MAX_FILENAME - 1 && name && name[i] != '\0'; ++i) {
            name_[i] = name[i];
        }
        name_[i] = '\0';
    }
    
    inline void setHorizontal() {
        is_horizontal_ = true;
    }

    // Helper macros for bounds checking
    #define CHECK_BOUNDS(size) \
        if (offset_ + (size) > data_size_) { \
            SRL::Logger::LogFatal("[BulletML] Unexpected end of file in '%s' at offset %u (need %u bytes, have %u)", name_, offset_, (size), data_size_); \
            return false; \
        }

    #define CHECK_BOUNDS_PTR(size) \
        if (offset_ + (size) > data_size_) { \
            SRL::Logger::LogFatal("[BulletML] Unexpected end of file in '%s' at offset %u (need %u bytes, have %u)", name_, offset_, (size), data_size_); \
            return nullptr; \
        }

    /// Read primitives from buffer
    inline uint8_t readUInt8() {
        if (offset_ + 1 > data_size_) {
            return 0;
        }
        return data_[offset_++];
    }
    
    inline uint16_t readUInt16() {
        if (offset_ + 2 > data_size_) {
            return 0;
        }
        // Little-endian
        uint16_t value = data_[offset_] | (data_[offset_ + 1] << 8);
        offset_ += 2;
        return value;
    }
    
    inline uint32_t readUInt32() {
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
    
/// Read string from current position and store its offset
inline const char* readString() {
    uint16_t length = readUInt16();
    if (offset_ + length > data_size_) {
        return "";
    }
    
    // Store offset into data buffer for this string
    if (string_table_count_ < BULLETML_MAX_STRINGS) {
        string_table_offsets_[string_table_count_] = offset_;  // Store offset into data_
        string_table_count_++;
    }
    
    const char* str = reinterpret_cast<const char*>(data_ + offset_);
    offset_ += length;
    return str;
}

/// Get string from string table by index
inline const char* readStringAt(uint32_t index) {
    if (index >= string_table_count_) {
        return "";
    }
    // string_table_offsets_ stores offsets into data_
    return reinterpret_cast<const char*>(data_ + string_table_offsets_[index]);
}
    
    /// Verify header magic and version
    inline bool verifyHeader() {
        CHECK_BOUNDS(sizeof(BulletMLBinaryHeader));
        // Read header without libc helpers
        const uint8_t* h = data_ + offset_;
        header_.magic[0] = static_cast<char>(h[0]);
        header_.magic[1] = static_cast<char>(h[1]);
        header_.magic[2] = static_cast<char>(h[2]);
        header_.magic[3] = static_cast<char>(h[3]);
        header_.version = static_cast<uint16_t>(h[4] | (h[5] << 8));
        header_.orientation = h[6];
        header_.flags = h[7];
        header_.string_table_offset =
            static_cast<uint32_t>(h[8] | (h[9] << 8) | (h[10] << 16) | (h[11] << 24));
        header_.refmap_offset =
            static_cast<uint32_t>(h[12] | (h[13] << 8) | (h[14] << 16) | (h[15] << 24));
        header_.tree_offset =
            static_cast<uint32_t>(h[16] | (h[17] << 8) | (h[18] << 16) | (h[19] << 24));
        header_.file_size =
            static_cast<uint32_t>(h[20] | (h[21] << 8) | (h[22] << 16) | (h[23] << 24));
        offset_ += sizeof(BulletMLBinaryHeader);

        // Verify magic number
        if (header_.magic[0] != 'B' || header_.magic[1] != 'L' || header_.magic[2] != 'B' || header_.magic[3] != '\0') {
            SRL::Logger::LogFatal("[BulletML] Invalid magic number in '%s': %02X %02X %02X %02X", name_,
                                  (unsigned int)(uint8_t)header_.magic[0],
                                  (unsigned int)(uint8_t)header_.magic[1],
                                  (unsigned int)(uint8_t)header_.magic[2],
                                  (unsigned int)(uint8_t)header_.magic[3]);
            return false;
        }
        
        // Check version
        if (header_.version != 1) {
            SRL::Logger::LogFatal("[BulletML] Unsupported version %u in '%s'", header_.version, name_);
            return false;
        }
        
        // Verify file size
        if (header_.file_size != data_size_) {
            SRL::Logger::LogFatal("[BulletML] File size mismatch in '%s': header says %u, actual is %u", name_, header_.file_size, data_size_);
            return false;
        }
        
        // Set orientation
        if (header_.orientation == 1) {
            setHorizontal();
        }
        
        return true;
    }
    
    /// Load string table
    inline bool loadStringTable() {
        CHECK_BOUNDS(4);
        
        offset_ = header_.string_table_offset;
        uint32_t string_count = readUInt32();
        
        if (string_count > BULLETML_MAX_STRINGS) string_count = BULLETML_MAX_STRINGS;
        for (uint32_t i = 0; i < string_count; ++i) {
            readString();
        }
        
        return true;
    }
    
    /// Load reference maps
    inline bool loadReferenceMaps() {
        offset_ = header_.refmap_offset;
        
        // Load bullet references
        CHECK_BOUNDS(4);
        uint32_t bullet_count = readUInt32();
        if (bullet_count > BULLETML_MAX_REFS) bullet_count = BULLETML_MAX_REFS;
        bullet_refs_count_ = 0;
        for (uint32_t i = 0; i < bullet_count; ++i) {
            CHECK_BOUNDS(8);
            BulletMLRefEntry entry;
            entry.label_id = readUInt32();
            entry.node_id = readUInt32();
            if (bullet_refs_count_ < BULLETML_MAX_REFS) {
                bullet_refs_[bullet_refs_count_++] = entry;
            }
        }
        
        // Load action references
        CHECK_BOUNDS(4);
        uint32_t action_count = readUInt32();
        if (action_count > BULLETML_MAX_REFS) action_count = BULLETML_MAX_REFS;
        action_refs_count_ = 0;
        for (uint32_t i = 0; i < action_count; ++i) {
            CHECK_BOUNDS(8);
            BulletMLRefEntry entry;
            entry.label_id = readUInt32();
            entry.node_id = readUInt32();
            if (action_refs_count_ < BULLETML_MAX_REFS) {
                action_refs_[action_refs_count_++] = entry;
            }
        }
        
        // Load fire references
        CHECK_BOUNDS(4);
        uint32_t fire_count = readUInt32();
        if (fire_count > BULLETML_MAX_REFS) fire_count = BULLETML_MAX_REFS;
        fire_refs_count_ = 0;
        for (uint32_t i = 0; i < fire_count; ++i) {
            CHECK_BOUNDS(8);
            BulletMLRefEntry entry;
            entry.label_id = readUInt32();
            entry.node_id = readUInt32();
            if (fire_refs_count_ < BULLETML_MAX_REFS) {
                fire_refs_[fire_refs_count_++] = entry;
            }
        }
        
        return true;
    }
    
    /// Convert binary node type to string name
    inline static const char* nodeTypeToString(uint8_t type) {
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
    
    /// Parse node tree recursively
    inline BulletMLNode* parseNode() {
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
        const char* node_name = nodeTypeToString(node_header.node_type);
        BulletMLNode* node = new BulletMLNode(node_name);
        
        // Set node ID for reference mapping
        uint32_t this_node_id = current_node_id_++;
        if (this_node_id < BULLETML_MAX_NODES) {
            node_map_[this_node_id] = node;
        }
        
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
            if (node_header.value_string_id < string_table_count_) {
                node->setValue(readStringAt(node_header.value_string_id));
            }
        }
        
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
    
    #undef CHECK_BOUNDS
    #undef CHECK_BOUNDS_PTR

protected:
    const uint8_t* data_;        // Pointer to binary data
    uint32_t data_size_;         // Size of data
    uint32_t offset_;            // Current read offset
    bool owns_data_;             // True if we allocated data_
    
    BulletMLBinaryHeader header_;
    
    // Parse scratch storage (shared): only needed while parsing, not at runtime.
    static uint32_t string_table_offsets_[BULLETML_MAX_STRINGS];
    static uint32_t string_table_count_;
    
    // Reference maps: label -> node_id
    static BulletMLRefEntry bullet_refs_[BULLETML_MAX_REFS];
    static uint32_t bullet_refs_count_;
    static BulletMLRefEntry action_refs_[BULLETML_MAX_REFS];
    static uint32_t action_refs_count_;
    static BulletMLRefEntry fire_refs_[BULLETML_MAX_REFS];
    static uint32_t fire_refs_count_;
    
    char error_message_[256];
    
    // Node ID counter for tracking nodes during parsing
    static uint32_t current_node_id_;
    
    // Map of node_id to BulletMLNode* for resolving references (parse scratch)
    static BulletMLNode* node_map_[BULLETML_MAX_NODES];
    
    // Parsed tree and reference maps (replaces BulletMLParser base members)
    BulletMLNode* bulletml_;
    BulletMLNode* bulletMap_[BULLETML_MAX_REFS];
    uint32_t bulletMap_count_;
    BulletMLNode* actionMap_[BULLETML_MAX_REFS];
    uint32_t actionMap_count_;
    BulletMLNode* fireMap_[BULLETML_MAX_REFS];
    uint32_t fireMap_count_;
    BulletMLNode* topActions_[BULLETML_MAX_REFS];
    uint32_t topActions_count_;
    
    char name_[BULLETML_MAX_FILENAME];
    bool is_horizontal_;
};

/// BulletMLParserBLB - File loading wrapper for Saturn
/// Loads binary BulletML files (.blb format) from storage
class BulletMLParserBLB : public BulletMLParserBinary {
public:
    /// Constructor that stores filename for later loading
    explicit BulletMLParserBLB(const char* filename)
        : BulletMLParserBinary(filename, nullptr, 0) {
        // Store filename for loading in parse()
        uint32_t i = 0;
        if (filename) {
            for (; i < BULLETML_MAX_FILENAME - 1 && filename[i] != '\0'; ++i) {
                filename_[i] = filename[i];
            }
        }
        filename_[i] = '\0';
    }
    
    /// Virtual destructor
    virtual ~BulletMLParserBLB() {}
    
    /// Override parse to load file first
    /// Returns true on success, false on failure
    virtual bool parse() override {
        // Load file data
        if (filename_[0] == '\0') {
            SRL::Logger::LogFatal("[BulletML] No filename specified");
            return false;
        }
        
        if (!loadFromFile(filename_)) {
            SRL::Logger::LogFatal("[BulletML] Failed to load file: %s", filename_);
            return false;
        }
        
        // Data loaded and allocated by loadFromFile(), call base class parse
        return BulletMLParserBinary::parse();
    }
    
private:
    char filename_[BULLETML_MAX_FILENAME];
    /// Static temp buffer for file loading (reused across instances)
    static uint8_t temp_load_buffer_[65536];
    
    /// Load binary file from storage
    bool loadFromFile(const char* filename) {
        if (!filename) {
            SRL::Logger::LogFatal("[BulletML] loadFromFile: null filename");
            return false;
        }

        SRL::Logger::LogTrace("[BulletML] Attempting to load file: %s", filename);
        
        SRL::Cd::File file(filename);
        
        if (!file.Exists()) {
            SRL::Logger::LogFatal("[BulletML] File does not exist: %s", filename);
            return false;
        }
        
        SRL::Logger::LogTrace("[BulletML] File exists: %s, attempting to open", filename);
        
        if (!file.Open()) {
            SRL::Logger::LogFatal("[BulletML] Failed to open file: %s", filename);
            return false;
        }
        
        SRL::Logger::LogTrace("[BulletML] File opened: %s, size: %d bytes", filename, file.Size.Bytes);

        // Read file into temporary static buffer, limiting to actual file size
        int32_t bytes_to_read = (int32_t)file.Size.Bytes;
        if (bytes_to_read > (int32_t)sizeof(temp_load_buffer_)) {
            SRL::Logger::LogFatal("[BulletML] File too large: %s (%d bytes, buffer is %d)", 
                                  filename, bytes_to_read, (int32_t)sizeof(temp_load_buffer_));
            file.Close();
            return false;
        }

        int32_t bytes_read = file.Read(bytes_to_read, temp_load_buffer_);
        file.Close();

        if (bytes_read <= 0) {
            SRL::Logger::LogFatal("[BulletML] Failed to read file: %s (bytes_read=%d)", filename, bytes_read);
            return false;
        }

        SRL::Logger::LogTrace("[BulletML] Successfully read %d bytes from %s (expected %d)", 
                              bytes_read, filename, bytes_to_read);

        // If parse() is called again on the same parser, free previous owned data first.
        if (owns_data_ && data_) {
            delete[] data_;
            data_ = nullptr;
            data_size_ = 0;
            owns_data_ = false;
        }
        
        // Allocate exact size needed and copy data (so temp buffer can be reused)
        uint8_t* allocated_buffer = new uint8_t[bytes_read];
        if (!allocated_buffer) {
            SRL::Logger::LogFatal("[BulletML] Failed to allocate %d bytes for %s", bytes_read, filename);
            return false;
        }
        
        memcpy(allocated_buffer, temp_load_buffer_, bytes_read);
        SRL::Logger::LogTrace("[BulletML] Allocated and copied %d bytes for %s", bytes_read, filename);
        if (bytes_read >= 4) {
            SRL::Logger::LogTrace("[BulletML] First 4 bytes for %s: %02X %02X %02X %02X", filename,
                                  (unsigned int)allocated_buffer[0],
                                  (unsigned int)allocated_buffer[1],
                                  (unsigned int)allocated_buffer[2],
                                  (unsigned int)allocated_buffer[3]);
        }
        
        // Update data pointers (base class will own and delete this memory)
        data_ = allocated_buffer;
        data_size_ = (uint32_t)bytes_read;
        owns_data_ = true;
        
        return true;
    }
};

// Shared parse scratch storage (reduces per-instance RAM usage significantly).
inline uint32_t BulletMLParserBinary::string_table_offsets_[BULLETML_MAX_STRINGS];
inline uint32_t BulletMLParserBinary::string_table_count_ = 0;
inline BulletMLRefEntry BulletMLParserBinary::bullet_refs_[BULLETML_MAX_REFS];
inline uint32_t BulletMLParserBinary::bullet_refs_count_ = 0;
inline BulletMLRefEntry BulletMLParserBinary::action_refs_[BULLETML_MAX_REFS];
inline uint32_t BulletMLParserBinary::action_refs_count_ = 0;
inline BulletMLRefEntry BulletMLParserBinary::fire_refs_[BULLETML_MAX_REFS];
inline uint32_t BulletMLParserBinary::fire_refs_count_ = 0;
inline uint32_t BulletMLParserBinary::current_node_id_ = 0;
inline BulletMLNode* BulletMLParserBinary::node_map_[BULLETML_MAX_NODES];

// Static temp buffer for file loading (shared across all instances for loading only)
inline uint8_t BulletMLParserBLB::temp_load_buffer_[65536];

#endif // BULLETMLPARSER_BINARY_HPP_
