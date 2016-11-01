// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/service/apt/apt.h"

namespace HLE {
namespace Applets {

ResultCode ErrEula::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    if (parameter.signal != static_cast<u32>(Service::APT::SignalType::LibAppJustStarted)) {
        LOG_ERROR(Service_APT, "unsupported signal %u", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());

    memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

    // TODO: allocated memory never released
    using Kernel::MemoryPermission;
    // Allocate a heap block of the required size for this applet.
    heap_memory = std::make_shared<std::vector<u8>>(capture_info.size);
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = Kernel::SharedMemory::CreateForApplet(
        heap_memory, 0, heap_memory->size(), MemoryPermission::ReadWrite,
        MemoryPermission::ReadWrite, "ErrEula Memory");

    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = static_cast<u32>(Service::APT::SignalType::LibAppFinished);
    result.buffer.clear();
    result.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    result.sender_id = static_cast<u32>(id);
    result.object = framebuffer_memory;

    Service::APT::SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode ErrEula::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    started = true;

    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (ErrEulaConfig) is wrong");

    memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    switch (config.error_type) {
    case ErrorType::ErrorCode:
        LOG_ERROR(Service_APT, "ErrEula error_code = 0x%08X", config.error_code);
        break;
    case ErrorType::LocalizedErrorText:
    case ErrorType::ErrorText: {
        std::string error = Common::UTF16ToUTF8(config.error_text);
        LOG_ERROR(Service_APT, "ErrEula error code: 0x%X text: %s", config.error_code,
                  error.c_str());
        break;
    }
    case ErrorType::Agree:
    case ErrorType::Eula:
    case ErrorType::EulaDrawOnly:
    case ErrorType::EulaFirstBoot:
        LOG_WARNING(Service_APT, "ErrEula eula accepted");
        break;
    }

    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(config));
    config.return_code = ReturnCode::None;

    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = static_cast<u32>(Service::APT::SignalType::LibAppClosed);
    message.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    message.sender_id = static_cast<u32>(id);
    Service::APT::SendParameter(message);

    started = false;
    return RESULT_SUCCESS;
}

void ErrEula::Update() {}

} // namespace Applets
} // namespace HLE
