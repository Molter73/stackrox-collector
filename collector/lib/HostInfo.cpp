/** collector

A full notice with attributions is provided along with this source code.

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

                                 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include "HostInfo.h"

#include <filesystem>
#include <fstream>

#include <bpf/libbpf.h>
#include <linux/bpf.h>

#include "FileSystem.h"
#include "Logging.h"

namespace collector {

// An offset for secure_boot option in boot_params.
// See https://www.kernel.org/doc/html/latest/x86/zero-page.html
const int SECURE_BOOT_OFFSET = 0x1EC;

namespace {

// Helper method which checks whether the given kernel & os
// are RHEL 7.6 (to inform later heuristics around eBPF support)
bool isRHEL76(const KernelVersion& kernel, const std::string& os_id) {
  if (os_id == "rhel" || os_id == "centos") {
    // example release version: 3.10.0-957.10.1.el7.x86_64
    // build_id = 957
    if (kernel.release.find(".el7.") != std::string::npos) {
      if (kernel.kernel == 3 && kernel.major == 10) {
        return kernel.build_id >= MIN_RHEL_BUILD_ID;
      }
    }
  }
  return false;
}

// Helper method which checks whether the given kernel & os
// support eBPF. In practice this is RHEL 7.6, and any kernel
// newer than 4.14
bool hasEBPFSupport(const KernelVersion& kernel, const std::string& os_id) {
  if (isRHEL76(kernel, os_id)) {
    return true;
  }
  return kernel.HasEBPFSupport();
}

// Given a stream, reads line by line, expecting '<key>=<value>' format
// returns the value matching the key called 'name'
std::string filterForKey(std::istream& stream, const char* name) {
  std::string line;
  while (std::getline(stream, line)) {
    auto idx = line.find('=');
    if (idx == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, idx);
    if (key == name) {
      std::string value = line.substr(idx + 1);
      // ensure we remove quotations from the start and end, if they exist.
      if (value[0] == '"') {
        value.erase(0, 1);
      }
      if (value[value.size() - 1] == '"') {
        value.erase(value.size() - 1);
      }
      return value;
    }
  }
  return "";
}

std::string GetHostnameFromFile(std::string_view hostnamePath) {
  auto hostnameFile = GetHostPath(hostnamePath);
  std::ifstream file(hostnameFile);
  std::string hostname = "";
  if (!file.is_open()) {
    CLOG(DEBUG) << hostnameFile << " file not found";
    CLOG(DEBUG) << "Failed to determine hostname from " << hostnameFile;
  } else if (!std::getline(file, hostname)) {
    CLOG(DEBUG) << hostnameFile << " is empty";
    CLOG(DEBUG) << "Failed to determine hostname from " << hostnameFile;
  }
  return hostname;
}

}  // namespace

KernelVersion HostInfo::GetKernelVersion() {
  if (kernel_version_.release.empty()) {
    kernel_version_ = KernelVersion::FromHost();
  }
  return kernel_version_;
}

const std::string& HostInfo::GetHostnameInner() {
  if (!hostname_.empty()) {
    return hostname_;
  }

  const char* hostname_env = std::getenv("NODE_HOSTNAME");
  if (hostname_env != nullptr && *hostname_env != '\0') {
    hostname_ = hostname_env;
    CLOG(DEBUG) << "Found hostname in NODE_HOSTNAME environment variable";
    return hostname_;
  }

  // if we can't get the hostname from the environment
  // we can look in /etc or /proc (mounted at /host/etc or /host/proc in the collector container)
  std::vector<std::string> hostnamePaths{
      "/etc/hostname",
      "/proc/sys/kernel/hostname",
  };

  for (auto hostnamePath : hostnamePaths) {
    hostname_ = GetHostnameFromFile(hostnamePath);
    if (!hostname_.empty()) {
      CLOG(DEBUG) << "Found hostname in " << hostnamePath;
      break;
    }
  }

  if (hostname_.empty()) {
    CLOG(FATAL) << "Unable to determine the hostname. Consider setting the environment variable NODE_HOSTNAME";
  }

  CLOG(INFO) << "Hostname: '" << hostname_ << "'";
  return hostname_;
}

std::string& HostInfo::GetDistro() {
  if (distro_.empty()) {
    distro_ = GetOSReleaseValue("PRETTY_NAME");
    if (distro_.empty()) {
      distro_ = "Linux";
    }
  }
  return distro_;
}

std::string& HostInfo::GetBuildID() {
  if (build_id_.empty()) {
    build_id_ = GetOSReleaseValue("BUILD_ID");
  }
  return build_id_;
}

std::string& HostInfo::GetOSID() {
  if (os_id_.empty()) {
    os_id_ = GetOSReleaseValue("ID");
  }
  return os_id_;
}

std::string HostInfo::GetOSReleaseValue(const char* name) {
  std::ifstream release_file(GetHostPath("/etc/os-release"));
  if (!release_file.is_open()) {
    release_file.open(GetHostPath("/usr/lib/os-release"));
    if (!release_file.is_open()) {
      CLOG(ERROR) << "Failed to open os-release file, unable to resolve OS information.";
      return "";
    }
  }

  return filterForKey(release_file, name);
}

bool HostInfo::IsRHEL76() {
  auto kernel = GetKernelVersion();
  return collector::isRHEL76(kernel, GetOSID());
}

bool HostInfo::IsRHEL86() {
  auto kernel = GetKernelVersion();
  if (GetOSID() == "rhel" || GetOSID() == "rhcos") {
    return kernel.release.find(".el8_6.") != std::string::npos;
  }
  return false;
}

bool HostInfo::HasEBPFSupport() {
  auto kernel = GetKernelVersion();
  return collector::hasEBPFSupport(kernel, GetOSID());
}

bool HostInfo::HasBTFSymbols() {
  struct location_s {
    std::string pattern;
    bool mounted;
  };
  // This list is taken from libbpf
  const std::vector<location_s> locations = {
      /* try canonical vmlinux BTF through sysfs first */
      {"/sys/kernel/btf/vmlinux", false},
      /* fall back to trying to find vmlinux on disk otherwise */
      {"/boot/vmlinux-%1$s", false},
      {"/lib/modules/%1$s/vmlinux-%1$s", false},
      {"/lib/modules/%1$s/build/vmlinux", false},
      {"/usr/lib/modules/%1$s/kernel/vmlinux", true},
      {"/usr/lib/debug/boot/vmlinux-%1$s", true},
      {"/usr/lib/debug/boot/vmlinux-%1$s.debug", true},
      {"/usr/lib/debug/lib/modules/%1$s/vmlinux", true},
  };

  char path[PATH_MAX + 1];

  for (const auto& location : locations) {
    snprintf(path, PATH_MAX, location.pattern.c_str(), kernel_version_.release.c_str());
    std::string host_path = location.mounted ? GetHostPath(path) : path;

    if (faccessat(AT_FDCWD, host_path.c_str(), R_OK, AT_EACCESS) == 0) {
      CLOG(DEBUG) << "BTF symbols found in " << host_path;
      return true;

    } else {
      if (errno == ENOTDIR || errno == ENOENT) {
        CLOG(DEBUG) << host_path << " does not exist";
      } else {
        CLOG(WARNING) << "Unable to access " << host_path << ": " << StrError();
      }
    }
  }

  CLOG(DEBUG) << "Unable to find BTF symbols in any of the usual locations.";

  return false;
}

