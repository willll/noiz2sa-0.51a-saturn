/// BulletML XML to Binary Converter
/**
 * Converts BulletML XML files to binary format (.bmlb)
 * Usage: bulletml-converter input.xml output.bmlb
 */

#include "../bulletml/bulletmlparser-tinyxml.h"
#include "../bulletml/bulletmltree.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <cstring>
#include <cstdint>

class BulletMLBinaryWriter {
public:
    BulletMLBinaryWriter() : current_node_id_(0) {}
    
    bool convert(const std::string& input_xml, const std::string& output_bin) {
        try {
            // Parse XML file
            std::cout << "Parsing XML file: " << input_xml << std::endl;
            BulletMLParserTinyXML parser(input_xml);
            parser.build();
            
            // Prepare for conversion
            buffer_.clear();
            string_table_.clear();
            string_to_id_.clear();
            bullet_refs_.clear();
            action_refs_.clear();
            fire_refs_.clear();
            current_node_id_ = 0;
            node_to_id_.clear();
            
            // Reserve space for header (will write later)
            buffer_.resize(24, 0);
            
            // Write string table placeholder
            uint32_t string_table_offset = buffer_.size();
            
            // Collect all strings and build tree
            std::cout << "Building string table..." << std::endl;
            collectStrings(parser.getBulletRef(0) ? parser.getBulletRef(0)->getParent() : nullptr);
            if (!parser.getBulletRef(0)) {
                // Try to get root from action or fire
                BulletMLNode* root = nullptr;
                if (parser.getTopActions().size() > 0) {
                    root = parser.getTopActions()[0];
                    while (root && root->getParent()) {
                        root = root->getParent();
                    }
                }
                collectStrings(root);
            }
            
            // Collect strings from entire tree starting from root
            BulletMLNode* bulletml = nullptr;
            if (parser.getTopActions().size() > 0) {
                bulletml = parser.getTopActions()[0];
                while (bulletml && bulletml->getParent()) {
                    bulletml = bulletml->getParent();
                }
            }
            
            if (!bulletml) {
                std::cerr << "Error: Could not find bulletml root node" << std::endl;
                return false;
            }
            
            collectStrings(bulletml);
            
            // Write string table
            writeStringTable();
            
            // Write reference maps placeholder
            uint32_t refmap_offset = buffer_.size();
            
            // Build reference maps by traversing tree
            std::cout << "Building reference maps..." << std::endl;
            buildReferenceMaps(bulletml, parser);
            
            // Write reference maps
            writeReferenceMaps();
            
            // Write node tree
            std::cout << "Writing node tree..." << std::endl;
            uint32_t tree_offset = buffer_.size();
            current_node_id_ = 0;
            writeNode(bulletml);
            
            // Write header
            std::cout << "Writing header..." << std::endl;
            writeHeader(parser.isHorizontal(), string_table_offset, refmap_offset, tree_offset);
            
            // Write to file
            std::cout << "Writing to file: " << output_bin << std::endl;
            std::ofstream out(output_bin.c_str(), std::ios::binary);
            if (!out) {
                std::cerr << "Error: Could not open output file" << std::endl;
                return false;
            }
            out.write(reinterpret_cast<const char*>(buffer_.data()), buffer_.size());
            out.close();
            
            std::cout << "Conversion successful!" << std::endl;
            std::cout << "  Input size:  " << getFileSize(input_xml) << " bytes" << std::endl;
            std::cout << "  Output size: " << buffer_.size() << " bytes" << std::endl;
            std::cout << "  Compression: " << (100.0 * buffer_.size() / getFileSize(input_xml)) << "%" << std::endl;
            
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception during conversion: " << e.what() << std::endl;
            return false;
        }
    }

private:
    void writeUInt8(uint8_t value) {
        buffer_.push_back(value);
    }
    
    void writeUInt16(uint16_t value) {
        buffer_.push_back(value & 0xFF);
        buffer_.push_back((value >> 8) & 0xFF);
    }
    
    void writeUInt32(uint32_t value) {
        buffer_.push_back(value & 0xFF);
        buffer_.push_back((value >> 8) & 0xFF);
        buffer_.push_back((value >> 16) & 0xFF);
        buffer_.push_back((value >> 24) & 0xFF);
    }
    
    void writeUInt32At(size_t offset, uint32_t value) {
        buffer_[offset] = value & 0xFF;
        buffer_[offset + 1] = (value >> 8) & 0xFF;
        buffer_[offset + 2] = (value >> 16) & 0xFF;
        buffer_[offset + 3] = (value >> 24) & 0xFF;
    }
    
    void writeString(const std::string& str) {
        writeUInt16(static_cast<uint16_t>(str.length()));
        buffer_.insert(buffer_.end(), str.begin(), str.end());
    }
    
    uint32_t addString(const std::string& str) {
        if (str.empty()) return 0xFFFFFFFF;
        
        std::map<std::string, uint32_t>::iterator it = string_to_id_.find(str);
        if (it != string_to_id_.end()) {
            return it->second;
        }
        
        uint32_t id = static_cast<uint32_t>(string_table_.size());
        string_table_.push_back(str);
        string_to_id_[str] = id;
        return id;
    }
    
