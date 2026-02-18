/*
 * test_xmlutil_unit.cpp - Unit tests for XMLUtil
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif
#include "test_framework.h"
#include "core/utility/XMLUtil.h"
#include <cstring>
#include <map>
#include <vector>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// ============================================================================
// XML Parsing Tests
// ============================================================================

class XMLParsingTest : public TestCase {
public:
    XMLParsingTest() : TestCase("XMLUtil::parseXML") {}

protected:
    void runTest() override {
        // Basic element parsing
        std::string xml1 = "<root>content</root>";
        XMLUtil::Element root1 = XMLUtil::parseXML(xml1);
        ASSERT_EQUALS("root", root1.name, "Root element name match");
        ASSERT_EQUALS("content", root1.content, "Content match");
        ASSERT_TRUE(root1.children.empty(), "No children");

        // Attributes parsing
        std::string xml2 = "<item id=\"1\" value='test' />";
        XMLUtil::Element root2 = XMLUtil::parseXML(xml2);
        ASSERT_EQUALS("item", root2.name, "Element name match");
        ASSERT_EQUALS("1", root2.attributes["id"], "Attribute id match");
        ASSERT_EQUALS("test", root2.attributes["value"], "Attribute value match");
        ASSERT_TRUE(root2.content.empty(), "Content empty for self-closing tag");

        // Nested elements
        std::string xml3 = "<library><track><title>Song</title></track></library>";
        XMLUtil::Element root3 = XMLUtil::parseXML(xml3);
        ASSERT_EQUALS("library", root3.name, "Root name match");
        ASSERT_EQUALS(1u, root3.children.size(), "One child (track)");

        const auto& track = root3.children[0];
        ASSERT_EQUALS("track", track.name, "Child name match");
        ASSERT_EQUALS(1u, track.children.size(), "Track has one child (title)");

        const auto& title = track.children[0];
        ASSERT_EQUALS("title", title.name, "Grandchild name match");
        ASSERT_EQUALS("Song", title.content, "Grandchild content match");

        // XML Declaration (should be skipped)
        std::string xml4 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><data>valid</data>";
        XMLUtil::Element root4 = XMLUtil::parseXML(xml4);
        ASSERT_EQUALS("data", root4.name, "Root name match after declaration");
        ASSERT_EQUALS("valid", root4.content, "Content match");

        // Whitespace handling
        std::string xml5 = "  <root> \n <child>  text  </child> \n </root>  ";
        XMLUtil::Element root5 = XMLUtil::parseXML(xml5);
        ASSERT_EQUALS("root", root5.name, "Root name match with whitespace");
        ASSERT_EQUALS(1u, root5.children.size(), "One child");
        ASSERT_EQUALS("text", root5.children[0].content, "Trimmed content match");
    }
};

// ============================================================================
// XML Error Handling Tests
// ============================================================================

class XMLErrorTest : public TestCase {
public:
    XMLErrorTest() : TestCase("XMLUtil::parseXML errors") {}

protected:
    void runTest() override {
        // Missing closing tag
        std::string xml1 = "<root>content";
        TestPatterns::assertThrows<std::runtime_error>([&]() {
            XMLUtil::parseXML(xml1);
        }, "Missing closing tag", "Should throw for missing closing tag");

        // Mismatched tags
        std::string xml2 = "<root></child>";
        TestPatterns::assertThrows<std::runtime_error>([&]() {
            XMLUtil::parseXML(xml2);
        }, "Missing closing tag", "Should throw for mismatched tags");

        // Invalid start
        std::string xml3 = "not xml";
        TestPatterns::assertThrows<std::runtime_error>([&]() {
            XMLUtil::parseXML(xml3);
        }, "Expected '<'", "Should throw for invalid start");

        // Empty string (should throw or handle gracefully? parseElement throws if pos >= length)
        std::string xml4 = "";
        TestPatterns::assertThrows<std::runtime_error>([&]() {
            XMLUtil::parseXML(xml4);
        }, "Expected '<'", "Should throw for empty string");
    }
};

// ============================================================================
// XML Generation Tests
// ============================================================================

class XMLGenerationTest : public TestCase {
public:
    XMLGenerationTest() : TestCase("XMLUtil::generateXML") {}

protected:
    void runTest() override {
        // Simple element
        XMLUtil::Element el("test", "content");
        std::string xml1 = XMLUtil::generateXML(el);
        ASSERT_EQUALS("<test>content</test>", xml1, "Simple generation");

        // With attributes
        el.attributes["id"] = "123";
        std::string xml2 = XMLUtil::generateXML(el);
        // Attributes order in map is alphabetical/sorted by key
        ASSERT_EQUALS("<test id=\"123\">content</test>", xml2, "Generation with attributes");

        // Nested with indentation
        XMLUtil::Element parent("parent");
        XMLUtil::Element child("child", "value");
        parent.children.push_back(child);

        std::string xml3 = XMLUtil::generateXML(parent, 0);
        // Expect indentation (newlines and spaces)
        // <parent>
        //   <child>value</child>
        // </parent>
        // Note: Implementation details of newlines might vary, checking containment or strict string match
        // Based on read code:
        // xml << ">\n"; for children
        // xml << generateXML(child, indent + 1) << "\n";
        // xml << indentStr << "</" << element.name << ">";

        // Let's check contains child
        ASSERT_TRUE(xml3.find("<parent>") != std::string::npos, "Contains parent start");
        ASSERT_TRUE(xml3.find("<child>value</child>") != std::string::npos, "Contains child");
        ASSERT_TRUE(xml3.find("</parent>") != std::string::npos, "Contains parent end");

        // Self-closing
        XMLUtil::Element selfClosing("empty");
        std::string xml4 = XMLUtil::generateXML(selfClosing);
        ASSERT_EQUALS("<empty/>", xml4, "Self-closing generation");
    }
};

// ============================================================================
// XML Helper Tests
// ============================================================================

class XMLHelperTest : public TestCase {
public:
    XMLHelperTest() : TestCase("XMLUtil helpers") {}

protected:
    void runTest() override {
        XMLUtil::Element root("root");
        XMLUtil::Element child1("child", "text1");
        XMLUtil::Element child2("child", "text2");
        XMLUtil::Element other("other", "text3");

        root.children.push_back(child1);
        root.children.push_back(other);
        root.children.push_back(child2);

        // getChildText
        ASSERT_EQUALS("text1", XMLUtil::getChildText(root, "child"), "Get first child text");
        ASSERT_EQUALS("text3", XMLUtil::getChildText(root, "other"), "Get other child text");
        ASSERT_EQUALS("", XMLUtil::getChildText(root, "missing"), "Get missing child text (empty)");

        // findChild
        const auto* found = XMLUtil::findChild(root, "other");
        ASSERT_NOT_NULL(found, "Find existing child");
        if (found) {
            ASSERT_EQUALS("text3", found->content, "Found correct child");
        }

        ASSERT_NULL(XMLUtil::findChild(root, "missing"), "Find missing child returns null");

        // findChildren
        auto children = XMLUtil::findChildren(root, "child");
        ASSERT_EQUALS(2u, children.size(), "Find all children with name 'child'");
        if (children.size() >= 2) {
            ASSERT_EQUALS("text1", children[0]->content, "First child content");
            ASSERT_EQUALS("text2", children[1]->content, "Second child content");
        }
    }
};

// ============================================================================
// XML Escaping Tests
// ============================================================================

class XMLEscapingTest : public TestCase {
public:
    XMLEscapingTest() : TestCase("XMLUtil escaping") {}

protected:
    void runTest() override {
        // Escape
        ASSERT_EQUALS("&lt;tag&gt;", XMLUtil::escapeXML("<tag>"), "Escape tags");
        ASSERT_EQUALS("&amp;", XMLUtil::escapeXML("&"), "Escape ampersand");
        ASSERT_EQUALS("&quot;val&quot;", XMLUtil::escapeXML("\"val\""), "Escape quotes");
        ASSERT_EQUALS("&apos;val&apos;", XMLUtil::escapeXML("'val'"), "Escape apostrophe");

        // Unescape
        ASSERT_EQUALS("<tag>", XMLUtil::unescapeXML("&lt;tag&gt;"), "Unescape tags");
        ASSERT_EQUALS("&", XMLUtil::unescapeXML("&amp;"), "Unescape ampersand");
        ASSERT_EQUALS("\"val\"", XMLUtil::unescapeXML("&quot;val&quot;"), "Unescape quotes");
        ASSERT_EQUALS("'val'", XMLUtil::unescapeXML("&apos;val&apos;"), "Unescape apostrophe");

        // Round trip
        std::string original = "< & \" ' >";
        std::string escaped = XMLUtil::escapeXML(original);
        std::string unescaped = XMLUtil::unescapeXML(escaped);
        ASSERT_EQUALS(original, unescaped, "Round trip escaping");
    }
};

// ============================================================================
// Test Registration
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("XMLUtil Unit Tests");

    suite.addTest(std::make_unique<XMLParsingTest>());
    suite.addTest(std::make_unique<XMLErrorTest>());
    suite.addTest(std::make_unique<XMLGenerationTest>());
    suite.addTest(std::make_unique<XMLHelperTest>());
    suite.addTest(std::make_unique<XMLEscapingTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
