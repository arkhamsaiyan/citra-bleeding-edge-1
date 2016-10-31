// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"
#include "core/file_sys/savedata_archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ResultVal<std::unique_ptr<FileBackend>> SaveDataArchive::OpenFile(const Path& path,
                                                                  const Mode& mode) const {
    LOG_DEBUG(Service_FS, "called path=%s mode=%01X", path.DebugStr().c_str(), mode.hex);

    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    if (mode.hex == 0) {
        LOG_ERROR(Service_FS, "Empty open mode");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    if (mode.create_flag && !mode.write_flag) {
        LOG_ERROR(Service_FS, "Create flag set but write flag not set");
        return ERROR_UNSUPPORTED_OPEN_FLAGS;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_FILE_NOT_FOUND;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::FileInPath:
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "Unexpected file or directory in %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY;
    case PathParser::NotFound:
        if (!mode.create_flag) {
            LOG_ERROR(Service_FS, "Non-existing file %s can't be open without mode create.",
                      full_path.c_str());
            return ERROR_FILE_NOT_FOUND;
        } else {
            // Create the file
            FileUtil::CreateEmptyFile(full_path);
        }
        break;
    }

    FileUtil::IOFile file(full_path, mode.write_flag ? "r+b" : "rb");
    if (!file.IsOpen()) {
        LOG_CRITICAL(Service_FS, "(unreachable) Unknown error opening %s", full_path.c_str());
        return ERROR_FILE_NOT_FOUND;
    }

    auto disk_file = std::make_unique<DiskFile>(std::move(file), mode);
    return MakeResult<std::unique_ptr<FileBackend>>(std::move(disk_file));
}

ResultCode SaveDataArchive::DeleteFile(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_FILE_NOT_FOUND;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::FileInPath:
    case PathParser::DirectoryFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "File not found %s", full_path.c_str());
        return ERROR_FILE_NOT_FOUND;
    }

    if (FileUtil::Delete(full_path)) {
        return RESULT_SUCCESS;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error deleting %s", full_path.c_str());
    return ERROR_FILE_NOT_FOUND;
}

ResultCode SaveDataArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
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
        return ERROR_DIRECTORY_NOT_EMPTY;

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::FileInPath:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "Unexpected file or directory %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY;
    }

    if (deleter(full_path)) {
        return RESULT_SUCCESS;
    }

    LOG_ERROR(Service_FS, "Directory not empty %s", full_path.c_str());
    return ERROR_DIRECTORY_NOT_EMPTY;
}

ResultCode SaveDataArchive::DeleteDirectory(const Path& path) const {
    return DeleteDirectoryHelper(path, mount_point, FileUtil::DeleteDir);
}

ResultCode SaveDataArchive::DeleteDirectoryRecursively(const Path& path) const {
    return DeleteDirectoryHelper(
        path, mount_point, [](const std::string& p) { return FileUtil::DeleteDirRecursively(p); });
}

ResultCode SaveDataArchive::CreateFile(const FileSys::Path& path, u64 size) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_FILE_NOT_FOUND;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Unexpected file in path %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY;
    case PathParser::DirectoryFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "%s already exists", full_path.c_str());
        return ERROR_FILE_ALREADY_EXISTS;
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

ResultCode SaveDataArchive::CreateDirectory(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_FILE_NOT_FOUND;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Unexpected file in path %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY;
    case PathParser::DirectoryFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "%s already exists", full_path.c_str());
        return ERROR_DIRECTORY_ALREADY_EXISTS;
    }

    if (FileUtil::CreateDir(mount_point + path.AsString())) {
        return RESULT_SUCCESS;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error creating %s", mount_point.c_str());
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                      ErrorLevel::Status);
}

ResultCode SaveDataArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    if (FileUtil::Rename(mount_point + src_path.AsString(), mount_point + dest_path.AsString()))
        return RESULT_SUCCESS;

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                      ErrorSummary::NothingHappened, ErrorLevel::Status);
}

ResultVal<std::unique_ptr<DirectoryBackend>> SaveDataArchive::OpenDirectory(
    const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path %s", path.DebugStr().c_str());
        return ERROR_INVALID_PATH;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point %s", mount_point.c_str());
        return ERROR_FILE_NOT_FOUND;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "Path not found %s", full_path.c_str());
        return ERROR_PATH_NOT_FOUND;
    case PathParser::FileInPath:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "Unexpected file in path %s", full_path.c_str());
        return ERROR_UNEXPECTED_FILE_OR_DIRECTORY;
    }

    auto directory = std::make_unique<DiskDirectory>(full_path);
    return MakeResult<std::unique_ptr<DirectoryBackend>>(std::move(directory));
}

u64 SaveDataArchive::GetFreeBytes() const {
    // TODO: Stubbed to return 1GiB
    return 1024 * 1024 * 1024;
}

} // namespace FileSys
