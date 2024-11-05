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
namespace sdbusRule = sdbusplus::bus::match::rules;

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_ROOT = "/org/freedesktop/systemd1";
constexpr auto SYSTEMD_INTERFACE = "org.freedesktop.systemd1.Manager";
const auto SYSTEMD_JOB_REMOVED_EVENT =
    sdbusRule::type::signal() + sdbusRule::path(SYSTEMD_ROOT) +
    sdbusRule::interface(SYSTEMD_INTERFACE) + sdbusRule::member("JobRemoved");

class ServiceCondition
{
  public:
    explicit ServiceCondition(const std::string& conditionService) :
        conditionService(conditionService),
        systemdSignals(
            bus, SYSTEMD_JOB_REMOVED_EVENT,
            std::bind_front(std::mem_fn(&ServiceCondition::unitStateChange),
                            this))
    {}

    void execute()
    {
        if (conditionService.empty())
        {
            info("Executed an empty condition, ignored");
            return;
        }
        if (beforeServiceRoutine)
        {
            (*beforeServiceRoutine)();
        }
        auto method = bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_ROOT,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(serviceName(), "replace");
        auto reply = bus.call(method);
        reply.read(jobPath);
        info("Started Service: {SERV}", "SERV", serviceName());
        info("Job path: {JOB}", "JOB", jobPath);
    }

    std::string serviceName()
    {
        auto service = conditionService;
        if (!arg.empty())
        {
            auto lastSlash = arg.rfind("/");
            if (lastSlash == arg.size() - 1)
            {
                error("Bad argument to a service unit: {ARG}", "ARG", arg);
                throw(std::runtime_error("Bad argument to a service unit"));
            }
            auto subFrom = lastSlash == std::string::npos ? 0 : lastSlash + 1;
            auto prunedArg = arg.substr(subFrom);
            service += "@" + prunedArg;
        }
        service += ".service";
        return service;
    }

    void assignArg(const std::string& arg)
    {
        this->arg = arg;
    }

    void
        assignRoutine(std::optional<std::function<void()>> beforeServiceRoutine,
                      std::optional<std::function<void()>> afterServiceRoutine)
    {
        if (beforeServiceRoutine)
        {
            this->beforeServiceRoutine = beforeServiceRoutine;
        }
        if (afterServiceRoutine)
        {
            this->afterServiceRoutine = afterServiceRoutine;
        }
    }

    void unitStateChange(sdbusplus::message_t& msg)
    {
        uint32_t jobId;
        sdbusplus::message::object_path returnedJobPath;
        try
        {
            msg.read(jobId, returnedJobPath);
        }
        catch (const std::exception& e)
        {
            error("Error reading message, {WHAT}", "WHAT", e.what());
            return;
        }
        if (returnedJobPath == jobPath)
        {
            info("received! {JOB}", "JOB", returnedJobPath);
            if (afterServiceRoutine)
            {
                (*afterServiceRoutine)();
            }
        }
    }

  private:
    decltype(pldm::utils::DBusHandler::getBus()) bus =
        pldm::utils::DBusHandler::getBus();
    std::string conditionService;
    std::string arg;
    sdbusplus::message::object_path jobPath;
    std::optional<std::function<void()>> beforeServiceRoutine;
    std::optional<std::function<void()>> afterServiceRoutine;
    sdbusplus::bus::match_t systemdSignals;
};

} // namespace fw_update
} // namespace pldm
