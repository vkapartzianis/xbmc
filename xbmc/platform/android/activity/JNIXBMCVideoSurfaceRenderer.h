/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <androidjni/JNIBase.h>
#include <androidjni/Surface.h>

namespace jni
{

class CJNIXBMCVideoSurfaceRenderer : virtual public CJNIBase
{
public:
  CJNIXBMCVideoSurfaceRenderer(const CJNISurface& outputSurface);
  ~CJNIXBMCVideoSurfaceRenderer();

  CJNISurface getInputSurface();
  void setRangeCorrection(bool enabled);
  void release();

private:
  CJNIXBMCVideoSurfaceRenderer() = default;
};

} // namespace jni
