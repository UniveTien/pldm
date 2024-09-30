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

    void addUpdateManager(const eid& eid, const std::shared_ptr<UpdateManagerInf>& updateManager)
    {
        updateManagerPairs.emplace_back(std::make_pair(eid,updateManager));
    }

    void removeUpdateManagers(const eid& eidToRemove)
    {
        updateManagerPairs.erase(std::remove_if(updateManagerPairs.begin(),updateManagerPairs.end(),
            [eidToRemove](std::pair<eid, std::shared_ptr<UpdateManagerInf>>& obj){
                return eidToRemove==obj.first;
            }),updateManagerPairs.end());
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
            for (auto& [_eid, manager] : updateManagerPairs)
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
    std::vector<std::pair<eid, std::shared_ptr<UpdateManagerInf>>> updateManagerPairs;
};

} // namespace pldm::fw_update