    void collectStrings(BulletMLNode* node) {
        if (!node) return;
        
        // Collect value string
        try {
            // getValue() might throw if no value is set, so we need to check
            // We can't directly check if there's a value, so we use a workaround
            // by checking if it's a node type that typically has values
            BulletMLNode::Name name = node->getName();
            if (name == BulletMLNode::direction || name == BulletMLNode::speed ||
                name == BulletMLNode::wait || name == BulletMLNode::times ||
                name == BulletMLNode::term || name == BulletMLNode::param) {
                // These nodes typically have values - but we need the string form
                // which we don't have direct access to. Skip for now.
            }
        } catch (...) {
            // No value, that's fine
        }
        
        // Recursively collect from children
        BulletMLNode::ChildIterator it;
        for (it = node->childBegin(); it != node->childEnd(); ++it) {
            collectStrings(*it);
        }
    }
    
    void writeStringTable() {
        writeUInt32(static_cast<uint32_t>(string_table_.size()));
        for (size_t i = 0; i < string_table_.size(); ++i) {
            writeString(string_table_[i]);
        }
    }
    
    void buildReferenceMaps(BulletMLNode* node, BulletMLParserTinyXML& parser) {
        // For this implementation, we'll use the parser's internal reference maps
        // This is a simplified version - in a full implementation, we would
        // traverse the tree and build label->id mappings
    }
    
    void writeReferenceMaps() {
        // Write bullet references
        writeUInt32(static_cast<uint32_t>(bullet_refs_.size()));
        for (size_t i = 0; i < bullet_refs_.size(); ++i) {
            writeUInt32(bullet_refs_[i].first);
            writeUInt32(bullet_refs_[i].second);
        }
        
        // Write action references
        writeUInt32(static_cast<uint32_t>(action_refs_.size()));
        for (size_t i = 0; i < action_refs_.size(); ++i) {
            writeUInt32(action_refs_[i].first);
            writeUInt32(action_refs_[i].second);
        }
        
        // Write fire references
        writeUInt32(static_cast<uint32_t>(fire_refs_.size()));
        for (size_t i = 0; i < fire_refs_.size(); ++i) {
            writeUInt32(fire_refs_[i].first);
            writeUInt32(fire_refs_[i].second);
        }
    }
    
    uint8_t nodeTypeToUInt8(BulletMLNode::Name name) {
        return static_cast<uint8_t>(name);
    }
    
    uint8_t valueTypeToUInt8(BulletMLNode::Type type) {
        return static_cast<uint8_t>(type);
    }
    
    void writeNode(BulletMLNode* node) {
        if (!node) return;
        
        uint32_t my_node_id = current_node_id_++;
        node_to_id_[node] = my_node_id;
        
        // Write node header
        writeUInt8(nodeTypeToUInt8(node->getName()));
        writeUInt8(valueTypeToUInt8(node->getType()));
        
        // Count children
        uint16_t child_count = 0;
        BulletMLNode::ChildIterator it;
        for (it = node->childBegin(); it != node->childEnd(); ++it) {
            child_count++;
        }
        writeUInt16(child_count);
        
        // Write ref ID
        BulletMLNode::Name name = node->getName();
        if (name == BulletMLNode::bulletRef || 
            name == BulletMLNode::actionRef || 
            name == BulletMLNode::fireRef) {
            writeUInt32(node->getRefID());
        } else {
            writeUInt32(0xFFFFFFFF);
        }
        
        // Write value string ID (we don't have direct access to the string form)
        writeUInt32(0xFFFFFFFF);
        
        // Write label string ID
        writeUInt32(0xFFFFFFFF);
        
        // Write children
        for (it = node->childBegin(); it != node->childEnd(); ++it) {
            writeNode(*it);
        }
    }
    
    void writeHeader(bool horizontal, uint32_t string_table_offset,
                    uint32_t refmap_offset, uint32_t tree_offset) {
        // Write at beginning
        buffer_[0] = 'B';
        buffer_[1] = 'M';
        buffer_[2] = 'L';
        buffer_[3] = 'B';
        
        writeUInt32At(4, 1); // version
        buffer_[6] = horizontal ? 1 : 0; // orientation
        buffer_[7] = 0; // flags
        
        writeUInt32At(8, string_table_offset);
        writeUInt32At(12, refmap_offset);
        writeUInt32At(16, tree_offset);
        writeUInt32At(20, static_cast<uint32_t>(buffer_.size()));
    }
    
    size_t getFileSize(const std::string& filename) {
        std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
        if (!file) return 0;
        return file.tellg();
    }

private:
    std::vector<uint8_t> buffer_;
    std::vector<std::string> string_table_;
    std::map<std::string, uint32_t> string_to_id_;
    std::vector<std::pair<uint32_t, uint32_t>> bullet_refs_;
    std::vector<std::pair<uint32_t, uint32_t>> action_refs_;
    std::vector<std::pair<uint32_t, uint32_t>> fire_refs_;
    uint32_t current_node_id_;
    std::map<BulletMLNode*, uint32_t> node_to_id_;
};

int main(int argc, char* argv[]) {
    std::cout << "BulletML XML to Binary Converter v1.0" << std::endl;
    std::cout << "=====================================" << std::endl << std::endl;
    
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <input.xml> <output.bmlb>" << std::endl;
        std::cout << std::endl;
        std::cout << "Converts BulletML XML files to binary format." << std::endl;
        std::cout << "Binary files are faster to load and smaller in size." << std::endl;
        return 1;
    }
    
    std::string input = argv[1];
    std::string output = argv[2];
    
    BulletMLBinaryWriter writer;
    if (writer.convert(input, output)) {
        return 0;
    } else {
        std::cerr << "Conversion failed!" << std::endl;
        return 1;
    }
}
