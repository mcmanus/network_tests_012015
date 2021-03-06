/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HELPFUNCTIONS_H__
#define HELPFUNCTIONS_H__

#include "prerror.h"

int LogErrorWithCode(PRErrorCode errCode, const char *aType);
int LogError(const char *aType);

#endif
