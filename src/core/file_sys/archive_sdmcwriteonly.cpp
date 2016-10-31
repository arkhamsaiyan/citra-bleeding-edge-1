// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "core/file_sys/archive_sdmcwriteonly.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ResultVal<std::unique_ptr<FileBackend>> SDMCWriteOnlyArchive::OpenFile(const Path& path,
                                                                       const Mode& mode) const {
    if (mode.read_flag) {
        LOG_ERROR(Service_FS, "Read flag is not supported");
        return ERROR_INVALID_READ_FLAG;
    }
    return SDMCArchive::OpenFileBase(path, mode);
}

ResultVal<std::unique_ptr<DirectoryBackend>> SDMCWriteOnlyArchive::OpenDirectory(
    const Path& path) const {
    LOG_ERROR(Service_FS, "Not supported");
    return ERROR_UNSUPPORTED_OPEN_FLAGS;
}

ArchiveFactory_SDMCWriteOnly::ArchiveFactory_SDMCWriteOnly(const ArchiveFactory_SDMC& sdmc)
    : sdmc_directory(sdmc.sdmc_directory) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SDMCWriteOnly::Open(const Path& path) {
    auto archive = std::make_unique<SDMCWriteOnlyArchive>(sdmc_directory);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SDMCWriteOnly::Format(const Path& path,
                                                const FileSys::ArchiveFormatInfo& format_info) {
    // TODO(wwylele): hwtest this
    LOG_ERROR(Service_FS, "Attempted to format a SDMC write-only archive.");
    return ResultCode(-1);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SDMCWriteOnly::GetFormatInfo(const Path& path) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive %s", GetName().c_str());
    return ResultCode(-1);
}

} // namespace FileSys
