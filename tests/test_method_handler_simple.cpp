#include "psymp3.h"
#include "test_framework.h"
#include "MethodHandler.h"
#include "PropertyManager.h"

using namespace TestFramework;

class MethodHandlerSimpleTest : public TestCase {
public:
    MethodHandlerSimpleTest() : TestCase("MethodHandlerSimpleTest") {}

    void setUp() override {
        // In testing mode, we can pass nullptr since Player calls are disabled
        m_property_manager = std::make_unique<PropertyManager>(nullptr);
        m_method_handler = std::make_unique<MethodHandler>(nullptr, m_property_manager.get());
    }

    void tearDown() override {
        m_method_handler.reset();
        m_property_manager.reset();
    }

    void runTest() override {
        testConstruction();
        testInitialization();
        testNullParameterHandling();
    }

private:
    std::unique_ptr<MethodHandler> m_method_handler;
    std::unique_ptr<PropertyManager> m_property_manager;

    void testConstruction() {
        ASSERT_TRUE(m_method_handler != nullptr, "MethodHandler should be constructed");
        std::cout << "Construction test passed" << std::endl;
    }

    void testInitialization() {
        // In testing mode with nullptr player, MethodHandler should not be ready
        ASSERT_FALSE(m_method_handler->isReady(), "MethodHandler should not be ready with null player");
        std::cout << "Initialization test passed" << std::endl;
    }

    void testNullParameterHandling() {
        // Test that the handler doesn't crash with null parameters
        auto result = m_method_handler->handleMessage(nullptr, nullptr);
        ASSERT_EQUALS(DBUS_HANDLER_RESULT_NOT_YET_HANDLED, result, "Should handle null parameters gracefully");
        std::cout << "Null parameter handling test passed" << std::endl;
    }
};

int main() {
    std::cout << "Running simple MethodHandler test..." << std::endl;
    
    TestSuite suite("MethodHandler Simple Tests");
    suite.addTest(std::make_unique<MethodHandlerSimpleTest>());
    
    auto results = suite.runAll();
    bool success = true;
    for (const auto& result : results) {
        if (result.result != TestResult::PASSED) {
            success = false;
            break;
        }
    }
    
    std::cout << "Simple MethodHandler test " << (success ? "PASSED" : "FAILED") << std::endl;
    return success ? 0 : 1;
}