bool HostInfo::HasBPFRingBufferSupport() {
  int res;

  res = libbpf_probe_bpf_map_type(BPF_MAP_TYPE_RINGBUF, NULL);

  if (res == 0) {
    CLOG(INFO) << "BPF RingBuffer map type is not available (errno="
               << StrError() << ")";
  }

  if (res < 0) {
    CLOG(WARNING) << "Unable to check for the BPF RingBuffer availability. "
                  << "Assuming it is available.";
  }

  return res != 0;
}

bool HostInfo::HasBPFTracingSupport() {
  int res;

  res = libbpf_probe_bpf_prog_type(BPF_PROG_TYPE_TRACING, NULL);

  if (res == 0) {
    CLOG(INFO) << "BPF tracepoint program type is not supported (errno="
               << StrError() << ")";
  }

  if (res < 0) {
    CLOG(WARNING) << "Unable to check for the BPF tracepoint program type support. "
                  << "Assuming it is available.";
  }

  return res != 0;
}

bool HostInfo::IsUEFI() {
  auto efi_path = GetHostPath("/sys/firmware/efi");
  std::error_code ec;
  auto status = std::filesystem::status(efi_path, ec);

  if (ec) {
    CLOG(WARNING) << "Could not stat " << efi_path << ": " << ec.message()
                  << ". No UEFI heuristic is performed.";
    return false;
  }

  if (status.type() != std::filesystem::file_type::directory) {
    CLOG(WARNING) << "EFI path is not a directory or doesn't exist, legacy boot mode";
    return false;
  }

  CLOG(DEBUG) << "EFI directory exists, UEFI boot mode";
  return true;
}

