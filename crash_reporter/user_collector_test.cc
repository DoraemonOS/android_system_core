// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "base/file_util.h"
#include "crash-reporter/system_logging_mock.h"
#include "crash-reporter/user_collector.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"

int s_crashes = 0;
bool s_metrics = false;

static const char kFilePath[] = "/my/path";

void CountCrash() {
  ++s_crashes;
}

bool IsMetrics() {
  return s_metrics;
}

class UserCollectorTest : public ::testing::Test {
  void SetUp() {
    s_crashes = 0;
    collector_.Initialize(CountCrash,
                          kFilePath,
                          IsMetrics,
                          &logging_,
                          false);
    mkdir("test", 0777);
    collector_.set_core_pattern_file("test/core_pattern");
    pid_ = getpid();
  }
 protected:
  void TestEnableOK(bool generate_diagnostics);

  SystemLoggingMock logging_;
  UserCollector collector_;
  pid_t pid_;
};

TEST_F(UserCollectorTest, EnableOK) {
  std::string contents;
  ASSERT_TRUE(collector_.Enable());
  ASSERT_TRUE(file_util::ReadFileToString(FilePath("test/core_pattern"),
                                                   &contents));
  ASSERT_EQ("|/my/path --signal=%s --pid=%p", contents);
  ASSERT_EQ(s_crashes, 0);
  ASSERT_NE(logging_.log().find("Enabling crash handling"), std::string::npos);
}

TEST_F(UserCollectorTest, EnableNoFileAccess) {
  collector_.set_core_pattern_file("/does_not_exist");
  ASSERT_FALSE(collector_.Enable());
  ASSERT_EQ(s_crashes, 0);
  ASSERT_NE(logging_.log().find("Enabling crash handling"), std::string::npos);
  ASSERT_NE(logging_.log().find("Unable to write /does_not_exist"),
            std::string::npos);
}

TEST_F(UserCollectorTest, DisableOK) {
  std::string contents;
  ASSERT_TRUE(collector_.Disable());
  ASSERT_TRUE(file_util::ReadFileToString(FilePath("test/core_pattern"),
                                          &contents));
  ASSERT_EQ("core", contents);
  ASSERT_EQ(s_crashes, 0);
  ASSERT_NE(logging_.log().find("Disabling crash handling"),
            std::string::npos);
}

TEST_F(UserCollectorTest, DisableNoFileAccess) {
  collector_.set_core_pattern_file("/does_not_exist");
  ASSERT_FALSE(collector_.Disable());
  ASSERT_EQ(s_crashes, 0);
  ASSERT_NE(logging_.log().find("Disabling crash handling"), std::string::npos);
  ASSERT_NE(logging_.log().find("Unable to write /does_not_exist"),
            std::string::npos);
}

TEST_F(UserCollectorTest, HandleCrashWithoutMetrics) {
  s_metrics = false;
  collector_.HandleCrash(10, 20, "foobar");
  ASSERT_NE(logging_.log().find(
      "Received crash notification for foobar[20] sig 10"),
      std::string::npos);
  ASSERT_EQ(s_crashes, 0);
}

TEST_F(UserCollectorTest, HandleCrashWithMetrics) {
  s_metrics = true;
  collector_.HandleCrash(2, 5, "chrome");
  ASSERT_NE(logging_.log().find(
      "Received crash notification for chrome[5] sig 2"),
      std::string::npos);
  ASSERT_EQ(s_crashes, 1);
}

TEST_F(UserCollectorTest, GetProcessPath) {
  FilePath path = collector_.GetProcessPath(100);
  ASSERT_EQ("/proc/100", path.value());
}

TEST_F(UserCollectorTest, GetSymlinkTarget) {
  FilePath result;
  ASSERT_FALSE(collector_.GetSymlinkTarget(FilePath("/does_not_exist"),
                                           &result));

  std::string long_link;
  for (int i = 0; i < 50; ++i)
    long_link += "0123456789";
  long_link += "/gold";

  for (size_t len = 1; len <= long_link.size(); ++len) {
    std::string this_link;
    static const char kLink[] = "test/this_link";
    this_link.assign(long_link.c_str(), len);
    ASSERT_EQ(len, this_link.size());
    unlink(kLink);
    ASSERT_EQ(0, symlink(this_link.c_str(), kLink));
    ASSERT_TRUE(collector_.GetSymlinkTarget(FilePath(kLink), &result));
    ASSERT_EQ(this_link, result.value());
  }
}

