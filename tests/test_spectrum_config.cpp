#include "test_framework.h"
#include "core/SpectrumConfig.h"
#include <iostream>

using PsyMP3::Core::SpectrumConfig;
using namespace TestFramework;

class TestSpectrumConfig : public TestCase {
public:
    TestSpectrumConfig() : TestCase("Spectrum Config Tests") {}

    void runTest() override {
        // Verify constants
        ASSERT_EQUALS(SpectrumConfig::NumBands, 320, "NumBands should be 320");
        ASSERT_EQUALS(SpectrumConfig::Zone1End, 106, "Zone1End should be 106");
        ASSERT_EQUALS(SpectrumConfig::Zone3Start, 214, "Zone3Start should be 214");

        // Verify colors for key indices

        // Index 0 (Zone 1 start)
        auto color0 = SpectrumConfig::getBarColor(0);
        ASSERT_EQUALS(color0.r, 128, "Index 0 R");
        ASSERT_EQUALS(color0.g, 255, "Index 0 G");
        ASSERT_EQUALS(color0.b, 0, "Index 0 B"); // 0 * 2.398 = 0

        // Index 105 (Zone 1 end)
        auto color105 = SpectrumConfig::getBarColor(105);
        ASSERT_EQUALS(color105.r, 128, "Index 105 R");
        ASSERT_EQUALS(color105.g, 255, "Index 105 G");
        // 105 * 2.398 = 251.79 -> 251
        ASSERT_EQUALS(color105.b, 251, "Index 105 B");

        // Index 106 (Zone 2 start)
        // r = 128 - (0 * 1.1962615) = 128
        // g = 255 - (0 * 2.383177) = 255
        // b = 255
        auto color106 = SpectrumConfig::getBarColor(106);
        ASSERT_EQUALS(color106.r, 128, "Index 106 R");
        ASSERT_EQUALS(color106.g, 255, "Index 106 G");
        ASSERT_EQUALS(color106.b, 255, "Index 106 B");

        // Index 213 (Zone 2 end)
        // r = 128 - (107 * 1.1962615) = 128 - 127.99998 = 0
        // g = 255 - (107 * 2.383177) = 255 - 254.999939 = 0
        // b = 255
        auto color213 = SpectrumConfig::getBarColor(213);
        ASSERT_EQUALS(color213.r, 0, "Index 213 R");
        ASSERT_EQUALS(color213.g, 0, "Index 213 G");
        ASSERT_EQUALS(color213.b, 255, "Index 213 B");

        // Index 214 (Zone 3 start)
        // r = (0) * 2.4 = 0
        // g = 0
        // b = 255
        auto color214 = SpectrumConfig::getBarColor(214);
        ASSERT_EQUALS(color214.r, 0, "Index 214 R");
        ASSERT_EQUALS(color214.g, 0, "Index 214 G");
        ASSERT_EQUALS(color214.b, 255, "Index 214 B");

        // Index 319 (Zone 3 end)
        // r = (319 - 214) * 2.4 = 105 * 2.4 = 252
        // g = 0
        // b = 255
        auto color319 = SpectrumConfig::getBarColor(319);
        ASSERT_EQUALS(color319.r, 252, "Index 319 R");
        ASSERT_EQUALS(color319.g, 0, "Index 319 G");
        ASSERT_EQUALS(color319.b, 255, "Index 319 B");
    }
};

int main() {
    TestSuite suite("Spectrum Config Tests");
    suite.addTest(std::make_unique<TestSpectrumConfig>());
    auto results = suite.runAll();
    suite.printResults(results);
    return (suite.getFailureCount(results) == 0) ? 0 : 1;
}
