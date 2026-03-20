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
#include <string>

using namespace PsyMP3::Core::Utility;
using namespace TestFramework;

// ============================================================================
// Simple Parsing Tests
// ============================================================================

class SimpleParsingTest : public TestCase {
public:
    SimpleParsingTest() : TestCase("XMLUtil::SimpleParsing") {}

protected:
    void runTest() override {
        // Simple element
        XMLUtil::Element root = XMLUtil::parseXML("<root>content</root>");
        ASSERT_EQUALS("root", root.name, "Root name correct");
        ASSERT_EQUALS("content", root.content, "Root content correct");
        ASSERT_TRUE(root.children.empty(), "No children");

        // With XML declaration
        root = XMLUtil::parseXML("<?xml version=\"1.0\"?><root>content</root>");
        ASSERT_EQUALS("root", root.name, "Root name correct with declaration");
        ASSERT_EQUALS("content", root.content, "Root content correct with declaration");

        // Self-closing
        root = XMLUtil::parseXML("<root/>");
        ASSERT_EQUALS("root", root.name, "Self-closing root name correct");
        ASSERT_TRUE(root.content.empty(), "Self-closing content empty");

        // Whitespace handling
        root = XMLUtil::parseXML("  \t<root>  content  </root>  ");
        ASSERT_EQUALS("root", root.name, "Root name correct with whitespace");
        ASSERT_EQUALS("content", root.content, "Content trimmed");
    }
};

// ============================================================================
// Attribute Parsing Tests
// ============================================================================

class AttributeParsingTest : public TestCase {
public:
    AttributeParsingTest() : TestCase("XMLUtil::AttributeParsing") {}

protected:
    void runTest() override {
        // Single attribute
        XMLUtil::Element root = XMLUtil::parseXML("<root attr=\"value\"/>");
        ASSERT_EQUALS("value", root.attributes["attr"], "Attribute value correct");

        // Multiple attributes
        root = XMLUtil::parseXML("<root a=\"1\" b='2' c=3/>");
        ASSERT_EQUALS("1", root.attributes["a"], "Attribute a correct");
        ASSERT_EQUALS("2", root.attributes["b"], "Attribute b correct");
        ASSERT_EQUALS("3", root.attributes["c"], "Attribute c correct (unquoted)");

        // Attributes with whitespace
        root = XMLUtil::parseXML("<root  attr1 = \"val1\"  attr2='val2' />");
        ASSERT_EQUALS("val1", root.attributes["attr1"], "Attribute 1 correct with whitespace");
        ASSERT_EQUALS("val2", root.attributes["attr2"], "Attribute 2 correct with whitespace");

        // Attributes with special characters
        root = XMLUtil::parseXML("<root attr=\"&lt;&quot;&amp;&gt;\"/>");
        ASSERT_EQUALS("<\"&>", root.attributes["attr"], "Attribute unescaped correctly");
    }
};

// ============================================================================
// Nested XML Parsing Tests
// ============================================================================

class NestedXMLTest : public TestCase {
public:
    NestedXMLTest() : TestCase("XMLUtil::NestedXML") {}

protected:
    void runTest() override {
        // Nested children
        std::string xml = "<root><child1>text1</child1><child2 attr=\"val\">text2</child2></root>";
        XMLUtil::Element root = XMLUtil::parseXML(xml);

        ASSERT_EQUALS("root", root.name, "Root name");
        ASSERT_EQUALS(2u, root.children.size(), "Two children");

        ASSERT_EQUALS("child1", root.children[0].name, "Child 1 name");
        ASSERT_EQUALS("text1", root.children[0].content, "Child 1 content");

        ASSERT_EQUALS("child2", root.children[1].name, "Child 2 name");
        ASSERT_EQUALS("text2", root.children[1].content, "Child 2 content");
        ASSERT_EQUALS("val", root.children[1].attributes["attr"], "Child 2 attribute");

        // Deep nesting
        xml = "<A><B><C>content</C></B></A>";
        root = XMLUtil::parseXML(xml);
        ASSERT_EQUALS("A", root.name, "A name");
        ASSERT_EQUALS(1u, root.children.size(), "A has 1 child");
        ASSERT_EQUALS("B", root.children[0].name, "B name");
        ASSERT_EQUALS(1u, root.children[0].children.size(), "B has 1 child");
        ASSERT_EQUALS("C", root.children[0].children[0].name, "C name");
        ASSERT_EQUALS("content", root.children[0].children[0].content, "C content");

        // Nested tags with SAME NAME - This is expected to fail with current implementation
        xml = "<item><item>inner</item></item>";
        root = XMLUtil::parseXML(xml);
        ASSERT_EQUALS("item", root.name, "Outer item name");
        ASSERT_EQUALS(1u, root.children.size(), "Outer item should have 1 child");
        ASSERT_EQUALS("item", root.children[0].name, "Inner item name");
        ASSERT_EQUALS("inner", root.children[0].content, "Inner item content");

        // Nested tags with same name and content after - stronger test for the bug
        xml = "<item><item>inner</item>outer</item>";
        root = XMLUtil::parseXML(xml);
        ASSERT_EQUALS("item", root.name, "Outer item name");
        ASSERT_EQUALS(1u, root.children.size(), "Outer item child count");
        ASSERT_EQUALS("inner", root.children[0].content, "Inner content");
        // This will fail if the parser consumes the first </item> for both inner and outer
        ASSERT_EQUALS("outer", root.content, "Outer content");
    }
};

