/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <mutex>

struct MHD_Daemon;

/*!
 * \brief Lightweight localhost-only HTTP server for serving Kodi VFS files
 * to external apps (e.g. VR video players) with Range/seek support.
 *
 * Binds to 127.0.0.1 on an OS-assigned port.  Any file that CFile can
 * open is served — no access-control checks, no authentication.
 *
 * Typical URL: http://127.0.0.1:<port>/<url-encoded-vfs-path>
 */
class CVfsProxyServer
{
public:
  static CVfsProxyServer& GetInstance();

  /*!
   * \brief Start the server if not already running.
   * \return The port number, or 0 on failure.
   */
  uint16_t Start();

  //! \brief Stop the server and close all connections.
  void Stop();

  bool IsRunning() const;
  uint16_t GetPort() const;

private:
  CVfsProxyServer() = default;
  ~CVfsProxyServer();
  CVfsProxyServer(const CVfsProxyServer&) = delete;
  CVfsProxyServer& operator=(const CVfsProxyServer&) = delete;

  MHD_Daemon* m_daemon = nullptr;
  uint16_t m_port = 0;
  mutable std::mutex m_mutex;
};
