#pragma once

#include "common/types.hpp"
#include "common/utils.hpp"

#include <libpldm/base.h>

#include <phosphor-logging/lg2.hpp>

namespace pldm
{

namespace fw_update
{

using namespace sdeventplus;
using namespace sdeventplus::source;
using namespace pldm;
namespace fs = std::filesystem;
using Json = nlohmann::json;

using ServiceConditionPair = std::pair<std::string, std::string>;
using ConditionBindMap =
    std::unordered_map<FirmwareDeviceName, ServiceConditionPair>;

class JsonConditionCollector
{
  public:
    explicit JsonConditionCollector(fs::path jsonPath)
    {
        std::ifstream jsonFile(jsonPath);
        Json data = Json::parse(jsonFile);
        if (data.contains("Components") && data["Components"].is_array())
        {
            for (const auto& component : data["Components"])
            {
                if (component.contains("Component") &&
                    component["Component"].is_string() &&
                    component.contains("PreSetupTarget") &&
                    component["PreSetupTarget"].is_string() &&
                    component.contains("PostSetupTarget") &&
                    component["PostSetupTarget"].is_string())
                {
                    FirmwareDeviceName componentName = component["Component"];
                    std::string preSetupTarget = component["PreSetupTarget"];
                    std::string postSetupTarget = component["PostSetupTarget"];

                    conditionBindMap.emplace(
                        std::move(componentName),
                        ServiceConditionPair(preSetupTarget, postSetupTarget));
                }
                else
                {
                    error("A bad Component Attributes for {JSPATH} found",
                          "JSPATH", jsonPath);
                    continue;
                }
            }
        }
        else
        {
            error("Bad Json format for parsing: {JSPATH}", "JSPATH", jsonPath);
        }
    }

    std::string preCondition(FirmwareDeviceName name)
    {
        if (conditionBindMap.find(name) != conditionBindMap.end())
        {
            return conditionBindMap.at(name).first;
        }
        return std::string("");
    }

    std::string postCondition(FirmwareDeviceName name)
    {
        if (conditionBindMap.find(name) != conditionBindMap.end())
        {
            return conditionBindMap.at(name).second;
        }
        return std::string("");
    }

    std::optional<ServiceConditionPair> conditionPair(FirmwareDeviceName name)
    {
        if (conditionBindMap.find(name) != conditionBindMap.end())
        {
            return conditionBindMap.at(name);
        }
        return std::nullopt;
    }

  private:
    ConditionBindMap conditionBindMap;
};

} // namespace fw_update
} // namespace pldm
