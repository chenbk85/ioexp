// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#include "include/amio.h"
#include "posix/posix-transport.h"
#include "posix/posix-errors.h"
#include "linux/linux-utils.h"
#include "linux/linux-epoll.h"
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>

using namespace ke;
using namespace amio;

bool
amio::GetLinuxVersion(int *major, int *minor, int *release)
{
  struct utsname u;
  if (uname(&u) == -1)
    return false;

  char *majsplit = strchr(u.release, '.');
  if (!majsplit)
    return false;
  *majsplit = '\0';
  *major = atoi(u.release);
  char *minsplit = strchr(majsplit + 1, '.');
  if (!minsplit)
    return false;
  *minsplit = '\0';
  *minor = atoi(majsplit + 1);
  if (*(minsplit + 1) == '\0')
    *release = 0;
  else
    *release = atoi(minsplit + 1);
  return true;
}

PassRef<IOError>
PollerFactory::CreateEpollImpl(Ref<Poller> *outp, size_t maxEventsPerPoll)
{
  Ref<EpollImpl> poller(new EpollImpl(maxEventsPerPoll));
  Ref<IOError> error = poller->Initialize();
  if (error)
    return error;
  *outp = poller;
  return nullptr;
}

PassRef<IOError>
PollerFactory::Create(Ref<Poller> *outp)
{
  if (IsAtLeastLinux(2, 5, 44))
    return CreateEpollImpl(outp, 0);
  return PollerFactory::CreatePollImpl(outp);
}
