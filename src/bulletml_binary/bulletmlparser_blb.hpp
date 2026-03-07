/// BulletML Binary Parser - Header-Only Implementation
/**
 * Fast binary parser for BulletML files.
 * Reads .blb (BulletML Binary) files instead of XML.
 * 
 * This is a header-only library that can be directly integrated into your project.
 * 
 * Usage:
 *   #include "bulletmlparser_blb.hpp"
 *   
 *   // From file:
 *   BulletMLParserBLB parser("pattern.blb");
 *   if (parser.build()) {
 *       // Use parser
 *   }
 *   
 *   // From memory:
 *   BulletMLParserBLB parser(data_ptr, data_size);
 *   if (parser.build()) {
 *       // Use parser
 *   }
 * 
 * Binary Format:
 *   - Magic: "BLB\x00" (4 bytes)
 *   - Version: 1 (uint16_t)
 *   - See tools/BINARY_FORMAT.md for complete specification
 */

#ifndef BULLETMLPARSER_BLB_HPP_
#define BULLETMLPARSER_BLB_HPP_

#include <cstdint>
#include <srl_cd.hpp>
#include <srl_log.hpp>

// Manual size_t definition (stddef.h not available on Saturn)
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned int size_t;
#endif


// Maximum limits used for dynamic allocation and validation
#define BULLETML_MAX_CHILDREN 32
#define BULLETML_MAX_FILENAME 32
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
    
    explicit BulletMLNode(const char* name, uint32_t child_capacity = BULLETML_MAX_CHILDREN)
        : name_(name), type_(type_none), value_(nullptr), refID_(0xFFFFFFFF),
          children_(nullptr), child_capacity_(0), child_count_(0), parent_(nullptr) {
        if (child_capacity > BULLETML_MAX_CHILDREN) {
            child_capacity = BULLETML_MAX_CHILDREN;
        }
        child_capacity_ = child_capacity;
        if (child_capacity_ > 0) {
            children_ = new BulletMLNode*[child_capacity_];
            for (uint32_t i = 0; i < child_capacity_; ++i) {
                children_[i] = nullptr;
            }
        }
    }
    
    ~BulletMLNode() {
        for (uint32_t i = 0; i < child_count_; ++i) {
            delete children_[i];
        }
        delete[] children_;
        children_ = nullptr;
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
        if (child_count_ < child_capacity_) {
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
    BulletMLNode** children_;
    uint32_t child_capacity_;
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
class BulletMLParserBLB;

#include "bulletmlstate.hpp"

/// Binary BulletML Parser - Standalone Implementation
class BulletMLParserBLB {
public:
    /// Constructor from memory buffer (filename for identification)
    explicit BulletMLParserBLB(const char* filename, const uint8_t* data, uint32_t size)
                : flags_(0),
                    data_(data), data_size_(size), offset_(0),
                    parse_step_count_(0), parse_step_budget_(0),
                    string_table_offsets_(nullptr),
                    bullet_refs_(nullptr),
                    action_refs_(nullptr),
                    fire_refs_(nullptr),
                    current_node_id_(0), node_map_(nullptr), node_map_capacity_(0),
                    bulletml_(nullptr),
                    bulletMap_(nullptr), bulletMap_count_(0),
                    actionMap_(nullptr), actionMap_count_(0),
                    fireMap_(nullptr), fireMap_count_(0),
                    topActions_(nullptr), topActions_count_(0),
                    name_(nullptr), filename_(nullptr), error_message_(nullptr),
                    string_table_count_(0),
                    bullet_refs_count_(0),
                    action_refs_count_(0),
                    fire_refs_count_(0)
    {
        allocateMetadataBuffers();
        setName(filename);
        if (error_message_) error_message_[0] = '\0';
        if (filename_) filename_[0] = '\0';
    }
    
    /// Constructor from memory buffer (unnamed)
    explicit BulletMLParserBLB(const uint8_t* data, uint32_t size)
                : flags_(0),
                    data_(data), data_size_(size), offset_(0),
                    parse_step_count_(0), parse_step_budget_(0),
                    string_table_offsets_(nullptr),
                    bullet_refs_(nullptr),
                    action_refs_(nullptr),
                    fire_refs_(nullptr),
                    current_node_id_(0), node_map_(nullptr), node_map_capacity_(0),
                    bulletml_(nullptr),
                    bulletMap_(nullptr), bulletMap_count_(0),
                    actionMap_(nullptr), actionMap_count_(0),
                    fireMap_(nullptr), fireMap_count_(0),
                    topActions_(nullptr), topActions_count_(0),
                    name_(nullptr), filename_(nullptr), error_message_(nullptr),
                    string_table_count_(0),
                    bullet_refs_count_(0),
                    action_refs_count_(0),
                    fire_refs_count_(0)
    {
        allocateMetadataBuffers();
        setName("(memory)");
        if (error_message_) error_message_[0] = '\0';
        if (filename_) filename_[0] = '\0';
    }

    /// Constructor from filename (file-loaded parse path)
    explicit BulletMLParserBLB(const char* filename)
                : flags_(0),
                    data_(nullptr), data_size_(0), offset_(0),
                    parse_step_count_(0), parse_step_budget_(0),
                    string_table_offsets_(nullptr),
                    bullet_refs_(nullptr),
                    action_refs_(nullptr),
                    fire_refs_(nullptr),
                    current_node_id_(0), node_map_(nullptr), node_map_capacity_(0),
                    bulletml_(nullptr),
                    bulletMap_(nullptr), bulletMap_count_(0),
                    actionMap_(nullptr), actionMap_count_(0),
                    fireMap_(nullptr), fireMap_count_(0),
                    topActions_(nullptr), topActions_count_(0),
                    name_(nullptr), filename_(nullptr), error_message_(nullptr),
                    string_table_count_(0),
                    bullet_refs_count_(0),
                    action_refs_count_(0),
                    fire_refs_count_(0)
    {
        allocateMetadataBuffers();
        setName(filename);
        if (error_message_) error_message_[0] = '\0';

        uint32_t i = 0;
        if (filename_ && filename) {
            for (; i < BULLETML_MAX_FILENAME - 1 && filename[i] != '\0'; ++i) {
                filename_[i] = filename[i];
            }
        }
        if (filename_) filename_[i] = '\0';
    }
    
    virtual ~BulletMLParserBLB() {
        freeParseScratch();
        freeRuntimeMaps();
        if (bulletml_) {
            delete bulletml_;
            bulletml_ = nullptr;
        }
        if ((flags_ & 0x01) && data_) {  // bit 0 = owns_data_
            delete[] data_;
            data_ = nullptr;
            data_size_ = 0;
            flags_ &= ~0x01;  // clear owns_data_ bit
        }
        delete[] error_message_;
        error_message_ = nullptr;
        delete[] name_;
        name_ = nullptr;
        delete[] filename_;
        filename_ = nullptr;
    }
    
    /// Build the parser (calls parse internally)
    /// Returns true on success, false on failure
    bool build() {
        return parse();
    }
    
    /// Get the root node
    BulletMLNode* getRoot() const { return bulletml_; }
    
    /// Get bullet by reference ID
    BulletMLNode* getBullet(uint32_t id) const {
        return id < bulletMap_count_ ? bulletMap_[id] : nullptr;
    }
    
    /// Get action by reference ID
    BulletMLNode* getAction(uint32_t id) const {
        return id < actionMap_count_ ? actionMap_[id] : nullptr;
    }
    
    /// Get fire by reference ID  
    BulletMLNode* getFire(uint32_t id) const {
        return id < fireMap_count_ ? fireMap_[id] : nullptr;
    }
    
    /// Get top-level actions
    BulletMLNode** getTopActions(uint32_t* count) const {
        if (count) *count = topActions_count_;
        return topActions_;
    }
    
    /// Check if pattern is horizontal
    bool isHorizontal() const { return (flags_ & 0x02) != 0; }  // bit 1 = is_horizontal_
    
    /// Get parser name
    const char* getName() const { return name_; }

    /// Parse the binary file (called by build())
    /// Returns true on success, false on failure
    virtual bool parse() {
        freeRuntimeMaps();

        if (!ensureParseScratch()) {
            SRL::Logger::LogFatal("[BulletML] Failed to allocate parse scratch for '%s'", name_ ? name_ : "(null)");
            return false;
        }

        if (bulletml_) {
            delete bulletml_;
            bulletml_ = nullptr;
        }

        if ((!data_ || data_size_ == 0) && filename_ && filename_[0] != '\0') {
            if (!loadFromFile(filename_)) {
                SRL::Logger::LogFatal("[BulletML] Failed to load file: %s", filename_);
                freeParseScratch();
                return false;
            }
        }

        if (!data_ || data_size_ == 0) {
            SRL::Logger::LogFatal("[BulletML] No data to parse for '%s'", name_ ? name_ : "(null)");
            freeParseScratch();
            return false;
        }

        SRL::Logger::LogTrace("[BulletML] Starting parse for '%s', size=%d", name_, data_size_);

        offset_ = 0;
        current_node_id_ = 0;
        parse_step_count_ = 0;
        parse_step_budget_ = (data_size_ / 8) + 256;
        if (parse_step_budget_ < 512) {
            parse_step_budget_ = 512;
        }
        string_table_count_ = 0;
        bullet_refs_count_ = 0;
        action_refs_count_ = 0;
        fire_refs_count_ = 0;

        SRL::Logger::LogTrace("[BulletML] Verifying header for '%s'", name_);
        if (!verifyHeader()) {
            SRL::Logger::LogFatal("[BulletML] Header verification failed for '%s'", name_);
            freeParseScratch();
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] Header verified for '%s'", name_);

        uint32_t actual_node_count = countNodesInTree();
        if (actual_node_count == 0 || actual_node_count > BULLETML_MAX_NODES) {
            SRL::Logger::LogFatal("[BulletML] Invalid node count %u for '%s'", actual_node_count, name_);
            freeParseScratch();
            return false;
        }
        if (!allocateNodeMap(actual_node_count)) {
            freeParseScratch();
            return false;
        }
        for (uint32_t i = 0; i < node_map_capacity_; ++i) {
            node_map_[i] = nullptr;
        }

        SRL::Logger::LogTrace("[BulletML] Loading string table for '%s'", name_);
        if (!loadStringTable()) {
            SRL::Logger::LogFatal("[BulletML] Failed to load string table for '%s'", name_);
            freeParseScratch();
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] String table loaded for '%s'", name_);

        SRL::Logger::LogTrace("[BulletML] Loading reference maps for '%s'", name_);
        if (!loadReferenceMaps()) {
            SRL::Logger::LogFatal("[BulletML] Failed to load reference maps for '%s'", name_);
            freeParseScratch();
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] Reference maps loaded for '%s'", name_);

        SRL::Logger::LogTrace("[BulletML] Parsing tree for '%s'", name_);
        offset_ = header_.tree_offset;
        bulletml_ = parseNode();
        if (!bulletml_) {
            SRL::Logger::LogFatal("[BulletML] Failed to parse tree for '%s'", name_);
            freeParseScratch();
            return false;
        }

        bulletMap_count_ = 0;
        actionMap_count_ = 0;
        fireMap_count_ = 0;
        topActions_count_ = 0;

        uint32_t bullet_valid_count = 0;
        uint32_t action_valid_count = 0;
        uint32_t fire_valid_count = 0;
        uint32_t top_valid_count = 0;

        for (uint32_t i = 0; i < bullet_refs_count_; ++i) {
            uint32_t node_id = bullet_refs_[i].node_id;
            if (node_id < node_map_capacity_ && node_map_[node_id]) {
                ++bullet_valid_count;
            }
        }
        for (uint32_t i = 0; i < action_refs_count_; ++i) {
            uint32_t node_id = action_refs_[i].node_id;
            if (node_id < node_map_capacity_ && node_map_[node_id]) {
                ++action_valid_count;
            }
        }
        for (uint32_t i = 0; i < fire_refs_count_; ++i) {
            uint32_t node_id = fire_refs_[i].node_id;
            if (node_id < node_map_capacity_ && node_map_[node_id]) {
                ++fire_valid_count;
            }
        }
        if (bulletml_) {
            BulletMLNode::ChildIterator ite;
            for (ite = bulletml_->childBegin(); ite != bulletml_->childEnd(); ++ite) {
                if ((*ite)->getNameAsName() == BulletMLNode::action) {
                    ++top_valid_count;
                }
            }
        }

        if (!allocateRuntimeMaps(bullet_valid_count, action_valid_count, fire_valid_count, top_valid_count)) {
            SRL::Logger::LogFatal("[BulletML] Failed to allocate runtime maps for '%s'", name_ ? name_ : "(null)");
            freeParseScratch();
            return false;
        }

        for (uint32_t i = 0; i < bullet_refs_count_; ++i) {
            uint32_t node_id = bullet_refs_[i].node_id;
            if (node_id < node_map_capacity_ && node_map_[node_id] && bulletMap_count_ < bullet_valid_count) {
                bulletMap_[bulletMap_count_++] = node_map_[node_id];
            }
        }
        for (uint32_t i = 0; i < action_refs_count_; ++i) {
            uint32_t node_id = action_refs_[i].node_id;
            if (node_id < node_map_capacity_ && node_map_[node_id] && actionMap_count_ < action_valid_count) {
                actionMap_[actionMap_count_++] = node_map_[node_id];
            }
        }
        for (uint32_t i = 0; i < fire_refs_count_; ++i) {
            uint32_t node_id = fire_refs_[i].node_id;
            if (node_id < node_map_capacity_ && node_map_[node_id] && fireMap_count_ < fire_valid_count) {
                fireMap_[fireMap_count_++] = node_map_[node_id];
            }
        }

        SRL::Logger::LogTrace("[BulletML] Finding top actions for '%s'", name_);
        topActions_count_ = 0;
        if (bulletml_) {
            BulletMLNode::ChildIterator ite;
            for (ite = bulletml_->childBegin(); ite != bulletml_->childEnd(); ++ite) {
                if ((*ite)->getNameAsName() == BulletMLNode::action && topActions_count_ < top_valid_count) {
                    topActions_[topActions_count_++] = *ite;
                }
            }
        }

        SRL::Logger::LogTrace("[BulletML] Parse complete for '%s', found %d top actions", name_, topActions_count_);

        if ((flags_ & 0x01) && data_) {  // bit 0 = owns_data_
            SRL::Logger::LogTrace("[BulletML] Freeing raw data buffer (%d bytes) for '%s'", data_size_, name_);
            delete[] data_;
            data_ = nullptr;
            data_size_ = 0;
            flags_ &= ~0x01;  // clear owns_data_ bit
        }

        freeParseScratch();
        return true;
    }
    
    /// Get last error message (deprecated, use SRL logging instead)
    const char* getErrorMessage() const { return error_message_; }

private:
    inline void allocateMetadataBuffers() {
        error_message_ = new char[256];
        name_ = new char[BULLETML_MAX_FILENAME];
        filename_ = new char[BULLETML_MAX_FILENAME];
    }

    inline bool allocateRuntimeMaps(uint32_t bullet_count,
                                    uint32_t action_count,
                                    uint32_t fire_count,
                                    uint32_t top_count) {
        freeRuntimeMaps();

        if (bullet_count > 0) {
            bulletMap_ = new BulletMLNode*[bullet_count];
            if (!bulletMap_) {
                freeRuntimeMaps();
                return false;
            }
            for (uint32_t i = 0; i < bullet_count; ++i) {
                bulletMap_[i] = nullptr;
            }
        }

        if (action_count > 0) {
            actionMap_ = new BulletMLNode*[action_count];
            if (!actionMap_) {
                freeRuntimeMaps();
                return false;
            }
            for (uint32_t i = 0; i < action_count; ++i) {
                actionMap_[i] = nullptr;
            }
        }

        if (fire_count > 0) {
            fireMap_ = new BulletMLNode*[fire_count];
            if (!fireMap_) {
                freeRuntimeMaps();
                return false;
            }
            for (uint32_t i = 0; i < fire_count; ++i) {
                fireMap_[i] = nullptr;
            }
        }

        if (top_count > 0) {
            topActions_ = new BulletMLNode*[top_count];
            if (!topActions_) {
                freeRuntimeMaps();
                return false;
            }
            for (uint32_t i = 0; i < top_count; ++i) {
                topActions_[i] = nullptr;
            }
        }

        return true;
    }

    inline void freeRuntimeMaps() {
        delete[] bulletMap_;
        bulletMap_ = nullptr;
        delete[] actionMap_;
        actionMap_ = nullptr;
        delete[] fireMap_;
        fireMap_ = nullptr;
        delete[] topActions_;
        topActions_ = nullptr;
        bulletMap_count_ = 0;
        actionMap_count_ = 0;
        fireMap_count_ = 0;
        topActions_count_ = 0;
    }

    inline bool ensureParseScratch() {
        if (!string_table_offsets_) {
            string_table_offsets_ = new uint16_t[BULLETML_MAX_STRINGS];
            if (!string_table_offsets_) {
                freeParseScratch();
                return false;
            }
        }
        if (!bullet_refs_) {
            bullet_refs_ = new BulletMLRefEntry[BULLETML_MAX_REFS];
            if (!bullet_refs_) {
                freeParseScratch();
                return false;
            }
        }
        if (!action_refs_) {
            action_refs_ = new BulletMLRefEntry[BULLETML_MAX_REFS];
            if (!action_refs_) {
                freeParseScratch();
                return false;
            }
        }
        if (!fire_refs_) {
            fire_refs_ = new BulletMLRefEntry[BULLETML_MAX_REFS];
            if (!fire_refs_) {
                freeParseScratch();
                return false;
            }
        }
        return true;
    }

    inline void freeParseScratch() {
        delete[] string_table_offsets_;
        string_table_offsets_ = nullptr;
        delete[] bullet_refs_;
        bullet_refs_ = nullptr;
        delete[] action_refs_;
        action_refs_ = nullptr;
        delete[] fire_refs_;
        fire_refs_ = nullptr;
        delete[] node_map_;
        node_map_ = nullptr;
        node_map_capacity_ = 0;
        string_table_count_ = 0;
        bullet_refs_count_ = 0;
        action_refs_count_ = 0;
        fire_refs_count_ = 0;
        current_node_id_ = 0;
    }

    inline bool allocateNodeMap(uint32_t node_count) {
        delete[] node_map_;
        node_map_ = nullptr;
        node_map_capacity_ = 0;
        if (node_count == 0) {
            return false;
        }
        node_map_ = new BulletMLNode*[node_count];
        if (!node_map_) {
            SRL::Logger::LogFatal("[BulletML] Failed to allocate node_map for %u nodes in '%s'", node_count, name_ ? name_ : "(null)");
            return false;
        }
        node_map_capacity_ = node_count;
        return true;
    }

    inline uint32_t countNodesInTree() {
        if (!data_ || header_.tree_offset >= data_size_) {
            return 0;
        }
        uint32_t saved_offset = offset_;
        uint32_t count = 0;
        offset_ = header_.tree_offset;
        if (!countNodesRecursive(count, 0)) {
            offset_ = saved_offset;
            return 0;
        }
        offset_ = saved_offset;
        return count;
    }

    inline bool countNodesRecursive(uint32_t& count, uint32_t depth) {
        if (depth > BULLETML_MAX_NODES || offset_ + sizeof(BulletMLNodeHeader) > data_size_) {
            return false;
        }

        uint8_t node_type = readUInt8();
        uint8_t value_type = readUInt8();
        uint16_t child_count = readUInt16();
        (void)readUInt32();
        (void)readUInt32();
        (void)readUInt32();

        if (node_type >= 20 || value_type >= 5 || child_count > BULLETML_MAX_CHILDREN) {
            return false;
        }

        ++count;
        for (uint16_t i = 0; i < child_count; ++i) {
            if (!countNodesRecursive(count, depth + 1)) {
                return false;
            }
        }
        return true;
    }

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
        
        SRL::Logger::LogTrace("[BulletML] File exists: %s, preparing direct read", filename);

        // Read file directly into final-sized buffer to avoid extra temporary allocation.
        const uint32_t max_file_size = 65536;
        int32_t bytes_to_read = (int32_t)file.Size.Bytes;
        if (bytes_to_read > (int32_t)max_file_size) {
            SRL::Logger::LogFatal("[BulletML] File too large: %s (%d bytes, buffer is %d)", 
                                  filename, bytes_to_read, (int32_t)max_file_size);
            file.Close();
            return false;
        }

        // If parse() is called again on the same parser, free previous owned data first.
        if ((flags_ & 0x01) && data_) {  // bit 0 = owns_data_
            delete[] data_;
            data_ = nullptr;
            data_size_ = 0;
            flags_ &= ~0x01;  // clear owns_data_ bit
        }
        
        // Allocate exact size needed and read directly into it.
        uint8_t* allocated_buffer = new uint8_t[bytes_to_read];
        if (!allocated_buffer) {
            SRL::Logger::LogFatal("[BulletML] Failed to allocate %d bytes for %s", bytes_to_read, filename);
            return false;
        }

        int32_t bytes_read = file.LoadBytes(0, bytes_to_read, allocated_buffer);
        if (bytes_read <= 0) {
            SRL::Logger::LogFatal("[BulletML] Failed to load file bytes: %s (bytes_read=%d)", filename, bytes_read);
            delete[] allocated_buffer;
            return false;
        }
        SRL::Logger::LogTrace("[BulletML] Successfully read %d bytes from %s (expected %d)", 
                              bytes_read, filename, bytes_to_read);
        
        // Update data pointers (class will own and delete this memory)
        data_ = allocated_buffer;
        data_size_ = (uint32_t)bytes_read;
        flags_ |= 0x01;  // set owns_data_ bit
        
        return true;
    }

    inline void setName(const char* name) {
        if (!name_) {
            return;
        }
        uint32_t i = 0;
        for (; i < BULLETML_MAX_FILENAME - 1 && name && name[i] != '\0'; ++i) {
            name_[i] = name[i];
        }
        name_[i] = '\0';
    }
    
    inline void setHorizontal() {
        flags_ |= 0x02;  // set is_horizontal_ bit
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
    if (string_table_offsets_ && string_table_count_ < BULLETML_MAX_STRINGS) {
        string_table_offsets_[string_table_count_] = offset_;  // Store offset into data_
        string_table_count_++;
    }
    
    const char* str = reinterpret_cast<const char*>(data_ + offset_);
    offset_ += length;
    return str;
}

/// Get string from string table by index
inline const char* readStringAt(uint32_t index) {
    if (!string_table_offsets_ || index >= string_table_count_) {
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

        // Validate section offsets to prevent out-of-range tree parsing.
        if (header_.string_table_offset < sizeof(BulletMLBinaryHeader) || header_.string_table_offset >= data_size_) {
            SRL::Logger::LogFatal("[BulletML] Invalid string table offset in '%s': %u (size=%u)",
                                  name_, header_.string_table_offset, data_size_);
            return false;
        }
        if (header_.refmap_offset < sizeof(BulletMLBinaryHeader) || header_.refmap_offset >= data_size_) {
            SRL::Logger::LogFatal("[BulletML] Invalid reference map offset in '%s': %u (size=%u)",
                                  name_, header_.refmap_offset, data_size_);
            return false;
        }
        if (header_.tree_offset < sizeof(BulletMLBinaryHeader) || header_.tree_offset >= data_size_) {
            SRL::Logger::LogFatal("[BulletML] Invalid tree offset in '%s': %u (size=%u)",
                                  name_, header_.tree_offset, data_size_);
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
        switch (type) {
            case 0: return "bulletml";
            case 1: return "bullet";
            case 2: return "action";
            case 3: return "fire";
            case 4: return "changeDirection";
            case 5: return "changeSpeed";
            case 6: return "accel";
            case 7: return "wait";
            case 8: return "repeat";
            case 9: return "bulletRef";
            case 10: return "actionRef";
            case 11: return "fireRef";
            case 12: return "vanish";
            case 13: return "horizontal";
            case 14: return "vertical";
            case 15: return "term";
            case 16: return "times";
            case 17: return "direction";
            case 18: return "speed";
            case 19: return "param";
            default: break;
        }
        return "unknown";
    }
    
    /// Parse node tree recursively
    inline BulletMLNode* parseNode(uint32_t depth = 0) {
        if (depth > BULLETML_MAX_NODES) {
            SRL::Logger::LogFatal("[BulletML] Parse depth limit exceeded in '%s'", name_);
            return nullptr;
        }

        if (current_node_id_ >= node_map_capacity_) {
            SRL::Logger::LogFatal("[BulletML] Node count limit exceeded in '%s' (max=%u)", name_, node_map_capacity_);
            return nullptr;
        }

        parse_step_count_++;
        if (parse_step_count_ > parse_step_budget_) {
            SRL::Logger::LogFatal("[BulletML] Parse step budget exceeded in '%s' (steps=%u, budget=%u)",
                                  name_, parse_step_count_, parse_step_budget_);
            return nullptr;
        }

        CHECK_BOUNDS_PTR(16);
        
        // Read node header
        BulletMLNodeHeader node_header;
        node_header.node_type = readUInt8();
        node_header.value_type = readUInt8();
        node_header.child_count = readUInt16();
        node_header.ref_id = readUInt32();
        node_header.value_string_id = readUInt32();
        node_header.label_string_id = readUInt32();

        if (node_header.node_type >= 20) {
            SRL::Logger::LogFatal("[BulletML] Invalid node type %u in '%s' at depth %u (offset=%u)",
                                  node_header.node_type, name_, depth, offset_);
            return nullptr;
        }

        if (node_header.value_type >= 5) {
            SRL::Logger::LogFatal("[BulletML] Invalid value type %u in '%s' at depth %u (offset=%u)",
                                  node_header.value_type, name_, depth, offset_);
            return nullptr;
        }

        if (node_header.child_count > BULLETML_MAX_CHILDREN) {
            SRL::Logger::LogFatal("[BulletML] Child count %u exceeds max %u in '%s' at depth %u",
                                  node_header.child_count, BULLETML_MAX_CHILDREN, name_, depth);
            return nullptr;
        }
        
        // Create node
        const char* node_name = nodeTypeToString(node_header.node_type);
        BulletMLNode* node = new BulletMLNode(node_name, node_header.child_count);
        
        // Set node ID for reference mapping
        uint32_t this_node_id = current_node_id_++;
        if (this_node_id < node_map_capacity_) {
            node_map_[this_node_id] = node;
        }
        
        // Set type if present
        if (node_header.value_type != 0) {
            switch (node_header.value_type) {
                case 1: node->setType("aim"); break;
                case 2: node->setType("absolute"); break;
                case 3: node->setType("relative"); break;
                case 4: node->setType("sequence"); break;
                default: break;
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
            BulletMLNode* child = parseNode(depth + 1);
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
    // Flags: bit 0 = owns_data_, bit 1 = is_horizontal_
    // (Consolidates two bool members into single byte to eliminate 6+ bytes of padding)
    uint8_t flags_;
    
    // Input data offsets (6 bytes from optimized types)
    // Max BLB file size: 1699 bytes (verified from live CD data)
    uint16_t data_size_;         // Size of data (max 65535 bytes, actual max 1699)
    uint16_t offset_;            // Current read offset (max 1699)
    
    // Padding for alignment to 4-byte boundary (2 bytes)
    uint16_t pad_;
    
    // Pointer to binary data (4 bytes on 32-bit systems)
    const uint8_t* data_;

    // Parse state (4 bytes - optimized from uint32_t)
    // Budget calculated as: (data_size_ / 8) + 256, clamped to min 512
    // Max across all 73 BLB files: 512 (fits in uint16_t max 65535)
    uint16_t parse_step_count_;
    uint16_t parse_step_budget_;
    
    // Embedded header structure (24 bytes) - no padding needed
    BulletMLBinaryHeader header_;
    
    // Parse scratch pointers grouped for cache efficiency (20 bytes - expanded with offset storage)
    uint16_t* string_table_offsets_;  // Optimized: uint16_t valid for offsets max 1699 bytes (saves 2000 bytes per instance)
    BulletMLRefEntry* bullet_refs_;
    BulletMLRefEntry* action_refs_;
    BulletMLRefEntry* fire_refs_;

    // Node ID counter and node map for parsing (8 bytes - optimized from 12)
    // Max current_node_id_: BULLETML_MAX_NODES = 2000
    // Max node_map_capacity_: BULLETML_MAX_NODES = 2000
    uint16_t current_node_id_;
    BulletMLNode** node_map_;     // Dynamically allocated, sized to actual node count
    uint16_t node_map_capacity_;
    
    // Runtime tree and reference maps (18 bytes - optimized from 24)
    // Max counts: bulletMap/actionMap/fireMap = 500, topActions = ~32-40
    BulletMLNode* bulletml_;
    BulletMLNode** bulletMap_;
    uint16_t bulletMap_count_;
    BulletMLNode** actionMap_;
    uint16_t actionMap_count_;
    BulletMLNode** fireMap_;
    uint16_t fireMap_count_;
    BulletMLNode** topActions_;
    uint16_t topActions_count_;
    
    // Metadata pointers (12 bytes)
    char* name_;
    char* filename_;
    char* error_message_;
    
    // Parse scratch counts (8 bytes - optimized from 16)
    // Max string_table_count_: BULLETML_MAX_STRINGS = 1000
    // Max *_refs_count_: BULLETML_MAX_REFS = 500
    uint16_t string_table_count_;
    uint16_t bullet_refs_count_;
    uint16_t action_refs_count_;
    uint16_t fire_refs_count_;
};

#endif // BULLETMLPARSER_BLB_HPP_
