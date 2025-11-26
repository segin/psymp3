/*
 * XMLUtil.h - Simple XML utility class for parsing and generation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef XMLUTIL_H
#define XMLUTIL_H

namespace PsyMP3 {
namespace XML {

/**
 * @brief Simple XML utility class for basic parsing and generation
 * 
 * Provides lightweight XML functionality without external dependencies.
 * Designed for simple document structures like scrobble caching.
 */
class XMLUtil {
public:
    /**
     * @brief Simple XML element representation
     */
    struct Element {
        std::string name;
        std::string content;
        std::map<std::string, std::string> attributes;
        std::vector<Element> children;
        
        Element() = default;
        Element(const std::string& elementName) : name(elementName) {}
        Element(const std::string& elementName, const std::string& elementContent) 
            : name(elementName), content(elementContent) {}
    };
    
    /**
     * @brief Parse XML string into element tree
     * @param xml The XML string to parse
     * @return Root element of the parsed XML
     */
    static Element parseXML(const std::string& xml);
    
    /**
     * @brief Generate XML string from element tree
     * @param element The root element to serialize
     * @param indent Current indentation level (for pretty printing)
     * @return XML string representation
     */
    static std::string generateXML(const Element& element, int indent = 0);
    
    /**
     * @brief Extract text content from a named child element
     * @param parent The parent element to search in
     * @param childName The name of the child element to find
     * @return Text content of the child element, or empty string if not found
     */
    static std::string getChildText(const Element& parent, const std::string& childName);
    
    /**
     * @brief Find first child element with given name
     * @param parent The parent element to search in
     * @param childName The name of the child element to find
     * @return Pointer to child element, or nullptr if not found
     */
    static const Element* findChild(const Element& parent, const std::string& childName);
    
    /**
     * @brief Find all child elements with given name
     * @param parent The parent element to search in
     * @param childName The name of the child elements to find
     * @return Vector of pointers to matching child elements
     */
    static std::vector<const Element*> findChildren(const Element& parent, const std::string& childName);
    
    /**
     * @brief Escape XML special characters in text content
     * @param text The text to escape
     * @return Escaped text safe for XML content
     */
    static std::string escapeXML(const std::string& text);
    
    /**
     * @brief Unescape XML entities in text content
     * @param text The text to unescape
     * @return Unescaped text with entities converted back to characters
     */
    static std::string unescapeXML(const std::string& text);

private:
    /**
     * @brief Parse a single XML element from string
     * @param xml The XML string
     * @param pos Current position in string (modified during parsing)
     * @return Parsed element
     */
    static Element parseElement(const std::string& xml, size_t& pos);
    
    /**
     * @brief Skip whitespace characters
     * @param xml The XML string
     * @param pos Current position in string (modified)
     */
    static void skipWhitespace(const std::string& xml, size_t& pos);
    
    /**
     * @brief Parse element attributes
     * @param attributeString The attribute string to parse
     * @return Map of attribute name->value pairs
     */
    static std::map<std::string, std::string> parseAttributes(const std::string& attributeString);
    
    /**
     * @brief Generate indentation string
     * @param level The indentation level
     * @return String with appropriate indentation
     */
    static std::string getIndent(int level);
};

} // namespace XML
} // namespace PsyMP3

#endif // XMLUTIL_H
