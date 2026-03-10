/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VfsProxyServer.h"

#include "URL.h"
#include "filesystem/File.h"
#include "utils/Mime.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <cinttypes>
#include <cstring>
#include <memory>

#include <microhttpd.h>
#include <netinet/in.h>

// MHD callback return type changed from int to enum in 0.9.70
#if MHD_VERSION >= 0x00097002
using MHD_RESULT = MHD_Result;
#else
using MHD_RESULT = int;
#endif

namespace
{

struct FileContext
{
  std::shared_ptr<XFILE::CFile> file;
  int64_t rangeStart;
  int64_t rangeEnd;
};

ssize_t ContentReaderCallback(void* cls, uint64_t pos, char* buf, size_t max)
{
  auto* ctx = static_cast<FileContext*>(cls);
  if (!ctx || !ctx->file)
    return -1;

  const int64_t actualPos = ctx->rangeStart + static_cast<int64_t>(pos);
  if (ctx->rangeEnd >= 0 && actualPos > ctx->rangeEnd)
    return 0; // end of stream

  const int64_t remaining = ctx->rangeEnd - actualPos + 1;
  const size_t toRead =
      static_cast<size_t>(std::min(static_cast<int64_t>(max), remaining));

  if (ctx->file->GetPosition() != actualPos)
    ctx->file->Seek(actualPos);

  const ssize_t bytesRead = ctx->file->Read(buf, toRead);
  if (bytesRead <= 0)
    return -1;

  return bytesRead;
}

void ContentReaderFreeCallback(void* cls)
{
  auto* ctx = static_cast<FileContext*>(cls);
  if (ctx)
  {
    if (ctx->file)
      ctx->file->Close();
    delete ctx;
  }
}

MHD_RESULT AnswerCallback(void* /*cls*/,
                           MHD_Connection* connection,
                           const char* url,
                           const char* method,
                           const char* /*version*/,
                           const char* /*upload_data*/,
                           size_t* /*upload_data_size*/,
                           void** con_cls)
{
  // First call for this connection is headers-only; mark and return.
  if (*con_cls == nullptr)
  {
    *con_cls = reinterpret_cast<void*>(1);
    return MHD_YES;
  }

  const bool isGet = (std::strcmp(method, "GET") == 0);
  const bool isHead = (std::strcmp(method, "HEAD") == 0);
  if (!isGet && !isHead)
  {
    auto* resp = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
    auto ret = MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, resp);
    MHD_destroy_response(resp);
    return ret;
  }

  // The URL path (decoded by MHD) minus the leading '/'
  // gives us the original VFS path.
  std::string vfsPath(url + 1);

  CLog::Log(LOGDEBUG, "CVfsProxyServer: {} {}", method,
            CURL::GetRedacted(vfsPath));

  // Open the file through Kodi's VFS layer
  auto file = std::make_shared<XFILE::CFile>();
  if (!file->Open(vfsPath, XFILE::READ_NO_CACHE))
  {
    CLog::Log(LOGERROR, "CVfsProxyServer: CFile::Open failed for {}",
              CURL::GetRedacted(vfsPath));
    auto* resp = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
    auto ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
    MHD_destroy_response(resp);
    return ret;
  }

  const int64_t fileLength = file->GetLength();
  int64_t rangeStart = 0;
  int64_t rangeEnd = (fileLength > 0) ? fileLength - 1 : 0;
  int status = MHD_HTTP_OK;

  // Parse Range header (single range — sufficient for video players)
  const char* rangeHeader =
      MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Range");
  if (rangeHeader && fileLength > 0)
  {
    int64_t rs = -1, re = -1;
    if (sscanf(rangeHeader, "bytes=%" PRId64 "-%" PRId64, &rs, &re) >= 1 &&
        rs >= 0)
    {
      rangeStart = rs;
      rangeEnd = (re >= rs) ? re : fileLength - 1;
      if (rangeEnd >= fileLength)
        rangeEnd = fileLength - 1;
      status = MHD_HTTP_PARTIAL_CONTENT;
    }
  }

