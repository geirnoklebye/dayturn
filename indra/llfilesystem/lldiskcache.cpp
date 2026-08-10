/**
 * @file lldiskcache.cpp
 * @brief The disk cache implementation.
 *
 * Note: Rather than keep the top level function comments up
 * to date in both the source and header files, I elected to
 * only have explicit comments about each function and variable
 * in the header - look there for details. The same is true for
 * description of how this code is supposed to work.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2020, Linden Research, Inc.
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
#include "llapp.h"
#include "llassettype.h"
#include "lldir.h"
#include <boost/filesystem.hpp>
#include <algorithm>
#include <chrono>
#include <vector>

#include "lldiskcache.h"

 /**
  * The prefix inserted at the start of a cache file filename to
  * help identify it as a cache file. It's probably not required
  * (just the presence in the cache folder is enough) but I am
  * paranoid about the cache folder being set to something bad
  * like the users' OS system dir by mistake or maliciously and
  * this will help to offset any damage if that happens.
  *
  * Note: upstream uses "sl_cache". Renamed here since this viewer has no
  * Second Life support. Everything that matches on a prefix -- purge(),
  * clearCache(), dirFileSize() -- goes through isCacheFilename() below, so this
  * is the only place it is defined. Caches written by earlier builds under the
  * old prefix are cleared by the DiskCacheVersion bump that accompanied the
  * rename; see LEGACY_CACHE_FILENAME_PREFIX.
  */
static const std::string CACHE_FILENAME_PREFIX("dt_cache");

/**
 * The prefix earlier builds of this viewer wrote, before the cache was sharded.
 * Nothing constructs a path under it any more, so entries named this way are
 * unreachable; clearCache() matches it as well as the current prefix so that a
 * DiskCacheVersion bump actually empties the directory rather than leaving the
 * old generation behind forever. See LLAppViewer::getDiskCacheVersion().
 */
static const std::string LEGACY_CACHE_FILENAME_PREFIX("sl_cache");

/**
 * The characters naming the shard subdirectories. A cache entry lives in the
 * subdirectory named for the first hex digit of its ID.
 */
static const std::string CACHE_SUBDIR_CHARS("0123456789abcdef");

/**
 * Purge is skipped entirely until the cache exceeds HIGH_WATER of its maximum
 * size, and then trims down to LOW_WATER rather than to the maximum. Trimming
 * to exactly the maximum means the next asset written puts the cache back over
 * the limit, so a full cache gets rescanned and re-trimmed on every pass.
 */
static constexpr F64 CACHE_HIGH_WATER_PERCENT = 95.0;
static constexpr F64 CACHE_LOW_WATER_PERCENT = 70.0;

std::string LLDiskCache::sCacheDir;

LLDiskCache::LLDiskCache(const std::string& cache_dir,
                         const uintmax_t max_size_bytes,
                         const bool enable_cache_debug_info) :
    mMaxSizeBytes(max_size_bytes),
    mEnableCacheDebugInfo(enable_cache_debug_info)
{
    sCacheDir = cache_dir;
    createCacheDirs(cache_dir);
}

void LLDiskCache::createCacheDirs(const std::string& cache_dir)
{
    LLFile::mkdir(cache_dir);

    for (const char subdir : CACHE_SUBDIR_CHARS)
    {
        LLFile::mkdir(cache_dir + gDirUtilp->getDirDelimiter() + subdir);
    }
}

// Does this filename belong to the cache? Only names carrying one of our
// prefixes are ever deleted, so that a cache directory mistakenly pointed at
// something else cannot be emptied out from under the user.
static bool isCacheFilename(const std::string& filename, bool include_legacy)
{
    return filename.rfind(CACHE_FILENAME_PREFIX, 0) == 0
        || (include_legacy && filename.rfind(LEGACY_CACHE_FILENAME_PREFIX, 0) == 0);
}

// WARNING: purge() is called by LLPurgeDiskCacheThread. As such it must
// NOT touch any LLDiskCache data without introducing and locking a mutex!

// Interaction through the filesystem itself should be safe. Let’s say thread
// A is accessing the cache file for reading/writing and thread B is trimming
// the cache. Let’s also assume using llifstream to open a file and
// boost::filesystem::remove are not atomic (which will be pretty much the
// case).

// Now, A is trying to open the file using llifstream ctor. It does some
// checks if the file exists and whatever else it might be doing, but has not
// issued the call to the OS to actually open the file yet. Now B tries to
// delete the file: If the file has been already marked as in use by the OS,
// deleting the file will fail and B will continue with the next file. A can
// safely continue opening the file. If the file has not yet been marked as in
// use, B will delete the file. Now A actually wants to open it, operation
// will fail, subsequent check via llifstream.is_open will fail, asset will
// have to be re-requested. (Assuming here the viewer will actually handle
// this situation properly, that can also happen if there is a file containing
// garbage.)

