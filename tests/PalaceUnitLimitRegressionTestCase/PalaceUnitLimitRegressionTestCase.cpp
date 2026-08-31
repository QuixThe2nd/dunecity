#include <catch2/catch_all.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::filesystem::path sourceRoot() {
    if(const char* configuredRoot = std::getenv("DUNE_CITY_SOURCE_DIR")) {
        return configuredRoot;
    }
    return std::filesystem::current_path();
}

std::string readSource(const std::filesystem::path& relativePath) {
    std::ifstream stream(sourceRoot() / relativePath, std::ios::binary);
    REQUIRE(stream.is_open());
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

} // namespace

TEST_CASE("Palace reinforcements honor unit limits", "[palace][unit-limit][regression]") {
    const std::string palace = readSource("src/structures/Palace.cpp");

    REQUIRE(palace.find("isUnitLimitReached(Unit_Trooper)") != std::string::npos);
    REQUIRE(palace.find("isUnitLimitReached(Unit_Saboteur)") != std::string::npos);
    REQUIRE(palace.find("isUnitLimitReached(itemID)") != std::string::npos);
    REQUIRE(palace.find("isUnitLimitReached(Unit_Ornithopter)") != std::string::npos);
}

TEST_CASE("Canceled unit deployment cannot leave a dead global-list entry",
          "[palace][unit-lifetime][regression]") {
    const std::string unitBase = readSource("src/units/UnitBase.cpp");
    const std::string palace = readSource("src/structures/Palace.cpp");
    const std::string starPort = readSource("src/structures/StarPort.cpp");
    const std::string techCenter = readSource("src/structures/TechCenter.cpp");

    REQUIRE(unitBase.find("void UnitBase::cancelDeployment()") != std::string::npos);
    REQUIRE(unitBase.find("unitList.remove(this);") != std::string::npos);
    REQUIRE(palace.find("cancelDeployment()") != std::string::npos);
    REQUIRE(starPort.find("cancelDeployment()") != std::string::npos);
    REQUIRE(techCenter.find("cancelDeployment()") != std::string::npos);
    REQUIRE(palace.find("delete newUnit") == std::string::npos);
    REQUIRE(palace.find("delete ornithopter") == std::string::npos);
}