// ============================================================================
// Helper Function Tests
// ============================================================================

class HelperFunctionTest : public TestCase {
public:
    HelperFunctionTest() : TestCase("XMLUtil::HelperFunctions") {}

protected:
    void runTest() override {
        // escapeXML
        ASSERT_EQUALS("&lt;root&gt;", XMLUtil::escapeXML("<root>"), "Escape tags");
        ASSERT_EQUALS("&quot;&apos;&amp;", XMLUtil::escapeXML("\"'&"), "Escape special chars");

        // unescapeXML
        ASSERT_EQUALS("<root>", XMLUtil::unescapeXML("&lt;root&gt;"), "Unescape tags");
        ASSERT_EQUALS("\"'&", XMLUtil::unescapeXML("&quot;&apos;&amp;"), "Unescape special chars");

        // findChild / getChildText
        XMLUtil::Element root("root");
        XMLUtil::Element child1("child1", "content1");
        XMLUtil::Element child2("child2", "content2");
        root.children.push_back(child1);
        root.children.push_back(child2);

        const XMLUtil::Element* found = XMLUtil::findChild(root, "child1");
        ASSERT_NOT_NULL(found, "Found child1");
        ASSERT_EQUALS("content1", found->content, "Child1 content matches");

        ASSERT_EQUALS("content2", XMLUtil::getChildText(root, "child2"), "Get child2 text");
        ASSERT_EQUALS("", XMLUtil::getChildText(root, "nonexistent"), "Get nonexistent text empty");

        // findChildren
        XMLUtil::Element item1("item", "1");
        XMLUtil::Element item2("item", "2");
        root.children.push_back(item1);
        root.children.push_back(item2);

        auto items = XMLUtil::findChildren(root, "item");
        ASSERT_EQUALS(2u, items.size(), "Found 2 items");
        ASSERT_EQUALS("1", items[0]->content, "Item 1 content");
        ASSERT_EQUALS("2", items[1]->content, "Item 2 content");
    }
};

// ============================================================================
// Generation Tests
// ============================================================================

class GenerationTest : public TestCase {
public:
    GenerationTest() : TestCase("XMLUtil::Generation") {}

protected:
    void runTest() override {
        XMLUtil::Element root("root");
        root.attributes["attr"] = "val";
        XMLUtil::Element child("child", "content");
        root.children.push_back(child);

        std::string xml = XMLUtil::generateXML(root);
        // Note: Attribute order is not guaranteed, but with 1 attribute it is deterministic
        // Also indentation might vary

        // We verify by parsing back
        XMLUtil::Element parsed = XMLUtil::parseXML(xml);
        ASSERT_EQUALS("root", parsed.name, "Parsed root name");
        ASSERT_EQUALS("val", parsed.attributes["attr"], "Parsed attribute");
        ASSERT_EQUALS(1u, parsed.children.size(), "Parsed children count");
        ASSERT_EQUALS("child", parsed.children[0].name, "Parsed child name");
        ASSERT_EQUALS("content", parsed.children[0].content, "Parsed child content");
    }
};

// ============================================================================
// Error Handling Tests
// ============================================================================

class ErrorHandlingTest : public TestCase {
public:
    ErrorHandlingTest() : TestCase("XMLUtil::ErrorHandling") {}

protected:
    void runTest() override {
        // Unclosed tag (missing >)
        TestPatterns::assertThrows<std::runtime_error>([]() {
            XMLUtil::parseXML("<root");
        }, "Unclosed tag", "Should throw for unclosed opening tag");

        // Missing closing tag
        TestPatterns::assertThrows<std::runtime_error>([]() {
            XMLUtil::parseXML("<root>content");
        }, "Missing closing tag", "Should throw for missing closing tag");

        // Mismatched closing tag
        TestPatterns::assertThrows<std::runtime_error>([]() {
            XMLUtil::parseXML("<root><child></root>");
        }, "Unexpected closing tag", "Should throw for mismatched closing tag");

        // Missing closing tag (EOF)
        TestPatterns::assertThrows<std::runtime_error>([]() {
            XMLUtil::parseXML("<root>content");
        }, "Missing closing tag", "Should throw for missing closing tag at EOF");

        // Bad start
        TestPatterns::assertThrows<std::runtime_error>([]() {
            XMLUtil::parseXML("not xml");
        }, "Expected '<'", "Should throw for non-XML start");
    }
};

// ============================================================================
// Test Registration
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    TestSuite suite("XMLUtil Unit Tests");

    suite.addTest(std::make_unique<SimpleParsingTest>());
    suite.addTest(std::make_unique<AttributeParsingTest>());
    suite.addTest(std::make_unique<NestedXMLTest>());
    suite.addTest(std::make_unique<HelperFunctionTest>());
    suite.addTest(std::make_unique<GenerationTest>());
    suite.addTest(std::make_unique<ErrorHandlingTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