// Other situation: B is trimming the cache and A wants to read a file that is
// about to get deleted. boost::filesystem::remove does whatever it is doing
// before actually deleting the file. If A opens the file before the file is
// actually gone, the OS call from B to delete the file will fail since the OS
// will prevent this. B continues with the next file. If the file is already
// gone before A finally gets to open it, this operation will fail and the
// asset will have to be re-requested.
void LLDiskCache::purge()
{
    if (mEnableCacheDebugInfo)
    {
        LL_INFOS() << "Total dir size before purge is " << dirFileSize(sCacheDir) << LL_ENDL;
    }

    boost::system::error_code ec;
    auto start_time = std::chrono::high_resolution_clock::now();

    typedef std::pair<std::time_t, std::pair<uintmax_t, std::string>> file_info_t;
    std::vector<file_info_t> file_info;

    uintmax_t file_size_total = 0;

#if LL_WINDOWS
    std::wstring cache_path(ll_convert<std::wstring>(sCacheDir));
#else
    std::string cache_path(sCacheDir);
#endif
    // Sharded, so this must recurse into the subdirectories.
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
        boost::filesystem::recursive_directory_iterator iter(cache_path, ec);
        while (iter != boost::filesystem::recursive_directory_iterator() && !ec.failed())
        {
            if (!LLApp::isRunning())
            {
                return;
            }

            // A per-entry failure is cleared before the loop condition is
            // tested again. It used to be left set, so the first unreadable
            // entry silently abandoned the rest of the scan - which matters
            // more now that the running total decides whether a purge happens
            // at all, not just which files it picks.
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                // Legacy entries are excluded here on purpose: nothing reads
                // them, so ageing them out against the water marks would let
                // them evict live entries. clearCache() is what removes them.
                if (isCacheFilename((*iter).path().filename().string(), false))
                {
                    const uintmax_t file_size = boost::filesystem::file_size(*iter, ec);
                    const std::time_t file_time = ec.failed() ? 0 : boost::filesystem::last_write_time(*iter, ec);

                    if (!ec.failed())
                    {
                        file_size_total += file_size;
                        file_info.push_back(file_info_t(file_time, { file_size, (*iter).path().string() }));
                    }
                }
            }

            ec.clear();
            iter.increment(ec);
        }
    }

    const uintmax_t high_water_bytes =
        static_cast<uintmax_t>(static_cast<F64>(mMaxSizeBytes) * (CACHE_HIGH_WATER_PERCENT / 100.0));

    if (file_size_total <= high_water_bytes)
    {
        if (mEnableCacheDebugInfo)
        {
            LL_INFOS() << "Cache is " << file_size_total << "/" << mMaxSizeBytes
                       << " bytes, below the high water mark of " << high_water_bytes
                       << " - nothing to purge" << LL_ENDL;
        }
        return;
    }

    // Oldest first, so the files we delete are at the front and we can stop
    // as soon as we are back under the low water mark.
    std::sort(file_info.begin(), file_info.end(), [](const file_info_t& x, const file_info_t& y)
    {
        return x.first < y.first;
    });

    const uintmax_t target_bytes =
        static_cast<uintmax_t>(static_cast<F64>(mMaxSizeBytes) * (CACHE_LOW_WATER_PERCENT / 100.0));

    LL_INFOS() << "Purging cache from " << file_size_total << " down to a maximum of "
               << target_bytes << " bytes" << LL_ENDL;

    uintmax_t deleted_size_total = 0;
    size_t deleted_count = 0;
    for (const file_info_t& entry : file_info)
    {
        if (!LLApp::isRunning())
        {
            return;
        }

        if ((file_size_total - deleted_size_total) <= target_bytes)
        {
            break;
        }

        boost::filesystem::remove(entry.second.second, ec);
        if (ec.failed())
        {
            LL_WARNS() << "Failed to delete cache file " << entry.second.second << ": " << ec.message() << LL_ENDL;
            ec.clear();
            continue;
        }

        deleted_size_total += entry.second.first;
        ++deleted_count;

        if (mEnableCacheDebugInfo)
        {
            // have to do this because of LL_INFO/LL_END weirdness
            std::ostringstream line;

            line << "DELETE:  ";
            line << entry.first << "  ";
            line << entry.second.first << "  ";
            line << entry.second.second;
            line << " (" << (file_size_total - deleted_size_total) << "/" << mMaxSizeBytes << ")";
            LL_INFOS() << line.str() << LL_ENDL;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto execute_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    LL_INFOS() << "Purged " << deleted_count << " of " << file_info.size() << " files ("
               << deleted_size_total << " bytes) in " << execute_time << " ms; dir size is now "
               << (file_size_total - deleted_size_total) << " bytes" << LL_ENDL;
}

const std::string LLDiskCache::metaDataToFilepath(const LLUUID& id, LLAssetType::EType at)
{
    const std::string id_str = id.asString();

    // Shard on the first hex digit of the ID. asString() is never empty for a
    // well-formed UUID, but a null UUID still yields "00000000-..." so there
    // is always a character to index.
    return llformat("%s%s%c%s%s_%s_0.asset",
                    sCacheDir.c_str(),
                    gDirUtilp->getDirDelimiter().c_str(),
                    id_str[0],
                    gDirUtilp->getDirDelimiter().c_str(),
                    CACHE_FILENAME_PREFIX.c_str(),
                    id_str.c_str());
}

const std::string LLDiskCache::getCacheInfo()
{
    std::ostringstream cache_info;

    double max_in_mb = static_cast<double>(mMaxSizeBytes) / (1024.0 * 1024.0);
    double percent_used = ((F32)dirFileSize(sCacheDir) / (F32)mMaxSizeBytes) * 100.0f;

    cache_info << std::fixed;
    cache_info << std::setprecision(1);
    cache_info << "Max size " << max_in_mb << " MB ";
    cache_info << "(" << percent_used << "% used)";

    return cache_info.str();
}

void LLDiskCache::clearCache()
{
    /**
     * See notes on performance in dirFileSize(..) - there may be
     * a quicker way to do this by operating on the parent dir vs
     * the component files but it's called infrequently so it's
     * likely just fine
     */
    boost::system::error_code ec;
#if LL_WINDOWS
    std::wstring cache_path(ll_convert<std::wstring>(sCacheDir));
#else
    std::string cache_path(sCacheDir);
#endif
    // Collect first, delete afterwards. Deleting the entry the iterator is
    // standing on breaks recursive_directory_iterator: its increment() stats
    // the current entry to decide whether to descend into it, that stat fails
    // with ENOENT on the file we just removed, and the error ends the walk.
    // The result is that exactly one file gets deleted per call, silently -
    // no warning, since the removal itself succeeded. A plain
    // directory_iterator does not look at the current entry on increment,
    // which is why the unsharded version of this loop could get away with it.
    std::vector<std::string> doomed;

    // Sharded, so this must recurse into the subdirectories.
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
        boost::filesystem::recursive_directory_iterator iter(cache_path, ec);
        while (iter != boost::filesystem::recursive_directory_iterator() && !ec.failed())
        {
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                // Legacy entries included: this is the one path that clears
                // them out, and it is what a DiskCacheVersion bump reaches.
                if (isCacheFilename((*iter).path().filename().string(), true))
                {
                    doomed.push_back((*iter).path().string());
                }
            }

            ec.clear();
            iter.increment(ec);
        }
    }

    for (const std::string& file_path : doomed)
    {
        boost::filesystem::remove(file_path, ec);
        if (ec.failed())
        {
            LL_WARNS() << "Failed to delete cache file " << file_path << ": " << ec.message() << LL_ENDL;
            ec.clear();
        }
    }

    LL_INFOS() << "Cleared " << doomed.size() << " files from the disk cache" << LL_ENDL;

    // The shard subdirectories are not removed above, but recreate them in
    // case the whole cache directory was removed out from under us.
    createCacheDirs(sCacheDir);
}