  // Content type from extension
  std::string ext = URIUtils::GetExtension(vfsPath);
  StringUtils::ToLower(ext);
  std::string mime = CMime::GetMimeType(ext);

  // HEAD — return headers only
  if (isHead)
  {
    file->Close();
    auto* resp =
        MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(resp, "Accept-Ranges", "bytes");
    MHD_add_response_header(resp, "Content-Type", mime.c_str());
    if (fileLength > 0)
      MHD_add_response_header(resp, "Content-Length",
                              std::to_string(fileLength).c_str());
    auto ret = MHD_queue_response(connection, MHD_HTTP_OK, resp);
    MHD_destroy_response(resp);
    return ret;
  }

  // GET — stream via callback
  const uint64_t responseLength =
      (fileLength > 0) ? static_cast<uint64_t>(rangeEnd - rangeStart + 1)
                       : MHD_SIZE_UNKNOWN;

  auto* ctx = new FileContext{file, rangeStart, rangeEnd};

  auto* resp = MHD_create_response_from_callback(
      responseLength, 65536, // 64 KB chunks
      &ContentReaderCallback, ctx, &ContentReaderFreeCallback);
  if (!resp)
  {
    delete ctx;
    return MHD_NO;
  }

  MHD_add_response_header(resp, "Accept-Ranges", "bytes");
  MHD_add_response_header(resp, "Content-Type", mime.c_str());

  if (status == MHD_HTTP_PARTIAL_CONTENT && fileLength > 0)
  {
    std::string cr = "bytes " + std::to_string(rangeStart) + "-" +
                     std::to_string(rangeEnd) + "/" +
                     std::to_string(fileLength);
    MHD_add_response_header(resp, "Content-Range", cr.c_str());
  }

  auto ret = MHD_queue_response(connection, status, resp);
  MHD_destroy_response(resp);
  return ret;
}

} // anonymous namespace

CVfsProxyServer& CVfsProxyServer::GetInstance()
{
  static CVfsProxyServer instance;
  return instance;
}

CVfsProxyServer::~CVfsProxyServer()
{
  Stop();
}

uint16_t CVfsProxyServer::Start()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_daemon)
    return m_port;

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = 0; // OS assigns a free port
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  m_daemon = MHD_start_daemon(
      MHD_USE_THREAD_PER_CONNECTION
#if (MHD_VERSION >= 0x00095207)
          | MHD_USE_INTERNAL_POLLING_THREAD
#endif
      ,
      0, nullptr, nullptr, &AnswerCallback, nullptr,
      MHD_OPTION_SOCK_ADDR, &addr,
      MHD_OPTION_CONNECTION_TIMEOUT, static_cast<unsigned int>(300),
      MHD_OPTION_END);

  if (!m_daemon)
  {
    CLog::Log(LOGERROR, "CVfsProxyServer: Failed to start daemon");
    return 0;
  }

  // Retrieve the OS-assigned port via the listen socket
  const union MHD_DaemonInfo* info =
      MHD_get_daemon_info(m_daemon, MHD_DAEMON_INFO_LISTEN_FD);
  if (info)
  {
    struct sockaddr_in bound = {};
    socklen_t len = sizeof(bound);
    if (getsockname(info->listen_fd,
                    reinterpret_cast<struct sockaddr*>(&bound), &len) == 0)
      m_port = ntohs(bound.sin_port);
  }

  if (m_port == 0)
  {
    CLog::Log(LOGERROR, "CVfsProxyServer: Failed to determine assigned port");
    MHD_stop_daemon(m_daemon);
    m_daemon = nullptr;
    return 0;
  }

  CLog::Log(LOGINFO, "CVfsProxyServer: Listening on 127.0.0.1:{}", m_port);
  return m_port;
}

void CVfsProxyServer::Stop()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_daemon)
    return;

  MHD_stop_daemon(m_daemon);
  m_daemon = nullptr;
  CLog::Log(LOGINFO, "CVfsProxyServer: Stopped (was on port {})", m_port);
  m_port = 0;
}

bool CVfsProxyServer::IsRunning() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_daemon != nullptr;
}

uint16_t CVfsProxyServer::GetPort() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_port;
}
