/*
 * test_widget_hierarchy_properties.cpp - Property-based tests for widget hierarchy
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>
#endif

using namespace PsyMP3::Widget::Foundation;
using namespace TestFramework;

// ============================================================================
// Property-Based Tests using RapidCheck native API
// ============================================================================

#ifdef HAVE_RAPIDCHECK

bool runWidgetHierarchyPropertyTests() {
    bool all_passed = true;
    
    std::cout << "Running RapidCheck property-based tests for widget hierarchy...\n\n";
    
    // Property 1: Parent-child relationships are maintained correctly
    std::cout << "  WidgetHierarchy_ParentChildRelationships: ";
    auto result1 = rc::check("Parent-child relationships are maintained", []() {
        // Create a parent widget
        auto parent = std::make_unique<Widget>();
        
        // Add a child widget
        auto child = std::make_unique<Widget>();
        Widget* child_ptr = child.get();
        parent->addChild(std::move(child));
        
        // Verify parent pointer is set correctly
        RC_ASSERT(child_ptr->m_parent == parent.get());
    });
    if (!result1) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property 2: Children are added in order
    std::cout << "  WidgetHierarchy_ChildrenAddedInOrder: ";
    auto result2 = rc::check("Children are added in order", []() {
        auto parent = std::make_unique<Widget>();
        
        auto child1 = std::make_unique<Widget>();
        auto child2 = std::make_unique<Widget>();
        auto child3 = std::make_unique<Widget>();
        
        Widget* c1 = child1.get();
        Widget* c2 = child2.get();
        Widget* c3 = child3.get();
        
        parent->addChild(std::move(child1));
        parent->addChild(std::move(child2));
        parent->addChild(std::move(child3));
        
        // Verify children are in the correct order
        RC_ASSERT(parent->m_children.size() == 3);
        RC_ASSERT(parent->m_children[0].get() == c1);
        RC_ASSERT(parent->m_children[1].get() == c2);
        RC_ASSERT(parent->m_children[2].get() == c3);
    });
    if (!result2) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property 3: Child ownership is transferred
    std::cout << "  WidgetHierarchy_ChildOwnershipTransferred: ";
    auto result3 = rc::check("Child ownership is transferred", []() {
        auto parent = std::make_unique<Widget>();
        auto child = std::make_unique<Widget>();
        Widget* child_ptr = child.get();
        
        parent->addChild(std::move(child));
        
        // After move, child should still be accessible through parent
        RC_ASSERT(parent->m_children.size() == 1);
        RC_ASSERT(parent->m_children[0].get() == child_ptr);
    });
    if (!result3) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property 4: Widget hierarchy depth is correct
    std::cout << "  WidgetHierarchy_DepthCalculation: ";
    auto result4 = rc::check("Widget hierarchy depth is correct", []() {
        auto root = std::make_unique<Widget>();
        auto level1 = std::make_unique<Widget>();
        auto level2 = std::make_unique<Widget>();
        auto level3 = std::make_unique<Widget>();
        
        Widget* l1 = level1.get();
        Widget* l2 = level2.get();
        Widget* l3 = level3.get();
        
        root->addChild(std::move(level1));
        l1->addChild(std::move(level2));
        l2->addChild(std::move(level3));
        
        // Verify hierarchy depth
        RC_ASSERT(l1->m_parent == root.get());
        RC_ASSERT(l2->m_parent == l1);
        RC_ASSERT(l3->m_parent == l2);
        RC_ASSERT(l3->m_parent->m_parent == l1);
        RC_ASSERT(l3->m_parent->m_parent->m_parent == root.get());
    });
    if (!result4) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property 5: Widget hierarchy is acyclic
    std::cout << "  WidgetHierarchy_NoCycles: ";
    auto result5 = rc::check("Widget hierarchy is acyclic", []() {
        auto parent = std::make_unique<Widget>();
        auto child = std::make_unique<Widget>();
        
        Widget* p = parent.get();
        Widget* c = child.get();
        
        parent->addChild(std::move(child));
        
        // Verify no cycle: child's parent is parent, but parent's parent is null
        RC_ASSERT(c->m_parent == p);
        RC_ASSERT(p->m_parent == nullptr);
    });
    if (!result5) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property 6: Root widget has no parent
    std::cout << "  WidgetHierarchy_RootHasNoParent: ";
    auto result6 = rc::check("Root widget has no parent", []() {
        auto root = std::make_unique<Widget>();
        RC_ASSERT(root->m_parent == nullptr);
    });
    if (!result6) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    // Property 7: Child widgets have correct parent pointer
    std::cout << "  WidgetHierarchy_ChildrenHaveCorrectParent: ";
    auto result7 = rc::check("Child widgets have correct parent pointer", []() {
        auto parent = std::make_unique<Widget>();
        auto child1 = std::make_unique<Widget>();
        auto child2 = std::make_unique<Widget>();
        
        Widget* p = parent.get();
        Widget* c1 = child1.get();
        Widget* c2 = child2.get();
        
        parent->addChild(std::move(child1));
        parent->addChild(std::move(child2));
        
        RC_ASSERT(c1->m_parent == p);
        RC_ASSERT(c2->m_parent == p);
    });
    if (!result7) { all_passed = false; std::cout << "FAILED\n"; } else { std::cout << "PASSED\n"; }
    
    return all_passed;
}

#endif // HAVE_RAPIDCHECK

// ============================================================================
// Main entry point
// ============================================================================

int main() {
    std::cout << "\n========================================\n";
    std::cout << "Widget Hierarchy Property Tests\n";
    std::cout << "Validates: Requirements 1.1, 1.2, 1.7\n";
    std::cout << "========================================\n\n";
    
#ifdef HAVE_RAPIDCHECK
    
    bool passed = runWidgetHierarchyPropertyTests();
    
    std::cout << "\n========================================\n";
    if (passed) {
        std::cout << "All widget hierarchy property tests PASSED\n";
    } else {
        std::cout << "Some widget hierarchy property tests FAILED\n";
    }
    std::cout << "========================================\n\n";
    
    return passed ? 0 : 1;
    
#else // !HAVE_RAPIDCHECK
    
    std::cout << "RapidCheck not enabled, skipping property-based tests\n\n";
    return 0;
    
#endif // HAVE_RAPIDCHECK
}
