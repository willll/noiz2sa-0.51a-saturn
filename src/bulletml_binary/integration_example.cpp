/// Example: Integrating Binary BulletML into noiz2sa
/**
 * This example shows how to integrate the binary BulletML loader
 * into the existing noiz2sa game with minimal code changes.
 */

#include "../bulletml/bulletmlparser-tinyxml.h"
#include "bulletmlparser_binary.hpp"
#include <string>
#include <sys/stat.h>

/// Smart BulletML loader that automatically chooses the best format
class BulletMLLoader {
public:
    /// Load BulletML file by name (without extension)
    /// Tries binary first (.bmlb), falls back to XML (.xml)
    static BulletMLParser* load(const std::string& base_path, const std::string& name) {
        BulletMLParser* parser = nullptr;
        
        // Try binary first (faster)
        std::string binary_path = base_path + "/" + name + ".bmlb";
        if (fileExists(binary_path)) {
            parser = new BulletMLParserBin(binary_path);
            parser->build();
            return parser;
        }
        
        // Fall back to XML
        std::string xml_path = base_path + "/" + name + ".xml";
        if (fileExists(xml_path)) {
            parser = new BulletMLParserTinyXML(xml_path);
            parser->build();
            return parser;
        }
        
        return nullptr;
    }
    
    /// Load with explicit extension
    static BulletMLParser* loadExplicit(const std::string& filename) {
        BulletMLParser* parser = nullptr;
        
        // Check extension
        if (endsWith(filename, ".bmlb")) {
            parser = new BulletMLParserBin(filename);
        } else if (endsWith(filename, ".xml")) {
            parser = new BulletMLParserTinyXML(filename);
        } else {
            return nullptr;
        }
        
        parser->build();
        return parser;
    }

private:
    static bool fileExists(const std::string& filename) {
        struct stat buffer;
        return (stat(filename.c_str(), &buffer) == 0);
    }
    
    static bool endsWith(const std::string& str, const std::string& suffix) {
        if (str.length() < suffix.length()) {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }
};

/* INTEGRATION EXAMPLE
 * ===================
 * 
 * Before (in barragemanager.cc or similar):
 * 
 *   BulletMLParserTinyXML* parser = 
 *       new BulletMLParserTinyXML("noiz2sa_share/boss/57way.xml");
 *   parser->build();
 * 
 * After (Method 1 - Simple replacement):
 * 
 *   BulletMLParserBin* parser = 
 *       new BulletMLParserBin("noiz2sa_share/boss/57way.bmlb");
 *   parser->build();
 * 
 * After (Method 2 - Auto-detect):
 * 
 *   BulletMLParser* parser = 
 *       BulletMLLoader::load("noiz2sa_share/boss", "57way");
 * 
 * After (Method 3 - Explicit):
 * 
 *   BulletMLParser* parser = 
 *       BulletMLLoader::loadExplicit("noiz2sa_share/boss/57way.bmlb");
 */

/* BUILD PROCESS INTEGRATION
 * =========================
 * 
 * Add to your CMakeLists.txt:
 * 
 *   # Add binary bulletml directory
 *   add_subdirectory(src/bulletml_binary)
 * 
 *   # Link your executable with binary bulletml
 *   target_link_libraries(noiz2sa
 *       bulletml_binary
 *       # ... other libraries
 *   )
 * 
 *   # Add custom target to convert XML files at build time
 *   add_custom_target(convert_bulletml ALL
 *       COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/src/bulletml_binary/convert_all.sh
 *               ${CMAKE_CURRENT_SOURCE_DIR}/noiz2sa_share/boss
 *               ${CMAKE_CURRENT_BINARY_DIR}/noiz2sa_share/boss_binary
 *       DEPENDS bulletml-converter
 *       COMMENT "Converting BulletML XML to binary..."
 *   )
 */

/* DISTRIBUTION
 * ============
 * 
 * When distributing your game:
 * 
 * Option A: Binary only
 *   - Include only .bmlb files
 *   - Smallest distribution size
 *   - Best performance
 *   - Users cannot easily mod files
 * 
 * Option B: Both formats
 *   - Include both .xml and .bmlb files
 *   - Larger distribution size
 *   - Binary used by default (fast)
 *   - Users can mod XML files
 *   - Use BulletMLLoader::load() to prefer binary
 * 
 * Option C: XML with runtime conversion
 *   - Include only .xml files
 *   - Convert to binary on first run
 *   - Cache binary files for future runs
 *   - Best of both worlds, but more complex
 */

/* DEBUGGING TIPS
 * ==============
 * 
 * 1. Verify binary file:
 *    ./test-binary noiz2sa_share/boss/57way.bmlb
 * 
 * 2. Compare XML vs Binary:
 *    // Load both and compare results
 *    BulletMLParserTinyXML xml_parser("file.xml");
 *    BulletMLParserBin bin_parser("file.bmlb");
 *    xml_parser.build();
 *    bin_parser.build();
 *    // Both should produce identical behavior
 * 
 * 3. Re-convert if issues:
 *    ./bulletml-converter file.xml file.bmlb
 * 
 * 4. Check error messages:
 *    BulletMLParserBin parser("file.bmlb");
 *    parser.build();
 *    std::cout << parser.getErrorMessage() << std::endl;
 */

#ifdef EXAMPLE_MAIN
// Example main function showing usage
int main() {
    // Example 1: Load with auto-detection
    BulletMLParser* parser1 = BulletMLLoader::load("noiz2sa_share/boss", "57way");
    if (parser1) {
        std::cout << "Loaded 57way successfully" << std::endl;
        std::cout << "Orientation: " << (parser1->isHorizontal() ? "horizontal" : "vertical") << std::endl;
        std::cout << "Top actions: " << parser1->getTopActions().size() << std::endl;
        delete parser1;
    }
    
    // Example 2: Load explicit binary
    BulletMLParser* parser2 = BulletMLLoader::loadExplicit("noiz2sa_share/boss/57way.bmlb");
    if (parser2) {
        std::cout << "Loaded binary file successfully" << std::endl;
        delete parser2;
    }
    
    return 0;
}
#endif
