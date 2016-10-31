// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ResultVal<std::unique_ptr<FileBackend>> SDMCArchive::OpenFile(const Path& path,
                                                              const Mode& mode) const {
    Mode modified_mode;
    modified_mode.hex = mode.hex;

    // SDMC archive always opens a file with at least read permission
    modified_mode.read_flag.Assign(1);

    return OpenFileBase(path, modified_mode);
}

ResultVal<std::unique_ptr<FileBackend>> SDMCArchive::OpenFileBase(const Path& path,
                                                                  const Mode& mode) const {
    LOG_DEBUG(Service_FS, "called path=%s mode=%01X", path.DebugStr().c_str(), mode.hex);

    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    if (mode.hex == 0) {
        LOG_ERROR(Service_FS, "Empty open mode");
        return ERROR_INVALID_OPEN_FLAGS;
    }

    if (mode.create_flag && !mode.write_flag) {
        LOG_ERROR(Service_FS, "Create flag set but write flag not set");
        return ERROR_INVALID_OPEN_FLAGS;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "%s is not a file", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC;
    case PathParser::NotFound:
        if (!mode.create_flag) {
            LOG_ERROR(Service_FS, "Non-existing file %s can't be open without mode create.",
                      full_path.c_str());
            return ERROR_NOT_FOUND;
        } else {
            // Create the file
            FileUtil::CreateEmptyFile(full_path);
        }
        break;
    }

    FileUtil::IOFile file(full_path, mode.write_flag ? "r+b" : "rb");
    if (!file.IsOpen()) {
        LOG_CRITICAL(Service_FS, "(unreachable) Unknown error opening %s", full_path.c_str());
        return ERROR_NOT_FOUND;
    }

    auto disk_file = std::make_unique<DiskFile>(std::move(file), mode);
    return MakeResult<std::unique_ptr<FileBackend>>(std::move(disk_file));
}

ResultCode SDMCArchive::DeleteFile(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "%s not found", full_path.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "%s is not a file", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC;
    }

    if (FileUtil::Delete(full_path)) {
        return RESULT_SUCCESS;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error deleting %s", full_path.c_str());
    return ERROR_NOT_FOUND;
}

ResultCode SDMCArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    if (FileUtil::Rename(mount_point + src_path.AsString(), mount_point + dest_path.AsString())) {
        return RESULT_SUCCESS;
    }

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

template <typename T>
static ResultCode DeleteDirectoryHelper(const Path& path, const std::string& mount_point,
                                        T deleter) {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    if (path_parser.IsRootDirectory())
        return ERROR_NOT_FOUND;

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::FileInPath:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "Unexpected file in path %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC;
    }

    if (deleter(full_path)) {
        return RESULT_SUCCESS;
    }

    LOG_ERROR(Service_FS, "Directory not empty %s", full_path.c_str());
    return ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC;
}

ResultCode SDMCArchive::DeleteDirectory(const Path& path) const {
    return DeleteDirectoryHelper(path, mount_point, FileUtil::DeleteDir);
}

ResultCode SDMCArchive::DeleteDirectoryRecursively(const Path& path) const {
    return DeleteDirectoryHelper(
        path, mount_point, [](const std::string& p) { return FileUtil::DeleteDirRecursively(p); });
}

ResultCode SDMCArchive::CreateFile(const FileSys::Path& path, u64 size) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "%s already exists", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC;
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "%s already exists", full_path.c_str());
        return ERROR_ALREADY_EXISTS;
    }

    if (size == 0) {
        FileUtil::CreateEmptyFile(full_path);
        return RESULT_SUCCESS;
    }

    FileUtil::IOFile file(full_path, "wb");
    // Creates a sparse file (or a normal file on filesystems without the concept of sparse files)
    // We do this by seeking to the right size, then writing a single null byte.
    if (file.Seek(size - 1, SEEK_SET) && file.WriteBytes("", 1) == 1) {
        return RESULT_SUCCESS;
    }

    LOG_ERROR(Service_FS, "Too large file");
    return ResultCode(ErrorDescription::TooLarge, ErrorModule::FS, ErrorSummary::OutOfResource,
                      ErrorLevel::Info);
}

ResultCode SDMCArchive::CreateDirectory(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::DirectoryFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "%s already exists", full_path.c_str());
        return ERROR_ALREADY_EXISTS;
    }

    if (FileUtil::CreateDir(mount_point + path.AsString())) {
        return RESULT_SUCCESS;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error creating %s", mount_point.c_str());
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                      ErrorLevel::Status);
}

ResultCode SDMCArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    if (FileUtil::Rename(mount_point + src_path.AsString(), mount_point + dest_path.AsString()))
        return RESULT_SUCCESS;

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

ResultVal<std::unique_ptr<DirectoryBackend>> SDMCArchive::OpenDirectory(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "%s not found", full_path.c_str());
        return ERROR_NOT_FOUND;
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Unexpected file in path %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY_SDMC;
    }

    auto directory = std::make_unique<DiskDirectory>(full_path);
    return MakeResult<std::unique_ptr<DirectoryBackend>>(std::move(directory));
}

u64 SDMCArchive::GetFreeBytes() const {
    // TODO: Stubbed to return 1GiB
    return 1024 * 1024 * 1024;
}

ArchiveFactory_SDMC::ArchiveFactory_SDMC(const std::string& sdmc_directory)
    : sdmc_directory(sdmc_directory) {
    LOG_INFO(Service_FS, "Directory %s set as SDMC.", sdmc_directory.c_str());
}

bool ArchiveFactory_SDMC::Initialize() {
    if (!Settings::values.use_virtual_sd) {
        LOG_WARNING(Service_FS, "SDMC disabled by config.");
        return false;
    }

    if (!FileUtil::CreateFullPath(sdmc_directory)) {
        LOG_ERROR(Service_FS, "Unable to create SDMC path.");
        return false;
    }

    return true;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SDMC::Open(const Path& path) {
    auto archive = std::make_unique<SDMCArchive>(sdmc_directory);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SDMC::Format(const Path& path,
                                       const FileSys::ArchiveFormatInfo& format_info) {
    // This is kind of an undesirable operation, so let's just ignore it. :)
    return RESULT_SUCCESS;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SDMC::GetFormatInfo(const Path& path) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive %s", GetName().c_str());
    return ResultCode(-1);
}
} // namespace FileSys
