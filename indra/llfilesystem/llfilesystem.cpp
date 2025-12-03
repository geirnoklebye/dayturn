/**
 * @file filesystem.cpp
 * @brief Implementation of local file system operations.
 * @Note The initial implementation does actually use standard C++
 *       file operations but eventually, there will be another
 *       layer that caches and manages file meta data too.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "lldir.h"
#include "llfile.h"
#include "llfilesystem.h"
#include "lldiskcache.h"

#include "boost/filesystem.hpp"

constexpr S32 LLFileSystem::READ        = 0x00000001;
constexpr S32 LLFileSystem::WRITE       = 0x00000002;
constexpr S32 LLFileSystem::READ_WRITE  = 0x00000003;  // LLFileSystem::READ & LLFileSystem::WRITE
constexpr S32 LLFileSystem::APPEND      = 0x00000006;  // 0x00000004 & LLFileSystem::WRITE


LLFileSystem::LLFileSystem(const LLUUID& file_id, const LLAssetType::EType file_type, S32 mode)
{
    mFileType = file_type;
    mFileID = file_id;
    mPosition = 0;
    mBytesRead = 0;
    mMode = mode;

    // This block of code was originally called in the read() method but after comments here:
    // https://bitbucket.org/lindenlab/viewer/commits/e28c1b46e9944f0215a13cab8ee7dded88d7fc90#comment-10537114
    // we decided to follow Henri's suggestion and move the code to update the last access time here.
    if (mode == LLFileSystem::READ)
    {
        // build the filename (TODO: we do this in a few places - perhaps we should factor into a single function)
        const std::string filename = LLDiskCache::metaDataToFilepath(mFileID, mFileType);

        // update the last access time for the file if it exists - this is required
        // even though we are reading and not writing because this is the
        // way the cache works - it relies on a valid "last accessed time" for
        // each file so it knows how to remove the oldest, unused files
        //
        // One stat serves both purposes: it tells us the file exists, and it
        // carries the mtime updateFileAccessTime() needs in order to decide
        // whether a rewrite is due. This used to be two stats - one via
        // gDirUtilp->fileExists(), then another inside updateFileAccessTime() -
        // on a path that runs for every asset read.
        llstat st;
        if (LLFile::stat(filename, &st) == 0)
        {
            updateFileAccessTime(filename, st.st_mtime);
        }
    }
}

// static
bool LLFileSystem::getExists(const LLUUID& file_id, const LLAssetType::EType file_type)
{
    const std::string filename = LLDiskCache::metaDataToFilepath(file_id, file_type);

    // One stat answers all three questions asked here - does it exist, is it a
    // regular file, is it non-empty. This used to be three separate calls, so
    // three stats, and the result is not cached anywhere: getSize() routes
    // through getFileSize() on every call, and callers ask repeatedly (seek()
    // asks per seek; llxfer_vfile asks three times in one function).
    llstat st;
    if (LLFile::stat(filename, &st) == 0 && S_ISREG(st.st_mode))
    {
        return st.st_size > 0;
    }
    return false;
}

// static
bool LLFileSystem::removeFile(const LLUUID& file_id, const LLAssetType::EType file_type, int suppress_warning /*= 0*/)
{
    const std::string filename = LLDiskCache::metaDataToFilepath(file_id, file_type);

    LLFile::remove(filename.c_str(), suppress_warning);

    return true;
}

// static
bool LLFileSystem::renameFile(const LLUUID& old_file_id, const LLAssetType::EType old_file_type,
                              const LLUUID& new_file_id, const LLAssetType::EType new_file_type)
{
    const std::string old_filename = LLDiskCache::metaDataToFilepath(old_file_id, old_file_type);
    const std::string new_filename = LLDiskCache::metaDataToFilepath(new_file_id, new_file_type);

    if (LLFile::rename(old_filename, new_filename) != 0)
    {
        // We would like to return false here indicating the operation
        // failed but the original code does not and doing so seems to
        // break a lot of things so we go with the flow...
        //return false;
        LL_WARNS() << "Failed to rename " << old_file_id << " to " << new_file_id << " reason: " << strerror(errno) << LL_ENDL;
    }

    return true;
}

// static
S32 LLFileSystem::getFileSize(const LLUUID& file_id, const LLAssetType::EType file_type)
{
    const std::string filename = LLDiskCache::metaDataToFilepath(file_id, file_type);

    // As getExists() above: one stat instead of three.
    llstat st;
    if (LLFile::stat(filename, &st) == 0 && S_ISREG(st.st_mode))
    {
        return static_cast<S32>(st.st_size);
    }
    return 0;
}

bool LLFileSystem::read(U8* buffer, S32 bytes)
{
    bool success = false;

    const std::string filename = LLDiskCache::metaDataToFilepath(mFileID, mFileType);

    llifstream file(filename, std::ios::binary);
    if (file.is_open())
    {
        file.seekg(mPosition, std::ios::beg);

        file.read(reinterpret_cast<char*>(buffer), bytes);

        if (file)
        {
            mBytesRead = bytes;
        }
        else
        {
            mBytesRead = static_cast<S32>(file.gcount());
        }

        file.close();

        mPosition += mBytesRead;
        if (mBytesRead)
        {
            success = true;
        }
    }

    return success;
}

S32 LLFileSystem::getLastBytesRead() const
{
    return mBytesRead;
}

