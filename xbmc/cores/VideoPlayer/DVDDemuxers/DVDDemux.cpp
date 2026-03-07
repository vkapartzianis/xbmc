/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DVDDemux.h"

#include "utils/StringUtils.h"

std::string CDemuxStreamAudio::GetStreamType()
{
  std::string strInfo;
  switch (codec)
  {
    case AV_CODEC_ID_AC3:
      strInfo = "AC3 ";
      break;
    case AV_CODEC_ID_EAC3:
    {
      if (profile == FF_PROFILE_EAC3_DDP_ATMOS ||
          StringUtils::Contains(codecName, "JOC"))
        strInfo = "DD+ ATMOS ";
      else
        strInfo = "DD+ ";
      break;
    }
    case AV_CODEC_ID_DTS:
    {
      switch (profile)
      {
        case FF_PROFILE_DTS_96_24:
          strInfo = "DTS 96/24 ";
          break;
        case FF_PROFILE_DTS_ES:
          strInfo = "DTS ES ";
          break;
        case FF_PROFILE_DTS_EXPRESS:
          strInfo = "DTS EXPRESS ";
          break;
        case FF_PROFILE_DTS_HD_MA:
          strInfo = "DTS-HD MA ";
          break;
        case FF_PROFILE_DTS_HD_HRA:
          strInfo = "DTS-HD HRA ";
          break;
        case FF_PROFILE_DTS_HD_MA_X:
          strInfo = "DTS:X ";
          break;
        case FF_PROFILE_DTS_HD_MA_X_IMAX:
          strInfo = "DTS:X IMAX ";
          break;
        default:
          strInfo = "DTS ";
          break;
      }
      break;
    }
    case AV_CODEC_ID_MP2:
      strInfo = "MP2 ";
      break;
    case AV_CODEC_ID_MP3:
      strInfo = "MP3 ";
      break;
    case AV_CODEC_ID_TRUEHD:
      strInfo = "TrueHD ";
      break;
    case AV_CODEC_ID_AAC:
    {
      switch (profile)
      {
        case FF_PROFILE_AAC_LOW:
        case FF_PROFILE_MPEG2_AAC_LOW:
          strInfo = "AAC-LC ";
          break;
        case FF_PROFILE_AAC_HE:
        case FF_PROFILE_MPEG2_AAC_HE:
          strInfo = "HE-AAC ";
          break;
        case FF_PROFILE_AAC_HE_V2:
          strInfo = "HE-AACv2 ";
          break;
        case FF_PROFILE_AAC_SSR:
          strInfo = "AAC-SSR ";
          break;
        case FF_PROFILE_AAC_LTP:
          strInfo = "AAC-LTP ";
          break;
        default:
        {
          // Try check by codec full string according to RFC 6381
          if (codecName == "mp4a.40.2" || codecName == "mp4a.40.17")
            strInfo = "AAC-LC ";
          else if (codecName == "mp4a.40.3")
            strInfo = "AAC-SSR ";
          else if (codecName == "mp4a.40.4" || codecName == "mp4a.40.19")
            strInfo = "AAC-LTP ";
          else if (codecName == "mp4a.40.5")
            strInfo = "HE-AAC ";
          else if (codecName == "mp4a.40.29")
            strInfo = "HE-AACv2 ";
          else
            strInfo = "AAC ";
          break;
        }
      }
      break;
    }
    case AV_CODEC_ID_ALAC:
      strInfo = "ALAC ";
      break;
    case AV_CODEC_ID_FLAC:
      strInfo = "FLAC ";
      break;
    case AV_CODEC_ID_OPUS:
      strInfo = "Opus ";
      break;
    case AV_CODEC_ID_VORBIS:
      strInfo = "Vorbis ";
      break;
    case AV_CODEC_ID_PCM_BLURAY:
    case AV_CODEC_ID_PCM_DVD:
      strInfo = "PCM ";
      break;
    default:
      strInfo = "";
      break;
  }

  // Use a clean channel layout name based on channel count (e.g. "5.1", "7.1.4")
  // instead of FFmpeg's av_channel_layout_describe which includes speaker placement
  // suffixes like "(side)"
  static constexpr const char* defaultLayouts[] = {
      "0.0", "1.0", "2.0", "2.1", "4.0", "5.0", "5.1", "6.1",
      "7.1", "",    "5.1.4", "",   "7.1.4", "",   "9.1.4", "",  "9.1.6"};

  if (iChannels > 0 &&
      iChannels < static_cast<int>(sizeof(defaultLayouts) / sizeof(defaultLayouts[0])) &&
      defaultLayouts[iChannels][0] != '\0')
  {
    strInfo += defaultLayouts[iChannels];
  }
  else if (iChannels > 0)
  {
    strInfo += std::to_string(iChannels) + "ch";
  }

  return strInfo;
}

int CDVDDemux::GetNrOfStreams(StreamType streamType)
{
  int iCounter = 0;

  for (auto pStream : GetStreams())
  {
    if (pStream && pStream->type == streamType)
      iCounter++;
  }

  return iCounter;
}

int CDVDDemux::GetNrOfSubtitleStreams()
{
  return GetNrOfStreams(STREAM_SUBTITLE);
}

std::string CDemuxStream::GetStreamName()
{
  return name;
}
