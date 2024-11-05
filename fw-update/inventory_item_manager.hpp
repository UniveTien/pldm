#pragma once

#include "activation.hpp"
#include "aggregate_update_manager.hpp"
#include "code_updater.hpp"
#include "common/types.hpp"
#include "version.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Board/server.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

namespace pldm::fw_update
{

using InventoryItemBoardIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Board>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
using VersionPurpose =
    sdbusplus::server::xyz::openbmc_project::software::Version::VersionPurpose;
using Activations =
    sdbusplus::server::xyz::openbmc_project::software::Activation::Activations;

using ObjectPath = pldm::dbus::ObjectPath;

struct InterfaceHub
{
    std::unique_ptr<InventoryItemBoardIntf> inventoryItem = nullptr;
    std::unique_ptr<Version> version = nullptr;
    std::unique_ptr<AssociationDefinitionsIntf> association = nullptr;
    std::unique_ptr<CodeUpdater> codeUpdater = nullptr;
};

class InventoryItemManager
{
  public:
    InventoryItemManager(
        std::shared_ptr<AggregateUpdateManager> aggregateUpdateManager) :
        aggregateUpdateManager(aggregateUpdateManager)
    {}
    InventoryItemManager(const InventoryItemManager&) = delete;
    InventoryItemManager(InventoryItemManager&&) = delete;
    InventoryItemManager& operator=(const InventoryItemManager&) = delete;
    InventoryItemManager& operator=(InventoryItemManager&&) = delete;
    ~InventoryItemManager() = default;

    void createInventoryItem(const eid& eid,
                             const FirmwareDeviceName& deviceName,
                             const std::string& activeVersion,
                             std::shared_ptr<UpdateManager> updateManager = nullptr);

    void refreshInventoryPath(const eid& eid, const InventoryPath& path);

    void removeInventoryItem(const DeviceId& deviceId);

    void removeInventoryItems(const eid& eidToRemove);

  private:
    std::string getVersionId(const std::string& version);

    void createVersion(InterfaceHub& hub, const std::string& path,
                       std::string version, VersionPurpose purpose);

    void createAssociation(InterfaceHub& hub, const std::string& path,
                           const std::string& foward,
                           const std::string& reverse,
                           const std::string& assocEndPoint);

    const ObjectPath getBoardPath(const InventoryPath& path);

    decltype(utils::DBusHandler::getBus()) bus = utils::DBusHandler::getBus();

    std::map<eid, InventoryPath> inventoryPathMap;

    std::map<DeviceId, InterfaceHub> interfaceHubMap;

    std::shared_ptr<AggregateUpdateManager> aggregateUpdateManager;

    std::mutex recordOperator;
};

} // namespace pldm::fw_update
