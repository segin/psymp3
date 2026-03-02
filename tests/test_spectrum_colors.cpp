#include "test_framework.h"
#include "SpectrumColors.h"

using namespace PsyMP3;
using namespace TestFramework;

class SpectrumColorTest : public TestCase {
public:
    SpectrumColorTest() : TestCase("SpectrumColorTest") {}

    void runTest() override {
        uint8_t r, g, b;

        // Test Low range (x < 106)
        // x = 0
        SpectrumColorConfig::getRGB(0, r, g, b);
        ASSERT_EQUALS(128, r, "x=0 r");
        ASSERT_EQUALS(255, g, "x=0 g");
        ASSERT_EQUALS(0, b, "x=0 b");

        // x = 105
        SpectrumColorConfig::getRGB(105, r, g, b);
        ASSERT_EQUALS(128, r, "x=105 r");
        ASSERT_EQUALS(255, g, "x=105 g");
        // b = 105 * 2.398 = 251.79 -> 251
        ASSERT_EQUALS(251, b, "x=105 b");

        // Test Mid range (106 <= x <= 213)
        // x = 106
        SpectrumColorConfig::getRGB(106, r, g, b);
        // r = 128 - (0 * 1.19...) = 128
        // g = 255 - (0 * 2.38...) = 255
        // b = 255
        ASSERT_EQUALS(128, r, "x=106 r");
        ASSERT_EQUALS(255, g, "x=106 g");
        ASSERT_EQUALS(255, b, "x=106 b");

        // x = 213
        SpectrumColorConfig::getRGB(213, r, g, b);
        // r = 128 - ((213-106) * 1.1962615) = 128 - (107 * 1.1962615) = 128 - 127.99998 = 0
        // g = 255 - ((213-106) * 2.383177) = 255 - (107 * 2.383177) = 255 - 254.999939 = 0
        ASSERT_EQUALS(0, r, "x=213 r");
        ASSERT_EQUALS(0, g, "x=213 g");
        ASSERT_EQUALS(255, b, "x=213 b");

        // Test High range (x > 213)
        // x = 214
        SpectrumColorConfig::getRGB(214, r, g, b);
        // r = (214-214) * 2.4 = 0
        ASSERT_EQUALS(0, r, "x=214 r");
        ASSERT_EQUALS(0, g, "x=214 g");
        ASSERT_EQUALS(255, b, "x=214 b");

        // x = 319 (last bin)
        SpectrumColorConfig::getRGB(319, r, g, b);
        // r = (319-214) * 2.4 = 105 * 2.4 = 252
        ASSERT_EQUALS(252, r, "x=319 r");
        ASSERT_EQUALS(0, g, "x=319 g");
        ASSERT_EQUALS(255, b, "x=319 b");
    }
};

int main() {
    TestSuite suite("Spectrum Colors Test Suite");
    suite.addTest(std::make_unique<SpectrumColorTest>());

    std::vector<TestCaseInfo> results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) > 0 ? 1 : 0;
}
