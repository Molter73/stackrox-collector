#pragma once

#include <memory>
#include <mutex>

#include "CivetWrapper.h"
#include "Profiler.h"

namespace collector {

// Profiling Endpoints:
// POST /profile/cpu
//   - accepts post data of on|off|empty to enable, disable or clear/delete the latest cpu profile
// POST /profile/heap
//   - accepts post data of on|off|empty to enable, disable or clear/delete the latest heap profile
// GET /profile
//   - get profiling support and status
// GET /profile/cpu
//   - get latest cpu profile
// GET /profile/heap
//   - get latest heap profile
class ProfilerHandler : public CivetWrapper {
 public:
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;
  bool handlePost(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kCPUProfileFilename;
  static const std::string kBaseRoute;
  static const std::string kCPURoute;
  static const std::string kHeapRoute;

  bool ServerError(struct mg_connection* conn, const char* err);
  bool ClientError(struct mg_connection* conn, const char* err);
  bool SendStatus(struct mg_connection* conn);
  bool SendHeapProfile(struct mg_connection* conn);
  bool SendCPUProfile(struct mg_connection* conn);
  bool HandleCPURoute(struct mg_connection* conn, const std::string& post_data);
  bool HandleHeapRoute(struct mg_connection* conn, const std::string& post_data);
  MallocUniquePtr heap_profile_;
  size_t heap_profile_length_;
  size_t cpu_profile_length_;
  std::mutex mutex_;
};
}  // namespace collector
