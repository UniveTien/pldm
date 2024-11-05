#pragma once

#include "update_manager_decorator.hpp"

namespace pldm::fw_update
{

class AggregateUpdateManager : public UpdateManagerDecorator
{
  public:
    AggregateUpdateManager(const std::shared_ptr<UpdateManagerInf>& updateManager) :
        UpdateManagerDecorator(updateManager)
    {}

    void addUpdateManager(const DeviceId deviceId,
                          const std::shared_ptr<UpdateManagerInf> updateManager)
    {
        updateManagerMap[deviceId] = updateManager;
    }

    void removeUpdateManager(DeviceId deviceId)
    {
        updateManagerMap.erase(deviceId);
    }

    void removeUpdateManagers(const eid& eidToRemove)
    {
        std::erase_if(updateManagerMap, [&](const auto& item) {
            return item.first.first == eidToRemove;
        });
    }

    Response handleRequest(mctp_eid_t eid, uint8_t command,
                           const pldm_msg* request, size_t reqMsgLen) override
    {
        auto response = UpdateManagerDecorator::handleRequest(
            eid, command, request, reqMsgLen);
        auto responseMsg = reinterpret_cast<pldm_msg*>(response.data());

        if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
        {
            return response;
        }
        else
        {
            for (auto& [deviceId, manager] : updateManagerMap)
            {
                response = manager->handleRequest(eid, command, request,
                                                 reqMsgLen);
                responseMsg = reinterpret_cast<pldm_msg*>(response.data());
                if (responseMsg->payload[0] != PLDM_FWUP_COMMAND_NOT_EXPECTED)
                {
                    return response;
                }
            }

            return response;
        }
    }

  private:
    std::map<DeviceId, std::shared_ptr<UpdateManagerInf>> updateManagerMap;
};

} // namespace pldm::fw_update
