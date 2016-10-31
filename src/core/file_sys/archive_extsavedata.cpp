// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"
#include "core/file_sys/savedata_archive.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/**
 * A modified version of DiskFile for fix-size file used by ExtSaveData
 * The file size can't be changed by SetSize or Write.
 */
class FixSizeDiskFile : public DiskFile {
public:
    FixSizeDiskFile(FileUtil::IOFile&& file, const Mode& mode) : DiskFile(std::move(file), mode) {
        size = GetSize();
    }

    bool SetSize(u64 size) const override {
        return false;
    }

    ResultVal<size_t> Write(u64 offset, size_t length, bool flush,
                            const u8* buffer) const override {
        if (offset > size) {
            return ResultCode(ErrorDescription::FS_WriteBeyondEnd, ErrorModule::FS,
                              ErrorSummary::InvalidArgument, ErrorLevel::Usage);
        } else if (offset == size) {
            return MakeResult<size_t>(0);
        }

        if (offset + length > size) {
            length = size - offset;
        }

        return DiskFile::Write(offset, length, flush, buffer);
    }

private:
    u64 size{};
};

/**
 * Archive backend for general extsave data archive type.
 * The behaviour of ExtSaveDataArchive is almost the same as SaveDataArchive, except for
 *  - file size can't be changed once created (thus creating zero-size file and openning with create
 *    flag are prohibited);
 *  - always open a file with read+write permission.
 */
class ExtSaveDataArchive : public SaveDataArchive {
public:
    ExtSaveDataArchive(const std::string& mount_point) : SaveDataArchive(mount_point) {}

    std::string GetName() const override {
        return "ExtSaveDataArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode& mode) const override {
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

        if (mode.create_flag) {
            LOG_ERROR(Service_FS, "Create flag is not supported");
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
            LOG_ERROR(Service_FS, "%s not found", full_path.c_str());
            return ERROR_FILE_NOT_FOUND;
        }

        FileUtil::IOFile file(full_path, "r+b");
        if (!file.IsOpen()) {
            LOG_CRITICAL(Service_FS, "(unreachable) Unknown error opening %s", full_path.c_str());
            return ERROR_FILE_NOT_FOUND;
        }

        Mode rwmode;
        rwmode.write_flag.Assign(1);
        rwmode.read_flag.Assign(1);
        auto disk_file = std::make_unique<FixSizeDiskFile>(std::move(file), rwmode);
        return MakeResult<std::unique_ptr<FileBackend>>(std::move(disk_file));
    }

    ResultCode CreateFile(const Path& path, u64 size) const override {
        if (size == 0) {
            LOG_ERROR(Service_FS, "Zero-size file is not supported");
            return ERROR_UNSUPPORTED_OPEN_FLAGS;
        }
        return SaveDataArchive::CreateFile(path, size);
    }
};

std::string GetExtSaveDataPath(const std::string& mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();
    const u32* data = reinterpret_cast<const u32*>(vec_data.data());
    u32 save_low = data[1];
    u32 save_high = data[2];
    return Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(), save_high, save_low);
}

std::string GetExtDataContainerPath(const std::string& mount_point, bool shared) {
    if (shared)
        return Common::StringFromFormat("%sdata/%s/extdata/", mount_point.c_str(),
                                        SYSTEM_ID.c_str());

    return Common::StringFromFormat("%sNintendo 3DS/%s/%s/extdata/", mount_point.c_str(),
                                    SYSTEM_ID.c_str(), SDCARD_ID.c_str());
}

Path ConstructExtDataBinaryPath(u32 media_type, u32 high, u32 low) {
    std::vector<u8> binary_path;
    binary_path.reserve(12);

    // Append each word byte by byte

    // The first word is the media type
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((media_type >> (8 * i)) & 0xFF);

    // Next is the low word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((low >> (8 * i)) & 0xFF);

    // Next is the high word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((high >> (8 * i)) & 0xFF);

    return {binary_path};
}

ArchiveFactory_ExtSaveData::ArchiveFactory_ExtSaveData(const std::string& mount_location,
                                                       bool shared)
    : shared(shared), mount_point(GetExtDataContainerPath(mount_location, shared)) {
    LOG_INFO(Service_FS, "Directory %s set as base for ExtSaveData.", mount_point.c_str());
}

bool ArchiveFactory_ExtSaveData::Initialize() {
    if (!FileUtil::CreateFullPath(mount_point)) {
        LOG_ERROR(Service_FS, "Unable to create ExtSaveData base path.");
        return false;
    }

    return true;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_ExtSaveData::Open(const Path& path) {
    std::string fullpath = GetExtSaveDataPath(mount_point, path) + "user/";
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Verify the archive behavior of SharedExtSaveData compared to ExtSaveData.
        // ExtSaveData seems to return FS_NotFound (120) when the archive doesn't exist.
        if (!shared) {
            return ResultCode(ErrorDescription::FS_NotFound, ErrorModule::FS,
                              ErrorSummary::InvalidState, ErrorLevel::Status);
        } else {
            return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
                              ErrorSummary::InvalidState, ErrorLevel::Status);
        }
    }
    auto archive = std::make_unique<ExtSaveDataArchive>(fullpath);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_ExtSaveData::Format(const Path& path,
                                              const FileSys::ArchiveFormatInfo& format_info) {
    // These folders are always created with the ExtSaveData
    std::string user_path = GetExtSaveDataPath(mount_point, path) + "user/";
    std::string boss_path = GetExtSaveDataPath(mount_point, path) + "boss/";
    FileUtil::CreateFullPath(user_path);
    FileUtil::CreateFullPath(boss_path);

    // Write the format metadata
    std::string metadata_path = GetExtSaveDataPath(mount_point, path) + "metadata";
    FileUtil::IOFile file(metadata_path, "wb");

    if (!file.IsOpen()) {
        // TODO(Subv): Find the correct error code
        return ResultCode(-1);
    }

    file.WriteBytes(&format_info, sizeof(format_info));
    return RESULT_SUCCESS;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_ExtSaveData::GetFormatInfo(const Path& path) const {
    std::string metadata_path = GetExtSaveDataPath(mount_point, path) + "metadata";
    FileUtil::IOFile file(metadata_path, "rb");

    if (!file.IsOpen()) {
        LOG_ERROR(Service_FS, "Could not open metadata information for archive");
        // TODO(Subv): Verify error code
        return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    ArchiveFormatInfo info = {};
    file.ReadBytes(&info, sizeof(info));
    return MakeResult<ArchiveFormatInfo>(info);
}

void ArchiveFactory_ExtSaveData::WriteIcon(const Path& path, const u8* icon_data,
                                           size_t icon_size) {
    std::string game_path = FileSys::GetExtSaveDataPath(GetMountPoint(), path);
    FileUtil::IOFile icon_file(game_path + "icon", "wb");
    icon_file.WriteBytes(icon_data, icon_size);
}

} // namespace FileSys
