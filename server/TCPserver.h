/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEST_TCP_SERVER_SIDE_H__
#define TEST_TCP_SERVER_SIDE_H__

#include "prio.h"
#include "prerror.h"

class TCPserver
{
public:
  TCPserver();
  ~TCPserver();
  int Start(uint16_t *aPort, int aNumberOfPorts);

private:
  int Init(uint16_t aPort, int aInx);
  int Run();
  int StartClientThread(PRFileDesc *fdClient);

  PRFileDesc **mFds;
  int mNumberOfPorts;
};

#endif