uintmax_t LLDiskCache::dirFileSize(const std::string& dir)
{
    uintmax_t total_file_size = 0;

    /**
     * There may be a better way that works directly on the folder (similar to
     * right clicking on a folder in the OS and asking for size vs right clicking
     * on all files and adding up manually) but this is very fast - less than 100ms
     * for 10,000 files in my testing so, so long as it's not called frequently,
     * it should be okay. Note that's it's only currently used for logging/debugging
     * so if performance is ever an issue, optimizing this or removing it altogether,
     * is an easy win.
     */
    boost::system::error_code ec;
#if LL_WINDOWS
    std::wstring dir_path(ll_convert<std::wstring>(dir));
#else
    std::string dir_path(dir);
#endif
    // Sharded, so this must recurse into the subdirectories.
    if (boost::filesystem::is_directory(dir_path, ec) && !ec.failed())
    {
        boost::filesystem::recursive_directory_iterator iter(dir_path, ec);
        while (iter != boost::filesystem::recursive_directory_iterator() && !ec.failed())
        {
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                // As purge(): legacy entries do not count against the budget,
                // since they are not part of the live cache.
                if (isCacheFilename((*iter).path().filename().string(), false))
                {
                    uintmax_t file_size = boost::filesystem::file_size(*iter, ec);
                    if (!ec.failed())
                    {
                        total_file_size += file_size;
                    }
                }
            }

            ec.clear();
            iter.increment(ec);
        }
    }

    return total_file_size;
}

LLPurgeDiskCacheThread::LLPurgeDiskCacheThread() :
    LLThread("PurgeDiskCacheThread", nullptr)
{
}

void LLPurgeDiskCacheThread::run()
{
    constexpr std::chrono::seconds CHECK_INTERVAL{60};

    while (LLApp::instance()->sleep(CHECK_INTERVAL))
    {
        LLDiskCache::instance().purge();
    }
}
