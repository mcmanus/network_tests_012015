/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TCP.h"
#include "prerror.h"

namespace NetworkPath {

extern PRLogModuleInfo* gClientTestLog;
#define LOG(args) PR_LOG(gClientTestLog, PR_LOG_DEBUG, args)

// I will give test a code:
#define TCP_reachability "Test_2"
#define TCP_performanceFromServerToClient "Test_3"
#define TCP_performanceFromClientToServer "Test_4"
#define FINISH "Finish"

// after this short interval, we will return to PR_Poll
#define NS_SOCKET_CONNECT_TIMEOUT PR_MillisecondsToInterval(400)
#define SNDBUFFERSIZE 12582912

extern uint64_t maxBytes;
extern uint32_t maxTime;

TCP::TCP(PRNetAddr *aNetAddr)
  : mFd(nullptr)
{
  memcpy(&mNetAddr, aNetAddr, sizeof(PRNetAddr));
}

TCP::~TCP()
{
  if (mFd) {
    PR_Close(mFd);
  }
}

nsresult
TCP::Start(int aTestType)
{
  mTestType = aTestType;
  nsresult rv = Init();
  if (NS_FAILED(rv)) {
    if(mFd) {
      PR_Close(mFd);
      mFd = nullptr;
    }
    return rv;
  }

  rv = Run();
  if (NS_FAILED(rv)) {
    if(mFd) {
      PR_Close(mFd);
      mFd = nullptr;
    }
  }
  return rv;
}

nsresult
TCP::Init()
{

  LOG(("NetworkTest TCP client: Open socket"));
  char host[164] = {0};
  PR_NetAddrToString(&mNetAddr, host, sizeof(host));
  LOG(("NetworkTest TCP client: Host: %s", host));
  LOG(("NetworkTest TCP client: AF: %d", mNetAddr.raw.family));
  int port = 0;
  if (mNetAddr.raw.family == AF_INET) {
    port = mNetAddr.inet.port;
  } else if (mNetAddr.raw.family == AF_INET6) {
    port = mNetAddr.ipv6.port;
  }
  LOG(("NetworkTest TCP client: port: %d", port));

  mFd = PR_OpenTCPSocket(mNetAddr.raw.family);
  if (!mFd) {
    return ErrorAccordingToNSPR("TCP");
  }

  LOG(("NetworkTest TCP client: Set Options"));
  PRStatus status;
  PRSocketOptionData opt;
  opt.option = PR_SockOpt_Nonblocking;
  opt.value.non_blocking = true;
  status = PR_SetSocketOption(mFd, &opt);
  if (status != PR_SUCCESS) {
    PR_Close(mFd);
    mFd = nullptr;
    return ErrorAccordingToNSPR("TCP");
  }

  opt.option = PR_SockOpt_NoDelay;
  opt.value.no_delay = true;
  status = PR_SetSocketOption(mFd, &opt);
  if (status != PR_SUCCESS) {
    PR_Close(mFd);
    mFd = nullptr;
    return ErrorAccordingToNSPR("TCP");
  }

  opt.option = PR_SockOpt_SendBufferSize;
  opt.value.send_buffer_size = SNDBUFFERSIZE;
  PR_SetSocketOption(mFd, &opt);
  if (status != PR_SUCCESS) {
    PR_Close(mFd);
    mFd = nullptr;
    return ErrorAccordingToNSPR("TCP");
  }

  LOG(("NetworkTest TCP client: Connect..."));
  status = PR_Connect(mFd, &mNetAddr, NS_SOCKET_CONNECT_TIMEOUT);
  if (status != PR_SUCCESS) {
    PRErrorCode errCode = PR_GetError();
    if (PR_IS_CONNECTED_ERROR == errCode) {
      LOG(("NetworkTest TCP client: It is connected"));
      return NS_OK;
    } else if ((PR_WOULD_BLOCK_ERROR == errCode) ||
               (PR_IN_PROGRESS_ERROR == errCode)) {
      PRPollDesc pollElem;
      pollElem.fd = mFd;
      pollElem.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
      LOG(("NetworkTest TCP client: Poll for a connection."));
      while (1) {
        pollElem.out_flags = 0;
        PR_Poll(&pollElem, 1, PR_INTERVAL_NO_WAIT);
        if ( pollElem.out_flags & PR_POLL_WRITE ) {
          LOG(("NetworkTest TCP client: Connected."));
          return NS_OK;
        } else if (pollElem.out_flags &
                   (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL)) {
          errCode = PR_GetError();
          if ((PR_WOULD_BLOCK_ERROR == errCode) ||
               (PR_IN_PROGRESS_ERROR == errCode)) {
            continue;
          }
          LOG(("NetworkTest TCP client: Could not connect."));
          PR_Close(mFd);
          mFd = nullptr;
          return ErrorAccordingToNSPR("TCP");
        }
      }
    }
    PR_Close(mFd);
    mFd = nullptr;
    return ErrorAccordingToNSPRWithCode(errCode, "TCP");
  }
  return NS_OK;
}

nsresult
TCP::Run()
{
  PRPollDesc pollElem;
  pollElem.fd = mFd;
  pollElem.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
  uint64_t readBytes = 0;
  uint64_t writtenBytes = 0;
  uint64_t recvBytesForRate = 0;
  PRIntervalTime timeFirstPktReceived = 0;
  PRIntervalTime startRateCalc = 0;
  uint16_t bufLen = 1500;
  char buf[bufLen];
  switch (mTestType) {
    case 2:
      memcpy(buf, TCP_reachability, 6);
      break;
    case 3:
      memcpy(buf, TCP_performanceFromServerToClient, 6);
      break;
    case 4:
      memcpy(buf, TCP_performanceFromClientToServer, 6);
      break;
    default:
      PR_Close(mFd);
      mFd = nullptr;
      return NS_ERROR_FAILURE;
  }

  LOG(("NetworkTest TCP client: Poll"));
  while (1) {
    pollElem.out_flags = 0;
    int rv = PR_Poll(&pollElem, 1, 1000);
    if (rv < 0) {
      LOG(("NetworkTest TCP client: Poll error: read bytes %lu", readBytes));
      PR_Close(mFd);
      mFd = nullptr;
      return ErrorAccordingToNSPR("TCP");
    } else if (rv == 0) {
      LOG(("NetworkTest TCP client: Closing, timeout: read bytes %lu",
           readBytes));
      PR_Close(mFd);
      mFd = nullptr;
      return ErrorAccordingToNSPR("TCP");
    }
    if (pollElem.out_flags & (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL)) {
      PRErrorCode errCode = PR_GetError();
      LOG(("NetworkTest TCP client: Connection error. read bytes %lu",
           readBytes));
      if (!(pollElem.out_flags & PR_POLL_NVAL)) {
        PR_Close(mFd);
        mFd = nullptr;
      }
      return ErrorAccordingToNSPRWithCode(errCode, "TCP");
    }

    if (pollElem.out_flags & PR_POLL_WRITE) {
      //LOG(("NetworkTest TCP client: Sending data for test %d.", mTestType));

      int written = 0;
      if (writtenBytes < bufLen) {
        // The first packet must be sent as it is so that server can read
        // exact positions in buffer. Actually only first bytes are important.
        written = PR_Write(mFd, buf + writtenBytes,
                           bufLen - writtenBytes);
      } else {
        written = PR_Write(mFd, buf, bufLen);
      }

      if (written < 0) {
        PRErrorCode code = PR_GetError();
        if (code == PR_WOULD_BLOCK_ERROR) {
          continue;
        }
        PR_Close(mFd);
        mFd = nullptr;
        return ErrorAccordingToNSPRWithCode(code, "TCP");
      }

      writtenBytes += written;

      if (writtenBytes >= bufLen) {
        if (mTestType == 2 || mTestType == 3) {
          // Test 2 and 3 are waiting for data from the server.
          pollElem.in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
        } else if (mTestType == 4) {
          // Test 4 is sending data, until it receives FINISH packet.
          pollElem.in_flags = PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT;
        }
      }
    }
    if (pollElem.out_flags & PR_POLL_READ) {
      int read = 0;
      if (mTestType == 3) {
        read = PR_Read(mFd, buf, bufLen);
      } else {
        read = PR_Read(mFd, buf + readBytes, bufLen - readBytes);
      }
      if (read < 1) {
        PRErrorCode code = PR_GetError();
        if (code == PR_WOULD_BLOCK_ERROR) {
          continue;
        }
        PR_Close(mFd);
        mFd = nullptr;
        return ErrorAccordingToNSPR("TCP");
      }
      readBytes += read;
//      LOG(("NetworkTest TCP client: time %lu Test %d - received %lu bytes.",
//           PR_IntervalNow() - timeFirstPktReceived, mTestType, readBytes));

      if (!timeFirstPktReceived) {
        timeFirstPktReceived = PR_IntervalNow();
      }
      switch (mTestType) {
        case 2:
          if (readBytes >= bufLen) {
            LOG(("NetworkTest TCP client: Closing: read enough bytes - %lu",
                 readBytes));
            PR_Close(mFd);
            mFd = nullptr;
            return NS_OK;
          }
        case 3:
          if (PR_IntervalToSeconds(PR_IntervalNow() - timeFirstPktReceived) >=
              2) {
            recvBytesForRate += read;
            if (!startRateCalc) {
              startRateCalc = PR_IntervalNow();
            }
          }

          // Check whether we have received enough data.
          if ((readBytes >= maxBytes) &&
              (PR_IntervalToSeconds(PR_IntervalNow() - timeFirstPktReceived)
               >= maxTime)) {

            if (PR_IntervalToSeconds(PR_IntervalNow() - startRateCalc)) {
              mPktPerSec = (double)recvBytesForRate / 1500.0 /
                (double)PR_IntervalToMilliseconds(PR_IntervalNow() - startRateCalc) *
                1000.0;
            }
            LOG(("NetworkTest TCP client: Closing, observed rate: %lu",
                 mPktPerSec));
            LOG(("Test 3 finished: time %lu, first packet sent %lu, "
                 "duration %lu, received %llu max to received %llu, received "
                 "bytes for rate calc %llu, duration for calc %lu",
                 PR_IntervalNow(),
                 timeFirstPktReceived,
                 PR_IntervalToMilliseconds(PR_IntervalNow() - timeFirstPktReceived),
                 readBytes, maxBytes, recvBytesForRate,
                 PR_IntervalNow() - startRateCalc));
            PR_Close(mFd);
            mFd = nullptr;
            return NS_OK;
          }
          break;
        case 4:
          if (readBytes >= bufLen) {
            LOG(("NetworkTest TCP client: Closing: read enough bytes - %lu",
                 readBytes));
            uint64_t rate;
            PR_STATIC_ASSERT(sizeof(rate) == 8);
            memcpy(&rate, buf, sizeof (rate));
            mPktPerSec = ntohl(rate);
            PR_Close(mFd);
            mFd = nullptr;
            return NS_OK;
          }
      }
    }
  }
  PR_Close(mFd);
  mFd = nullptr;
  return NS_OK;
}

} // namespace NetworkPath