TEST_F(UserCollectorTest, GetIdFromStatus) {
  int id = 1;
  EXPECT_FALSE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                          UserCollector::kIdEffective,
                                          "nothing here",
                                          &id));
  EXPECT_EQ(id, 1);

  // Not enough parameters.
  EXPECT_FALSE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                          UserCollector::kIdReal,
                                          "line 1\nUid:\t1\n", &id));

  const char valid_contents[] = "\nUid:\t1\t2\t3\t4\nGid:\t5\t6\t7\t8\n";
  EXPECT_TRUE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                         UserCollector::kIdReal,
                                         valid_contents,
                                         &id));
  EXPECT_EQ(1, id);

  EXPECT_TRUE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                         UserCollector::kIdEffective,
                                         valid_contents,
                                         &id));
  EXPECT_EQ(2, id);

  EXPECT_TRUE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                         UserCollector::kIdFileSystem,
                                         valid_contents,
                                         &id));
  EXPECT_EQ(4, id);

  EXPECT_TRUE(collector_.GetIdFromStatus(UserCollector::kGroupId,
                                         UserCollector::kIdEffective,
                                         valid_contents,
                                         &id));
  EXPECT_EQ(6, id);

  EXPECT_TRUE(collector_.GetIdFromStatus(UserCollector::kGroupId,
                                         UserCollector::kIdSet,
                                         valid_contents,
                                         &id));
  EXPECT_EQ(7, id);

  EXPECT_FALSE(collector_.GetIdFromStatus(UserCollector::kGroupId,
                                          UserCollector::IdKind(5),
                                          valid_contents,
                                          &id));
  EXPECT_FALSE(collector_.GetIdFromStatus(UserCollector::kGroupId,
                                          UserCollector::IdKind(-1),
                                          valid_contents,
                                          &id));

  // Fail if junk after number
  EXPECT_FALSE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                          UserCollector::kIdReal,
                                          "Uid:\t1f\t2\t3\t4\n",
                                          &id));
  EXPECT_TRUE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                         UserCollector::kIdReal,
                                         "Uid:\t1\t2\t3\t4\n",
                                         &id));
  EXPECT_EQ(1, id);

  // Fail if more than 4 numbers.
  EXPECT_FALSE(collector_.GetIdFromStatus(UserCollector::kUserId,
                                          UserCollector::kIdReal,
                                          "Uid:\t1\t2\t3\t4\t5\n",
                                          &id));
}

TEST_F(UserCollectorTest, GetUserInfoFromName) {
  gid_t gid = 100;
  uid_t uid = 100;
  EXPECT_TRUE(collector_.GetUserInfoFromName("root", &uid, &gid));
  EXPECT_EQ(0, uid);
  EXPECT_EQ(0, gid);
}

TEST_F(UserCollectorTest, GetCrashDirectoryInfo) {
  FilePath path;
  const int kRootUid = 0;
  const int kRootGid = 0;
  const int kNtpUid = 5;
  const int kChronosUid = 1000;
  const int kChronosGid = 1001;
  const mode_t kExpectedSystemMode = 01755;
  const mode_t kExpectedUserMode = 0755;

  mode_t directory_mode;
  uid_t directory_owner;
  gid_t directory_group;

  path = collector_.GetCrashDirectoryInfo(kRootUid,
                                          kChronosUid,
                                          kChronosGid,
                                          &directory_mode,
                                          &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/var/spool/crash", path.value());
  EXPECT_EQ(kExpectedSystemMode, directory_mode);
  EXPECT_EQ(kRootUid, directory_owner);
  EXPECT_EQ(kRootGid, directory_group);

  path = collector_.GetCrashDirectoryInfo(kNtpUid,
                                          kChronosUid,
                                          kChronosGid,
                                          &directory_mode,
                                          &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/var/spool/crash", path.value());
  EXPECT_EQ(kExpectedSystemMode, directory_mode);
  EXPECT_EQ(kRootUid, directory_owner);
  EXPECT_EQ(kRootGid, directory_group);

  path = collector_.GetCrashDirectoryInfo(kChronosUid,
                                          kChronosUid,
                                          kChronosGid,
                                          &directory_mode,
                                          &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/home/chronos/user/crash", path.value());
  EXPECT_EQ(kExpectedUserMode, directory_mode);
  EXPECT_EQ(kChronosUid, directory_owner);
  EXPECT_EQ(kChronosGid, directory_group);
}

TEST_F(UserCollectorTest, CopyOffProcFilesBadPath) {
  // Try a path that is not writable.
  ASSERT_FALSE(collector_.CopyOffProcFiles(pid_, FilePath("/bad/path")));
  ASSERT_NE(logging_.log().find(
      "Could not create /bad/path"),
            std::string::npos);
}

TEST_F(UserCollectorTest, CopyOffProcFilesBadPid) {
  FilePath container_path("test/container");
  ASSERT_FALSE(collector_.CopyOffProcFiles(0, container_path));
  ASSERT_NE(logging_.log().find(
      "Path /proc/0 does not exist"),
            std::string::npos);
}

TEST_F(UserCollectorTest, CopyOffProcFilesOK) {
  FilePath container_path("test/container");
  ASSERT_TRUE(collector_.CopyOffProcFiles(pid_, container_path));
  ASSERT_EQ(logging_.log().find(
      "Could not copy"), std::string::npos);
  static struct {
    const char *name;
    bool exists;
  } expectations[] = {
    { "auxv", true },
    { "cmdline", true },
    { "environ", true },
    { "maps", true },
    { "mem", false },
    { "mounts", false },
    { "sched", false },
    { "status", true }
  };
  for (unsigned i = 0; i < sizeof(expectations)/sizeof(expectations[0]); ++i) {
    EXPECT_EQ(expectations[i].exists,
              file_util::PathExists(
                  container_path.Append(expectations[i].name)));
  }
}

TEST_F(UserCollectorTest, FormatDumpBasename) {
  struct tm tm = {0};
  tm.tm_sec = 15;
  tm.tm_min = 50;
  tm.tm_hour = 13;
  tm.tm_mday = 23;
  tm.tm_mon = 4;
  tm.tm_year = 110;
  tm.tm_isdst = -1;
  std::string basename =
      collector_.FormatDumpBasename("foo", mktime(&tm), 100);
  ASSERT_EQ("foo.20100523.135015.100", basename);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
