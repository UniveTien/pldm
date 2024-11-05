#pragma once

#include "common/instance_id.hpp"
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

class InventoryManager;
class InventoryItemManager;
class DeviceUpdater;
class UpdateManager;
class Manager;

struct ResourceManager
{
    UpdateManager* updateManager;
    InventoryItemManager* inventoryItemManager;
    InventoryManager* inventoryManager;
};

} // namespace fw_update
} // namespace pldm
