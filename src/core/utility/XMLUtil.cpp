/*
 * XMLUtil.cpp - Simple XML utility class implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif

#include "core/utility/XMLUtil.h"
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <algorithm>

namespace PsyMP3 {
namespace Core {
namespace Utility {

XMLUtil::Element XMLUtil::parseXML(const std::string& xml) {
    size_t pos = 0;
    skipWhitespace(xml, pos);
    
    // Skip XML declaration if present
    if (pos < xml.length() && xml.substr(pos, 5) == "<?xml") {
        size_t end = xml.find("?>", pos);
        if (end != std::string::npos) {
            pos = end + 2;
            skipWhitespace(xml, pos);
        }
    }
    
    return parseElement(xml, pos);
}

std::string XMLUtil::generateXML(const Element& element, int indent) {
    std::ostringstream xml;
    std::string indentStr = getIndent(indent);
    
    xml << indentStr << "<" << element.name;
    
    // Add attributes
    for (const auto& attr : element.attributes) {
        xml << " " << attr.first << "=\"" << escapeXML(attr.second) << "\"";
    }
    
    if (element.children.empty() && element.content.empty()) {
        // Self-closing tag
        xml << "/>";
    } else {
        xml << ">";
        
        if (!element.content.empty()) {
            xml << escapeXML(element.content);
        }
        
        if (!element.children.empty()) {
            xml << "\n";
            for (const auto& child : element.children) {
                xml << generateXML(child, indent + 1) << "\n";
            }
            xml << indentStr;
        }
        
        xml << "</" << element.name << ">";
    }
    
    return xml.str();
}

std::string XMLUtil::getChildText(const Element& parent, const std::string& childName) {
    const Element* child = findChild(parent, childName);
    return child ? child->content : "";
}

const XMLUtil::Element* XMLUtil::findChild(const Element& parent, const std::string& childName) {
    for (const auto& child : parent.children) {
        if (child.name == childName) {
            return &child;
        }
    }
    return nullptr;
}

std::vector<const XMLUtil::Element*> XMLUtil::findChildren(const Element& parent, const std::string& childName) {
    std::vector<const Element*> result;
    for (const auto& child : parent.children) {
        if (child.name == childName) {
            result.push_back(&child);
        }
    }
    return result;
}

std::string XMLUtil::escapeXML(const std::string& text) {
    std::string result;
    result.reserve(text.length() * 1.1); // Slight overallocation
    
    for (char c : text) {
        switch (c) {
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '&':  result += "&amp;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c; break;
        }
    }
    
    return result;
}

std::string XMLUtil::unescapeXML(const std::string& text) {
    std::string result = text;
    
    // Replace entities (order matters for &amp;)
    size_t pos = 0;
    while ((pos = result.find("&lt;", pos)) != std::string::npos) {
        result.replace(pos, 4, "<");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = result.find("&gt;", pos)) != std::string::npos) {
        result.replace(pos, 4, ">");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = result.find("&quot;", pos)) != std::string::npos) {
        result.replace(pos, 6, "\"");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = result.find("&apos;", pos)) != std::string::npos) {
        result.replace(pos, 6, "'");
        pos += 1;
    }
    
    // &amp; must be last to avoid double-unescaping
    pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
        result.replace(pos, 5, "&");
        pos += 1;
    }
    
    return result;
}

XMLUtil::Element XMLUtil::parseElement(const std::string& xml, size_t& pos) {
    skipWhitespace(xml, pos);
    
    if (pos >= xml.length() || xml[pos] != '<') {
        throw std::runtime_error("Expected '<' at position " + std::to_string(pos));
    }
    
    pos++; // Skip '<'
    
    // Find end of opening tag
    size_t tagEnd = xml.find('>', pos);
    if (tagEnd == std::string::npos) {
        throw std::runtime_error("Unclosed tag starting at position " + std::to_string(pos - 1));
    }
    
    std::string tagContent = xml.substr(pos, tagEnd - pos);
    pos = tagEnd + 1;
    
    // Check for self-closing tag
    bool selfClosing = false;
    if (!tagContent.empty() && tagContent.back() == '/') {
        selfClosing = true;
        tagContent.pop_back();
    }
    
    // Parse tag name and attributes
    size_t spacePos = tagContent.find(' ');
    std::string tagName = (spacePos == std::string::npos) ? 
                          tagContent : tagContent.substr(0, spacePos);
    
    Element element(tagName);
    
    // Parse attributes if present
    if (spacePos != std::string::npos) {
        std::string attributeString = tagContent.substr(spacePos + 1);
        element.attributes = parseAttributes(attributeString);
    }
    
    if (selfClosing) {
        return element;
    }
    
    // Parse content and children
    std::string closingTag = "</" + tagName + ">";
    size_t closingPos = xml.find(closingTag, pos);
    
    if (closingPos == std::string::npos) {
        throw std::runtime_error("Missing closing tag for: " + tagName);
    }
    
    // Parse content between opening and closing tags
    while (pos < closingPos) {
        skipWhitespace(xml, pos);
        
        if (pos >= closingPos) break;
        
        if (xml[pos] == '<') {
            // Child element
            element.children.push_back(parseElement(xml, pos));
        } else {
            // Text content
            size_t textEnd = xml.find('<', pos);
            if (textEnd == std::string::npos || textEnd > closingPos) {
                textEnd = closingPos;
            }
            
            std::string text = xml.substr(pos, textEnd - pos);
            // Trim whitespace from text content
            size_t start = text.find_first_not_of(" \t\r\n");
            size_t end = text.find_last_not_of(" \t\r\n");
            
            if (start != std::string::npos && end != std::string::npos) {
                element.content += unescapeXML(text.substr(start, end - start + 1));
            }
            
            pos = textEnd;
        }
    }
    
    pos = closingPos + closingTag.length();
    return element;
}

void XMLUtil::skipWhitespace(const std::string& xml, size_t& pos) {
    while (pos < xml.length() && std::isspace(xml[pos])) {
        pos++;
    }
}

std::map<std::string, std::string> XMLUtil::parseAttributes(const std::string& attributeString) {
    std::map<std::string, std::string> attributes;
    size_t pos = 0;
    
    while (pos < attributeString.length()) {
        // Skip whitespace
        while (pos < attributeString.length() && std::isspace(attributeString[pos])) {
            pos++;
        }
        
        if (pos >= attributeString.length()) break;
        
        // Find attribute name
        size_t nameStart = pos;
        while (pos < attributeString.length() && attributeString[pos] != '=' && 
               !std::isspace(attributeString[pos])) {
            pos++;
        }
        
        if (pos >= attributeString.length()) break;
        
        std::string name = attributeString.substr(nameStart, pos - nameStart);
        
        // Skip whitespace and '='
        while (pos < attributeString.length() && 
               (std::isspace(attributeString[pos]) || attributeString[pos] == '=')) {
            pos++;
        }
        
        if (pos >= attributeString.length()) break;
        
        // Parse attribute value
        std::string value;
        if (attributeString[pos] == '"' || attributeString[pos] == '\'') {
            char quote = attributeString[pos];
            pos++; // Skip opening quote
            
            size_t valueStart = pos;
            while (pos < attributeString.length() && attributeString[pos] != quote) {
                pos++;
            }
            
            if (pos < attributeString.length()) {
                value = unescapeXML(attributeString.substr(valueStart, pos - valueStart));
                pos++; // Skip closing quote
            }
        } else {
            // Unquoted value (not recommended, but handle it)
            size_t valueStart = pos;
            while (pos < attributeString.length() && !std::isspace(attributeString[pos])) {
                pos++;
            }
            value = unescapeXML(attributeString.substr(valueStart, pos - valueStart));
        }
        
        attributes[name] = value;
    }
    
    return attributes;
}

std::string XMLUtil::getIndent(int level) {
    return std::string(level * 2, ' '); // 2 spaces per level
}

} // namespace Utility
} // namespace Core
} // namespace PsyMP3
