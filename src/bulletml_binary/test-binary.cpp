/// BulletML Binary Format Test Program
/**
 * Simple test program to verify binary BulletML parsing
 */

#include "bulletmlparser_binary.hpp"
#include "../bulletml/bulletmltree.h"
#include <iostream>
#include <cstdlib>

void printNode(BulletMLNode* node, int indent = 0) {
    if (!node) return;
    
    for (int i = 0; i < indent; ++i) std::cout << "  ";
    
    std::cout << "Node: ";
    
    switch (node->getName()) {
        case BulletMLNode::bulletml: std::cout << "bulletml"; break;
        case BulletMLNode::bullet: std::cout << "bullet"; break;
        case BulletMLNode::action: std::cout << "action"; break;
        case BulletMLNode::fire: std::cout << "fire"; break;
        case BulletMLNode::changeDirection: std::cout << "changeDirection"; break;
        case BulletMLNode::changeSpeed: std::cout << "changeSpeed"; break;
        case BulletMLNode::accel: std::cout << "accel"; break;
        case BulletMLNode::wait: std::cout << "wait"; break;
        case BulletMLNode::repeat: std::cout << "repeat"; break;
        case BulletMLNode::bulletRef: std::cout << "bulletRef"; break;
        case BulletMLNode::actionRef: std::cout << "actionRef"; break;
        case BulletMLNode::fireRef: std::cout << "fireRef"; break;
        case BulletMLNode::vanish: std::cout << "vanish"; break;
        case BulletMLNode::term: std::cout << "term"; break;
        case BulletMLNode::times: std::cout << "times"; break;
        case BulletMLNode::direction: std::cout << "direction"; break;
        case BulletMLNode::speed: std::cout << "speed"; break;
        case BulletMLNode::param: std::cout << "param"; break;
        default: std::cout << "unknown"; break;
    }
    
    if (node->getType() != BulletMLNode::none) {
        std::cout << " (type=";
        switch (node->getType()) {
            case BulletMLNode::aim: std::cout << "aim"; break;
            case BulletMLNode::absolute: std::cout << "absolute"; break;
            case BulletMLNode::relative: std::cout << "relative"; break;
            case BulletMLNode::sequence: std::cout << "sequence"; break;
            default: break;
        }
        std::cout << ")";
    }
    
    std::cout << std::endl;
    
    // Print children
    BulletMLNode::ChildIterator it;
    for (it = node->childBegin(); it != node->childEnd(); ++it) {
        printNode(*it, indent + 1);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "BulletML Binary Format Test Program" << std::endl;
    std::cout << "====================================" << std::endl << std::endl;
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file.bmlb>" << std::endl;
        std::cout << std::endl;
        std::cout << "Loads and displays the structure of a binary BulletML file." << std::endl;
        return 1;
    }
    
    try {
        std::string filename = argv[1];
        std::cout << "Loading: " << filename << std::endl << std::endl;
        
        // Create parser
        BulletMLParserBin parser(filename);
        
        // Parse the file
        parser.build();
        
        std::cout << "Parse successful!" << std::endl;
        std::cout << "Orientation: " << (parser.isHorizontal() ? "horizontal" : "vertical") << std::endl;
        std::cout << "Top actions: " << parser.getTopActions().size() << std::endl;
        std::cout << std::endl;
        
        std::cout << "Tree structure:" << std::endl;
        std::cout << "---------------" << std::endl;
        
        // Print tree structure (get root from first top action)
        if (parser.getTopActions().size() > 0) {
            BulletMLNode* root = parser.getTopActions()[0];
            while (root && root->getParent()) {
                root = root->getParent();
            }
            printNode(root);
        }
        
        std::cout << std::endl;
        std::cout << "Test completed successfully!" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
