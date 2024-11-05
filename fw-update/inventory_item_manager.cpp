#include "inventory_item_manager.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <openssl/evp.h>

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace pldm::fw_update
{

void InventoryItemManager::createInventoryItem(
    const eid& eid, const FirmwareDeviceName& deviceName,
    const std::string& activeVersion,
    std::shared_ptr<UpdateManager> updateManager)
{
    if (!updateManager || deviceName.empty())
    {
        error("Invalid inventory item arguments for {EID}, {NAME}", "EID", eid,
              "NAME", deviceName);
        return;
    }

    const std::lock_guard<std::mutex> lock(recordOperator);
	try
	{
        const auto boardPath = getBoardPath(inventoryPathMap.at(eid));
        if (boardPath.empty())
        {
            error("Board name on {EID} is empty", "EID", eid);
            return;
        }
        auto devicePath = boardPath + "_" + deviceName;

        auto deviceId = DeviceId(eid, deviceName);
        interfaceHubMap[deviceId] = InterfaceHub();
        auto& hub = interfaceHubMap[deviceId];

        hub.inventoryItem =
            std::make_unique<InventoryItemBoardIntf>(bus, devicePath.c_str());
        const auto softwarePath = "/xyz/openbmc_project/software/" +
                                  devicePath.substr(devicePath.rfind("/") + 1) +
                                  "_" + getVersionId(activeVersion);
        createVersion(hub, softwarePath, activeVersion, VersionPurpose::Other);
        createAssociation(hub, softwarePath, "running", "ran_on", devicePath);
        aggregateUpdateManager->addUpdateManager(deviceId, updateManager);
        updateManager->overrideObjPath(softwarePath);
        updateManager->assignActivation(std::make_shared<Activation>(
            bus, softwarePath, Activations::Active, updateManager.get()));
        hub.codeUpdater = std::make_unique<CodeUpdater>(
            bus, softwarePath.c_str(), updateManager);
    }
	catch(const std::out_of_range& e)
	{
        error("Failed to find eid in map, error, EID is {EID} - {ERROR}", "EID",
              eid, "ERROR", e.what());
    }
}

using EVP_MD_CTX_Ptr =
    std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;

std::string InventoryItemManager::getVersionId(const std::string& version)
{
    if (version.empty())
    {
        error("Version is empty.");
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    EVP_MD_CTX_Ptr ctx(EVP_MD_CTX_new(), &::EVP_MD_CTX_free);

    EVP_DigestInit(ctx.get(), EVP_sha512());
    EVP_DigestUpdate(ctx.get(), version.c_str(), strlen(version.c_str()));
    EVP_DigestFinal(ctx.get(), digest.data(), nullptr);

    // We are only using the first 8 characters.
    char mdString[9];
    snprintf(mdString, sizeof(mdString), "%02x%02x%02x%02x",
             (unsigned int)digest[0], (unsigned int)digest[1],
             (unsigned int)digest[2], (unsigned int)digest[3]);

    return mdString;
}

void InventoryItemManager::createVersion(
    InterfaceHub& hub, const std::string& path, std::string version,
    VersionPurpose purpose)
{
    if (version.empty())
    {
        version = "N/A";
    }

    hub.version = std::make_unique<Version>(utils::DBusHandler::getBus(), path,
                                            version, purpose);
}

void InventoryItemManager::createAssociation(
    InterfaceHub& hub, const std::string& path, const std::string& foward,
    const std::string& reverse, const std::string& assocEndPoint)
{
    hub.association = std::make_unique<AssociationDefinitionsIntf>(
        utils::DBusHandler::getBus(), path.c_str());
    hub.association->associations(
        {{foward.c_str(), reverse.c_str(), assocEndPoint.c_str()}});
}

const ObjectPath InventoryItemManager::getBoardPath(const InventoryPath& path)
{
    constexpr auto boardInterface = "xyz.openbmc_project.Inventory.Item.Board";
    pldm::utils::GetAncestorsResponse response;

    try
    {
        response = pldm::utils::DBusHandler().getAncestors(path.c_str(),
                                                           {boardInterface});
    }
    catch (const sdbusplus::exception_t& e)
    {
        error("Failed to get ancestors, error={ERROR}, path={PATH}", "ERROR", e,
              "PATH", path.c_str());
        return {};
    }

    if (response.size() != 1)
    {
        // Use default path if multiple or no board objects are found
        error(
            "Ambiguous Board path, found multiple Dbus Objects with the same interface={INTERFACE}, SIZE={SIZE}",
            "INTERFACE", boardInterface, "SIZE", response.size());
        return "/xyz/openbmc_project/inventory/system/board/PLDM_Device";
    }

    return std::get<ObjectPath>(response.front());
}

void InventoryItemManager::refreshInventoryPath(const eid& eid,
                                                const InventoryPath& path)
{
    const std::lock_guard<std::mutex> lock(recordOperator);
    if (inventoryPathMap.contains(eid))
    {
        inventoryPathMap.at(eid) = path;
    }
    else
    {
        inventoryPathMap.emplace(eid, path);
    }
}

void InventoryItemManager::removeInventoryItem(const DeviceId& deviceId)
{
    const std::lock_guard<std::mutex> lock(recordOperator);
    interfaceHubMap.erase(deviceId);
    aggregateUpdateManager->removeUpdateManager(deviceId);
}

void InventoryItemManager::removeInventoryItems(const eid& eidToRemove)
{
    const std::lock_guard<std::mutex> lock(recordOperator);
    std::erase_if(interfaceHubMap, [&](const auto& item) {
        return item.first.first == eidToRemove;
    });
    aggregateUpdateManager->removeUpdateManagers(eidToRemove);
    inventoryPathMap.erase(eidToRemove);
}

} // namespace pldm::fw_update
