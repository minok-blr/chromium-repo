// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#import "chrome/updater/mac/mac_util.h"
#endif

namespace updater {
namespace {

const char kHexString[] = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
struct Charmap {
  bool Contains(unsigned char c) const {
    return ((map[c >> 5] & (1 << (c & 31))) != 0);
  }

  uint32_t map[8] = {};
};

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
constexpr Charmap kQueryCharmap = {{0xffffffffL, 0xfc00987dL, 0x78000001L,
                                    0xb8000001L, 0xffffffffL, 0xffffffffL,
                                    0xffffffffL, 0xffffffffL}};

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20. And if keep_escaped is true, %XX will be kept as it is, otherwise, if
// '%' is in the charmap, it is converted to %25.
std::string Escape(base::StringPiece text,
                   const Charmap& charmap,
                   bool use_plus,
                   bool keep_escaped = false) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (keep_escaped && '%' == c && i + 2 < text.length() &&
               base::IsHexDigit(text[i + 1]) && base::IsHexDigit(text[i + 2])) {
      escaped.push_back('%');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string EscapeQueryParamValue(base::StringPiece text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

}  // namespace

absl::optional<base::FilePath> GetBaseDataDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> app_data_dir;
#if BUILDFLAG(IS_WIN)
  base::FilePath path;
  if (!base::PathService::Get(scope == UpdaterScope::kSystem
                                  ? base::DIR_PROGRAM_FILES
                                  : base::DIR_LOCAL_APP_DATA,
                              &path)) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
  app_data_dir = path;
#elif BUILDFLAG(IS_MAC)
  app_data_dir = GetApplicationSupportDirectory(scope);
  if (!app_data_dir) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }
#endif
  const auto product_data_dir =
      app_data_dir->AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII(PRODUCT_FULLNAME_STRING);
  if (!base::CreateDirectory(product_data_dir)) {
    LOG(ERROR) << "Can't create base directory: " << product_data_dir;
    return absl::nullopt;
  }
  return product_data_dir;
}

absl::optional<base::FilePath> GetVersionedDataDirectory(UpdaterScope scope) {
  const absl::optional<base::FilePath> product_dir =
      GetBaseDataDirectory(scope);
  if (!product_dir) {
    LOG(ERROR) << "Failed to get the base directory.";
    return absl::nullopt;
  }

  const auto versioned_dir = product_dir->AppendASCII(kUpdaterVersion);
  if (!base::CreateDirectory(versioned_dir)) {
    LOG(ERROR) << "Can't create versioned directory.";
    return absl::nullopt;
  }

  return versioned_dir;
}

absl::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope,
    const base::Version& version) {
  const absl::optional<base::FilePath> path = GetBaseInstallDirectory(scope);
  if (!path)
    return absl::nullopt;
  return path->AppendASCII(version.GetString());
}

absl::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope) {
  return GetVersionedInstallDirectory(scope, base::Version(kUpdaterVersion));
}

TagParsingResult::TagParsingResult() = default;
TagParsingResult::TagParsingResult(absl::optional<tagging::TagArgs> tag_args,
                                   tagging::ErrorCode error)
    : tag_args(tag_args), error(error) {}
TagParsingResult::~TagParsingResult() = default;
TagParsingResult::TagParsingResult(const TagParsingResult&) = default;
TagParsingResult& TagParsingResult::operator=(const TagParsingResult&) =
    default;

TagParsingResult GetTagArgsForCommandLine(
    const base::CommandLine& command_line) {
  std::string tag = command_line.HasSwitch(kTagSwitch)
                        ? command_line.GetSwitchValueASCII(kTagSwitch)
                        : command_line.GetSwitchValueASCII(kHandoffSwitch);
#if BUILDFLAG(IS_WIN)
    if (tag.empty())
      tag = GetSwitchValueInLegacyFormat(command_line.GetCommandLineString(),
                                         base::ASCIIToWide(kHandoffSwitch));
#endif
    if (tag.empty())
      return {};
    tagging::TagArgs tag_args;
    const tagging::ErrorCode error =
        tagging::Parse(tag, absl::nullopt, &tag_args);
    VLOG_IF(1, error != tagging::ErrorCode::kSuccess)
        << "Tag parsing returned " << error << ".";
    return {tag_args, error};
}

TagParsingResult GetTagArgs() {
  return GetTagArgsForCommandLine(*base::CommandLine::ForCurrentProcess());
}

