/*
 * Leapdesk KVM -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Leapdesk KVM contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/DataDirectories.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

namespace inputleap {

namespace {

// a folder of its own in the temporary folder, removed with everything in it
class TempDir
{
public:
    TempDir() :
        path_{fs::temp_directory_path() /
              ("leapdesk-test-" + std::to_string(std::random_device{}()))}
    {
        fs::create_directories(path_);
    }

    ~TempDir()
    {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void write_file(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream;
    open_utf8_path(stream, path, std::ios_base::out | std::ios_base::binary);
    stream << text;
}

std::string read_file(const fs::path& path)
{
    std::ifstream stream;
    open_utf8_path(stream, path, std::ios_base::in | std::ios_base::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

} // namespace

TEST(DataDirectoriesTests, maybeCopyOldProfile_copiesInputLeapProfileAndRenamesCertificate)
{
    TempDir temp;
    auto old_profile = temp.path() / "InputLeap";
    auto profile = temp.path() / "Leapdesk";
    write_file(old_profile / "SSL" / "InputLeap.pem", "certificate");
    write_file(old_profile / "SSL" / "Fingerprints" / "TrustedServers.txt", "v2:sha256:00");

    DataDirectories::maybe_copy_old_profile(old_profile, profile);

    EXPECT_EQ(read_file(profile / "SSL" / "Leapdesk.pem"), "certificate");
    EXPECT_FALSE(fs::exists(profile / "SSL" / "InputLeap.pem"));
    EXPECT_EQ(read_file(profile / "SSL" / "Fingerprints" / "TrustedServers.txt"), "v2:sha256:00");
    EXPECT_TRUE(fs::exists(old_profile / "SSL" / "InputLeap.pem"));
}

TEST(DataDirectoriesTests, maybeCopyOldProfile_renamesBarrierCertificate)
{
    TempDir temp;
    auto old_profile = temp.path() / "barrier";
    auto profile = temp.path() / "Leapdesk";
    write_file(old_profile / "SSL" / "Barrier.pem", "certificate");

    DataDirectories::maybe_copy_old_profile(old_profile, profile);

    EXPECT_EQ(read_file(profile / "SSL" / "Leapdesk.pem"), "certificate");
    EXPECT_FALSE(fs::exists(profile / "SSL" / "Barrier.pem"));
}

TEST(DataDirectoriesTests, maybeCopyOldProfile_givesExistingProfileOnlyTheMissingCertificate)
{
    TempDir temp;
    auto old_profile = temp.path() / "InputLeap";
    auto profile = temp.path() / "Leapdesk";
    write_file(old_profile / "SSL" / "InputLeap.pem", "certificate");
    write_file(old_profile / "InputLeap.conf", "old settings");
    write_file(profile / "Leapdesk.conf", "settings");

    DataDirectories::maybe_copy_old_profile(old_profile, profile);

    EXPECT_EQ(read_file(profile / "SSL" / "Leapdesk.pem"), "certificate");
    EXPECT_FALSE(fs::exists(profile / "InputLeap.conf"));
    EXPECT_EQ(read_file(profile / "Leapdesk.conf"), "settings");
}

TEST(DataDirectoriesTests, maybeCopyOldProfile_keepsExistingCertificate)
{
    TempDir temp;
    auto old_profile = temp.path() / "InputLeap";
    auto profile = temp.path() / "Leapdesk";
    write_file(old_profile / "SSL" / "InputLeap.pem", "old certificate");
    write_file(profile / "SSL" / "Leapdesk.pem", "certificate");

    DataDirectories::maybe_copy_old_profile(old_profile, profile);

    EXPECT_EQ(read_file(profile / "SSL" / "Leapdesk.pem"), "certificate");
    EXPECT_FALSE(fs::exists(profile / "SSL" / "InputLeap.pem"));
}

TEST(DataDirectoriesTests, maybeCopyOldProfile_doesNothingWithoutOldProfile)
{
    TempDir temp;
    auto profile = temp.path() / "Leapdesk";

    DataDirectories::maybe_copy_old_profile(temp.path() / "InputLeap", profile);

    EXPECT_FALSE(fs::exists(profile));
}

} // namespace inputleap