bool LLFileSystem::eof() const
{
    return mPosition >= getSize();
}

bool LLFileSystem::write(const U8* buffer, S32 bytes)
{
    const std::string filename = LLDiskCache::metaDataToFilepath(mFileID, mFileType);

    bool success = false;

    // Note: every branch below flushes and then checks the stream state before
    // reporting success. Writes are buffered, so a failure (a full disk, most
    // obviously) does not necessarily surface at the write() call itself, and
    // claiming success for a write that never landed puts a truncated asset in
    // the cache that later reads will happily serve.
    if (mMode == APPEND)
    {
        llofstream ofs(filename, std::ios::app | std::ios::binary);
        if (ofs)
        {
            ofs.write(reinterpret_cast<const char*>(buffer), bytes);
            ofs.flush();

            if (ofs.good())
            {
                mPosition = static_cast<S32>(ofs.tellp());
                success = true;
            }
        }
    }
    else if (mMode == READ_WRITE)
    {
        // Don't truncate if file already exists
        llofstream ofs(filename, std::ios::in | std::ios::binary);
        if (ofs)
        {
            ofs.seekp(mPosition, std::ios::beg);
            ofs.write(reinterpret_cast<const char*>(buffer), bytes);
            ofs.flush();

            if (ofs.good())
            {
                mPosition += bytes;
                success = true;
            }
        }
        else
        {
            // File doesn't exist - open in write mode
            ofs.open(filename, std::ios::binary);
            if (ofs.is_open())
            {
                ofs.write(reinterpret_cast<const char*>(buffer), bytes);
                ofs.flush();

                if (ofs.good())
                {
                    mPosition += bytes;
                    success = true;
                }
            }
        }
    }
    else
    {
        // Truncate only on the first write to this handle. This method reopens the
        // file on every call, so opening with ios::out unconditionally would discard
        // everything written so far while mPosition advanced regardless -- leaving
        // tell() plausible and the file wrong. A chunked writer therefore ended up
        // with only its final write on disk. Single-write callers, which are all the
        // others in this tree, still truncate exactly as before.
        const bool truncate = (mPosition == 0);
        llofstream ofs(filename, truncate ? std::ios::binary
                                          : (std::ios::in | std::ios::binary));
        if (ofs)
        {
            if (!truncate)
            {
                ofs.seekp(mPosition, std::ios::beg);
            }

            ofs.write(reinterpret_cast<const char*>(buffer), bytes);
            ofs.flush();

            if (ofs.good())
            {
                mPosition += bytes;
                success = true;
            }
        }
    }

    return success;
}

bool LLFileSystem::seek(S32 offset, S32 origin)
{
    if (-1 == origin)
    {
        origin = mPosition;
    }

    S32 new_pos = origin + offset;

    S32 size = getSize();

    if (new_pos > size)
    {
        LL_WARNS() << "Attempt to seek past end of file" << LL_ENDL;

        mPosition = size;
        return false;
    }
    else if (new_pos < 0)
    {
        LL_WARNS() << "Attempt to seek past beginning of file" << LL_ENDL;

        mPosition = 0;
        return false;
    }

    mPosition = new_pos;
    return true;
}

S32 LLFileSystem::tell() const
{
    return mPosition;
}

S32 LLFileSystem::getSize() const
{
    return LLFileSystem::getFileSize(mFileID, mFileType);
}

S32 LLFileSystem::getMaxSize() const
{
    // offer up a huge size since we don't care what the max is
    return INT_MAX;
}

bool LLFileSystem::rename(const LLUUID& new_id, const LLAssetType::EType new_type)
{
    LLFileSystem::renameFile(mFileID, mFileType, new_id, new_type);

    mFileID = new_id;
    mFileType = new_type;

    return true;
}

bool LLFileSystem::remove() const
{
    LLFileSystem::removeFile(mFileID, mFileType);
    return true;
}


void LLFileSystem::updateFileAccessTime(const std::string& file_path, std::time_t last_write_time)
{
    /**
     * Threshold in time_t units that is used to decide if the last access time
     * time of the file is updated or not. Added as a precaution for the concern
     * outlined in SL-14582  about frequent writes on older SSDs reducing their
     * lifespan. I think this is the right place for the threshold value - rather
     * than it being a pref - do comment on that Jira if you disagree...
     *
     * Let's start with 1 hour in time_t units and see how that unfolds
     */
    constexpr std::time_t time_threshold = std::time_t(1) * 60 * 60;

    // current time
    const std::time_t cur_time = std::time(nullptr);

    // delta between cur time and last time the file was written. The caller has
    // already stat'ed the file to establish that it exists, so it hands us the
    // mtime rather than making us read it back a second time - see the
    // constructor, which is on the per-asset read path.
    const std::time_t delta_time = cur_time - last_write_time;

    // we only write the new value if the time in time_threshold has elapsed
    // before the last one
    if (delta_time > time_threshold)
    {
        boost::system::error_code ec;
#if LL_WINDOWS
        boost::filesystem::last_write_time(ll_convert<std::wstring>(file_path), cur_time, ec);
#else
        boost::filesystem::last_write_time(file_path, cur_time, ec);
#endif

        if (ec.failed())
        {
            LL_WARNS() << "Failed to update last write time for cache file " << file_path << ": " << ec.message() << LL_ENDL;
        }
    }
}