// Get SecureBoot status from reading a corresponding EFI variable. Every such
// variable is a small file <key name>-<vendor-guid> in efivarfs directory, and
// its format is described in UEFI specification.
SecureBootStatus HostInfo::GetSecureBootFromVars() {
  auto efi_path = GetHostPath("/sys/firmware/efi/efivars");
  DirHandle efivars = opendir(efi_path.c_str());

  if (!efivars.valid()) {
    CLOG(WARNING) << "Could not open " << efi_path << ": " << StrError();
    return SecureBootStatus::NOT_DETERMINED;
  }

  while (auto dp = efivars.read()) {
    std::string name(dp->d_name);

    if (name.rfind("SecureBoot-", 0) == 0) {
      std::uint8_t efi_key[5];
      auto path = efi_path / name;

      // There should be only one SecureBoot key, so it doesn't make sense to
      // search further in case if e.g. it couldn't be read.
      std::ifstream secure_boot(path, std::ios::binary | std::ios::in);
      if (!secure_boot.is_open()) {
        CLOG(WARNING) << "Failed to open SecureBoot key " << path;
        return SecureBootStatus::NOT_DETERMINED;
      }

      // An EFI variable contains 4 bytes with attributes, and 5th with the
      // actual value. The efivarfs doesn't support lseek, returning ESPIPE on
      // it, so read the header first, then the actual value.
      // See https://www.kernel.org/doc/html/latest/filesystems/efivarfs.html
      secure_boot.read(reinterpret_cast<char*>(&efi_key), 5);
      std::uint8_t status = efi_key[4];

      // Pretty intuitively 0 means the feature is disabled, 1 enabled.
      // SecureBoot efi variable doesn't have NOT_DETERMINED value.
      // See https://uefi.org/sites/default/files/resources/UEFI_Spec_2_9_2021_03_18.pdf#page=86
      if (status != 0 && status != 1) {
        CLOG(WARNING) << "Incorrect secure_boot param: " << (unsigned int)status;
        return SecureBootStatus::NOT_DETERMINED;
      }

      return static_cast<SecureBootStatus>(status);
    }
  }

  // No SecureBoot key found
  return SecureBootStatus::NOT_DETERMINED;
}

// Get SecureBoot status from reading boot_params structure. Not only it will
// tell whether the SecureBoot is enabled or disabled, but also if could not be
// determined.
SecureBootStatus HostInfo::GetSecureBootFromParams() {
  std::uint8_t status;
  auto boot_params_path = GetHostPath("/sys/kernel/boot_params/data");

  std::ifstream boot_params(boot_params_path, std::ios::binary | std::ios::in);

  if (!boot_params.is_open()) {
    CLOG(WARNING) << "Failed to open " << boot_params_path;
    return SecureBootStatus::NOT_DETERMINED;
  }

  boot_params.seekg(SECURE_BOOT_OFFSET);
  boot_params.read(reinterpret_cast<char*>(&status), 1);

  if (status < SecureBootStatus::NOT_DETERMINED ||
      status > SecureBootStatus::ENABLED) {
    CLOG(WARNING) << "Incorrect secure_boot param: " << (unsigned int)status;
    return SecureBootStatus::NOT_DETERMINED;
  }

  return static_cast<SecureBootStatus>(status);
}

SecureBootStatus HostInfo::GetSecureBootStatus() {
  std::uint8_t status;
  auto kernel = GetKernelVersion();

  if (secure_boot_status_ != SecureBootStatus::UNSET) {
    return secure_boot_status_;
  }

  if (kernel.HasSecureBootParam()) {
    status = GetSecureBootFromParams();
  } else {
    status = GetSecureBootFromVars();
  }

  secure_boot_status_ = static_cast<SecureBootStatus>(status);

  CLOG(DEBUG) << "SecureBoot status is " << secure_boot_status_;
  return secure_boot_status_;
}

// Minikube keeps its version under /etc/VERSION
std::string HostInfo::GetMinikubeVersion() {
  std::ifstream version_file(GetHostPath("/etc/VERSION"));

  if (!version_file.is_open()) {
    CLOG(WARNING) << "Failed to acquire minikube version";
    return "";
  }

  std::string version;
  std::regex version_re(R"(v\d+\.\d+\.\d+)");
  std::smatch match;

  std::getline(version_file, version);

  if (!std::regex_search(version, match, version_re)) {
    CLOG(WARNING) << "Failed to match minikube version: " << version;
    return "";
  }

  return match.str();
}

int HostInfo::NumPossibleCPU() {
  int n_possible_cpus = libbpf_num_possible_cpus();
  if (n_possible_cpus < 0) {
    CLOG(WARNING) << "Cannot get number of possible CPUs: "
                  << StrError(n_possible_cpus);
    return 0;
  }

  return n_possible_cpus;
}

}  // namespace collector
