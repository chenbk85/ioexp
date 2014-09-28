// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
#ifndef _include_amio_windows_util_h_
#define _include_amio_windows_util_h_

#include <amio-windows.h>

namespace amio {

bool CanEnableImmediateDelivery();
PassRef<IOError> EnableImmediateDelivery(HANDLE handle);

} // namespace amio

#endif // _include_amio_windows_util_h_