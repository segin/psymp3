/*
 * test_xmlutil.cpp - Unit tests for XMLUtil
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
        // Basic element
        std::string basic = "<test>content</test>";
        auto root = XMLUtil::parseXML(basic);
        ASSERT_EQUALS("test", root.name, "Root element name");
        ASSERT_EQUALS("content", root.content, "Root element content");
        ASSERT_TRUE(root.children.empty(), "No children");
        ASSERT_TRUE(root.attributes.empty(), "No attributes");

        // Attributes
        std::string withAttr = "<test attr=\"value\" num='123'>content</test>";
        root = XMLUtil::parseXML(withAttr);
        ASSERT_EQUALS("test", root.name, "Root element name");
        ASSERT_EQUALS("content", root.content, "Root element content");
        ASSERT_EQUALS(2u, root.attributes.size(), "Two attributes");
        ASSERT_EQUALS("value", root.attributes["attr"], "Attribute 1 value");
        ASSERT_EQUALS("123", root.attributes["num"], "Attribute 2 value");

        // Self-closing
        std::string selfClosing = "<test attr=\"value\" />";
        root = XMLUtil::parseXML(selfClosing);
        ASSERT_EQUALS("test", root.name, "Root element name");
        ASSERT_TRUE(root.content.empty(), "No content");
        ASSERT_EQUALS(1u, root.attributes.size(), "One attribute");

        // Nested elements
        std::string nested = "<root><child1>text1</child1><child2 attr='val'>text2</child2></root>";
        root = XMLUtil::parseXML(nested);
        ASSERT_EQUALS("root", root.name, "Root name");
        ASSERT_EQUALS(2u, root.children.size(), "Two children");

        const auto& child1 = root.children[0];
        ASSERT_EQUALS("child1", child1.name, "Child 1 name");
        ASSERT_EQUALS("text1", child1.content, "Child 1 content");

        const auto& child2 = root.children[1];
        ASSERT_EQUALS("child2", child2.name, "Child 2 name");
        ASSERT_EQUALS("text2", child2.content, "Child 2 content");
        ASSERT_EQUALS("val", child2.attributes.at("attr"), "Child 2 attribute");

        // XML Declaration
        std::string withDecl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root>content</root>";
        root = XMLUtil::parseXML(withDecl);
        ASSERT_EQUALS("root", root.name, "Root name (skipping declaration)");
        ASSERT_EQUALS("content", root.content, "Content (skipping declaration)");

        // Whitespace handling
        std::string whitespace = "  <root>  \n  <child>  text  </child>  \n  </root>  ";
        root = XMLUtil::parseXML(whitespace);
        ASSERT_EQUALS("root", root.name, "Root name (whitespace)");
        ASSERT_EQUALS(1u, root.children.size(), "One child (whitespace)");
        ASSERT_EQUALS("text", root.children[0].content, "Child content (trimmed)");
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
        XMLUtil::Element root("root");
        root.attributes["attr"] = "value";

        XMLUtil::Element child1("child", "content");
        root.children.push_back(child1);

        XMLUtil::Element child2("child");
        child2.attributes["id"] = "2";
        root.children.push_back(child2);

        std::string xml = XMLUtil::generateXML(root);

        // Basic checks (exact formatting might vary, but key parts should exist)
        ASSERT_TRUE(xml.find("<root") != std::string::npos, "Root tag found");
        ASSERT_TRUE(xml.find("attr=\"value\"") != std::string::npos, "Attribute found");
        ASSERT_TRUE(xml.find("<child>content</child>") != std::string::npos, "Child 1 found");
        ASSERT_TRUE(xml.find("<child id=\"2\"/>") != std::string::npos, "Child 2 (self-closing) found");
        ASSERT_TRUE(xml.find("</root>") != std::string::npos, "Closing tag found");
    }
};

// ============================================================================
// XML Utility Methods Tests
// ============================================================================

class XMLUtilityMethodsTest : public TestCase {
public:
    XMLUtilityMethodsTest() : TestCase("XMLUtil::utilityMethods") {}

protected:
    void runTest() override {
        XMLUtil::Element root("root");
        root.children.push_back(XMLUtil::Element("child1", "content1"));
        root.children.push_back(XMLUtil::Element("child2", "content2"));
        root.children.push_back(XMLUtil::Element("child2", "content3")); // Duplicate name

        // getChildText
        ASSERT_EQUALS("content1", XMLUtil::getChildText(root, "child1"), "Get child text");
        ASSERT_EQUALS("", XMLUtil::getChildText(root, "nonexistent"), "Get nonexistent child text");

        // findChild
        const auto* child = XMLUtil::findChild(root, "child1");
        ASSERT_NOT_NULL(child, "Find existing child");
        ASSERT_EQUALS("content1", child->content, "Found child content");

        const auto* missing = XMLUtil::findChild(root, "nonexistent");
        ASSERT_NULL(missing, "Find nonexistent child");

        // findChildren
        auto children = XMLUtil::findChildren(root, "child2");
        ASSERT_EQUALS(2u, children.size(), "Find multiple children");
        ASSERT_EQUALS("content2", children[0]->content, "First child content");
        ASSERT_EQUALS("content3", children[1]->content, "Second child content");
    }
};

// ============================================================================
// XML Escaping Tests
// ============================================================================

class XMLEscapingTest : public TestCase {
public:
    XMLEscapingTest() : TestCase("XMLUtil::escaping") {}

protected:
    void runTest() override {
        // escapeXML
        ASSERT_EQUALS("&lt;test&gt;", XMLUtil::escapeXML("<test>"), "Escape tags");
        ASSERT_EQUALS("&quot;&apos;", XMLUtil::escapeXML("\"\'"), "Escape quotes");
        ASSERT_EQUALS("&amp;", XMLUtil::escapeXML("&"), "Escape ampersand");
        ASSERT_EQUALS("Normal text", XMLUtil::escapeXML("Normal text"), "Escape normal text");

        // unescapeXML
        ASSERT_EQUALS("<test>", XMLUtil::unescapeXML("&lt;test&gt;"), "Unescape tags");
        ASSERT_EQUALS("\"\'", XMLUtil::unescapeXML("&quot;&apos;"), "Unescape quotes");
        ASSERT_EQUALS("&", XMLUtil::unescapeXML("&amp;"), "Unescape ampersand");
        ASSERT_EQUALS("Normal text", XMLUtil::unescapeXML("Normal text"), "Unescape normal text");

        // Round trip
        std::string original = "<>&\"'";
        ASSERT_EQUALS(original, XMLUtil::unescapeXML(XMLUtil::escapeXML(original)), "Round trip escaping");
    }
};

// ============================================================================
// XML Parsing Error Tests
// ============================================================================

class XMLParsingErrorTest : public TestCase {
public:
    XMLParsingErrorTest() : TestCase("XMLUtil::parsingErrors") {}

protected:
    void runTest() override {
        // Missing closing tag
        std::string missingClosing = "<root><child>content</root>";
        TestFramework::TestPatterns::assertThrows<std::runtime_error>(
            [&]() { XMLUtil::parseXML(missingClosing); },
            "Missing closing tag",
            "Should throw on missing closing tag"
        );

        // Mismatched tags
        std::string mismatched = "<root>content</other>";
        TestFramework::TestPatterns::assertThrows<std::runtime_error>(
            [&]() { XMLUtil::parseXML(mismatched); },
            "Missing closing tag", // Implementation typically reports missing expected tag
            "Should throw on mismatched tags"
        );

        // Invalid start
        std::string invalidStart = "not xml";
        TestFramework::TestPatterns::assertThrows<std::runtime_error>(
            [&]() { XMLUtil::parseXML(invalidStart); },
            "Expected '<'",
            "Should throw on invalid start"
        );

        // Unclosed tag
        std::string unclosedTag = "<root";
        TestFramework::TestPatterns::assertThrows<std::runtime_error>(
            [&]() { XMLUtil::parseXML(unclosedTag); },
            "Unclosed tag",
            "Should throw on unclosed tag definition"
        );
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
    suite.addTest(std::make_unique<XMLGenerationTest>());
    suite.addTest(std::make_unique<XMLUtilityMethodsTest>());
    suite.addTest(std::make_unique<XMLEscapingTest>());
    suite.addTest(std::make_unique<XMLParsingErrorTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
