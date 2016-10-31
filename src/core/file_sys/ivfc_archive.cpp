// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/file_sys/ivfc_archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

std::string IVFCArchive::GetName() const {
    return "IVFC";
}

ResultVal<std::unique_ptr<FileBackend>> IVFCArchive::OpenFile(const Path& path,
                                                              const Mode& mode) const {
    return MakeResult<std::unique_ptr<FileBackend>>(
        std::make_unique<IVFCFile>(romfs_file, data_offset, data_size, aes_context));
}

ResultCode IVFCArchive::DeleteFile(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a file from an IVFC archive (%s).",
                 GetName().c_str());
    // TODO(Subv): Verify error code
    return ResultCode(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                      ErrorLevel::Status);
}

ResultCode IVFCArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an IVFC archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode IVFCArchive::DeleteDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an IVFC archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode IVFCArchive::DeleteDirectoryRecursively(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an IVFC archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode IVFCArchive::CreateFile(const Path& path, u64 size) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a file in an IVFC archive (%s).",
                 GetName().c_str());
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
}

ResultCode IVFCArchive::CreateDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a directory in an IVFC archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultCode IVFCArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an IVFC archive (%s).",
                 GetName().c_str());
    // TODO(wwylele): Use correct error code
    return ResultCode(-1);
}

ResultVal<std::unique_ptr<DirectoryBackend>> IVFCArchive::OpenDirectory(const Path& path) const {
    return MakeResult<std::unique_ptr<DirectoryBackend>>(std::make_unique<IVFCDirectory>());
}

u64 IVFCArchive::GetFreeBytes() const {
    LOG_WARNING(Service_FS, "Attempted to get the free space in an IVFC archive");
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

u8 IVFCFile::ReadEncryptedByte(u64 offset) const {
    u64 index = offset / 16;
    u8 buf_offset = offset % 16;
    if (!decrypt_buffered || index != buf_index) {
        decrypt_buffered = true;
        buf_index = index;
        std::array<u8, 16> ctr = aes_context.ctr;
        AES::AddCtr(ctr, index);
        std::array<u8, 16> xorpad = AES::AesCipher(ctr, aes_context.key);
        romfs_file->Seek(data_offset + index * 16, SEEK_SET);
        romfs_file->ReadBytes(decrypt_buf.data(), 16);
        for (int i = 0; i < 16; ++i)
            decrypt_buf[i] ^= xorpad[i];
    }
    return decrypt_buf[buf_offset];
}

ResultVal<size_t> IVFCFile::Read(const u64 offset, const size_t length, u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset=%llu, length=%zu", offset, length);
    if (!romfs_file)
        return MakeResult<size_t>(0);
    size_t read_length = (size_t)std::min((u64)length, data_size - offset);
    if (aes_context.encrypted) {
        for (size_t i = 0; i < read_length; ++i) {
            buffer[i] = ReadEncryptedByte(offset + i);
        }
        return MakeResult<size_t>(read_length);
    } else {
        romfs_file->Seek(data_offset + offset, SEEK_SET);
        return MakeResult<size_t>(romfs_file->ReadBytes(buffer, read_length));
    }
}

ResultVal<size_t> IVFCFile::Write(const u64 offset, const size_t length, const bool flush,
                                  const u8* buffer) const {
    LOG_ERROR(Service_FS, "Attempted to write to IVFC file");
    // TODO(Subv): Find error code
    return MakeResult<size_t>(0);
}

u64 IVFCFile::GetSize() const {
    return data_size;
}

bool IVFCFile::SetSize(const u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to set the size of an IVFC file");
    return false;
}

} // namespace FileSys