absl::optional<tagging::AppArgs> GetAppArgs(const std::string& app_id) {
  const absl::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
  if (!tag_args || tag_args->apps.empty())
    return absl::nullopt;

  const std::vector<tagging::AppArgs>& apps_args = tag_args->apps;
  std::vector<tagging::AppArgs>::const_iterator it = std::find_if(
      std::begin(apps_args), std::end(apps_args),
      [&app_id](const tagging::AppArgs& app_args) {
        return base::EqualsCaseInsensitiveASCII(app_args.app_id, app_id);
      });
  return it != std::end(apps_args) ? absl::optional<tagging::AppArgs>(*it)
                                   : absl::nullopt;
}

std::string GetInstallDataIndexFromAppArgs(const std::string& app_id) {
  const absl::optional<tagging::AppArgs> app_args = GetAppArgs(app_id);
  return app_args ? app_args->install_data_index : std::string();
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
#if BUILDFLAG(IS_MAC)
  command_line.PrependWrapper("/usr/bin/sudo");
#endif
  return command_line;
}

// The log file is created in DIR_LOCAL_APP_DATA or DIR_ROAMING_APP_DATA.
void InitLogging(UpdaterScope updater_scope) {
  logging::LoggingSettings settings;
  const absl::optional<base::FilePath> log_dir =
      GetBaseDataDirectory(updater_scope);
  if (!log_dir) {
    LOG(ERROR) << "Error getting base dir.";
    return;
  }
  const base::FilePath log_file =
      log_dir->Append(FILE_PATH_LITERAL("updater.log"));
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
  VLOG(1) << "Log file: " << settings.log_file_path;
  VLOG(1) << "Process command line: "
          << base::CommandLine::ForCurrentProcess()->GetCommandLineString();
}

// This function and the helper functions are copied from net/base/url_util.cc
// to avoid the dependency on //net.
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (EscapeQueryParamValue(name, true) + "=" +
            EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

#if BUILDFLAG(IS_LINUX)

// TODO(crbug.com/1276188) - implement the functions below.
absl::optional<base::FilePath> GetBaseInstallDirectory(UpdaterScope scope) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

base::FilePath GetExecutableRelativePath() {
  NOTIMPLEMENTED();
  return base::FilePath();
}

bool PathOwnedByUser(const base::FilePath& path) {
  NOTIMPLEMENTED();
  return false;
}

#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)

std::wstring GetTaskNamePrefix(UpdaterScope scope) {
  std::wstring task_name = GetTaskDisplayName(scope);
  task_name.erase(std::remove_if(task_name.begin(), task_name.end(), isspace),
                  task_name.end());
  return task_name;
}

std::wstring GetTaskDisplayName(UpdaterScope scope) {
  return base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" Task ",
                       scope == UpdaterScope::kSystem ? L"System " : L"User ",
                       kUpdaterVersionUtf16});
}

#endif  // BUILDFLAG(IS_WIN)

absl::optional<base::FilePath> WriteInstallerDataToTempFile(
    const base::FilePath& directory,
    const std::string& installer_data) {
  VLOG(2) << __func__ << ": " << directory << ": " << installer_data;

  if (!base::DirectoryExists(directory))
    return absl::nullopt;

  if (installer_data.empty())
    return absl::nullopt;

  base::FilePath path;
  base::File file = base::CreateAndOpenTemporaryFileInDir(directory, &path);
  if (!file.IsValid())
    return absl::nullopt;

  const std::string installer_data_utf8_bom =
      base::StrCat({kUTF8BOM, installer_data});
  if (file.Write(0, installer_data_utf8_bom.c_str(),
                 installer_data_utf8_bom.length()) == -1) {
    VLOG(2) << __func__ << " file.Write failed";
    return absl::nullopt;
  }

  return path;
}

void InitializeThreadPool(const char* name) {
  base::ThreadPoolInstance::Create(name);

  // Reuses the logic in base::ThreadPoolInstance::StartWithDefaultParams.
  const size_t max_num_foreground_threads =
      static_cast<size_t>(std::max(3, base::SysInfo::NumberOfProcessors() - 1));
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
#if BUILDFLAG(IS_WIN)
  init_params.common_thread_pool_environment = base::ThreadPoolInstance::
      InitParams::CommonThreadPoolEnvironment::COM_MTA;
#endif
  base::ThreadPoolInstance::Get()->Start(init_params);
}

}  // namespace updater