// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#ifndef _include_amio_windows_header_h_
#define _include_amio_windows_header_h_

#include <amio-types.h>
#define WIN32_LEAN_AND_MEAN
#define FD_SETSIZE 256
#include <Windows.h>
#include <WinSock2.h>

namespace amio {

// Forward declarations for internal types.
class WinTransport;
class WinContext;

// Forward declarations.
class IOListener;

// IO operations must be associated with a context object. A context can be
// associated with at most one operation at a time. Internally, this wraps
// an OVERLAPPED structure from WINAPI.
class IOContext : public ke::RefcountedThreadsafe<IOContext>
{
 public:
  virtual ~IOContext()
  {}

  // Pre-allocate an IOContext with given user data.
  static PassRef<IOContext> New(uintptr_t data = 0);

  // Return user data (defaults to 0). This should not be called while the
  // context is being used by an IO operation.
  virtual uintptr_t UserData() const = 0;

  // Set arbitrary user data. This should not be called while the context is
  // being used by an IO operation. The old user data is returned.
  virtual uintptr_t SetUserData(uintptr_t data) = 0;

  // Access to internal types.
  virtual WinContext *toWinContext() = 0;
};

// The result of an IO operation. Testing the state of an IO operation typically
// should look like this:
//
//  IOResult r;
//  if (!transport->Read(&r, ...))
//     // return r.Error
//  ... exit back to the event loop.
// 
// However, it is possible to poll data faster by using the "Completed" bit
// in the result. If it is true, it is safe to immediately read or re-use
// bytes from the buffer given to the IO operation. However, unless the message
// pump has "ImmediateDelivery" enabled, the context may not be re-used
// for further operations until the corresponding event has been delivered to
// its listener. Since immediate delivery is not available on all versions of
// Windows, a fully-functional read loop might look like:
//
//  Ref<IOError> ProcessIO(Ref<Transport> transport, IOResult &r) {
//    if (!expecting_io_)
//      return nullptr;
//    ProcessBytesFrom(r);
//    while (true) {
//      expecting_io_ = false;
//
//      IOResult r;
//      if (!transport->Read(&r, r.context, ...))
//        return r.Error
//      if (!r.Completed)
//        return nullptr;
//
//      ProcessBytesFrom(r);
//
//      // Assuming we have one event active at a time, ignore the next event. In
//      // a system with multiple active operations per transport, we would have
//      // to communicate this through user data instead.
//      if (!r.Context) {
//        // Can't re-use the context, so wait until we get the event, then
//        // ignore it.
//        expecting_io_ = false;
//        return nullptr;
//      }
//    }
//  }
//
// If a system requires that immediate delivery is enabled, then there is no
// need to track whether or not the Context was set.
struct AMIO_CLASS IOResult
{
  // Set if there was an error.
  ke::Ref<IOError> Error;

  // True if a connection has received an orderly shutdown from its peer. If
  // Ended is true, then the socket is automatically removed from the poller.
  bool Ended;

  // When reading messages from a message-based stream (such as message pipes,
  // or UDP), and the given buffer was not large enough to read the entire
  // message, one of these will be true. Whether or not the remaining data can
  // be read depends on which is set. MoreData indicates that it can; Truncated
  // indicates that it cannot.
  bool MoreData;
  bool Truncated;

  // If true, the operation completed immediately and |Bytes| contains the
  // number of bytes that completed. Any structures associated with the IO
  // operation may be freed or re-used.
  //
  // If false, the operation is still pending.
  bool Completed;

  // Number of bytes that successfully completed. If 0 and Ended is false,
  // then no bytes were received or sent, and the caller should wait for
  // another read or write event to try again.
  size_t Bytes;

  // If the operation completed successfully and the underlying IO event is
  // is no longer queued (that is, no notification will be passed to the
  // listener), then this will contain the context used to start the operation.
  // Otherwise, it is null.
  Ref<IOContext> Context;

  IOResult()
   : Ended(false), MoreData(false), Truncated(false), Completed(false), Bytes(0)
  {}
  IOResult(PassRef<IOError> error, PassRef<IOContext> context)
   : Error(error), Ended(false), MoreData(false), Truncated(false),
     Completed(false), Bytes(0), Context(context)
  {}
};

// Describes a low-level transport mechanism used in Windows. All posted
// operations will be resolved in the order they are posted, however, the
// notification of completed events is not guaranteed to happen in-order.
// It is the application's responsibility to resolve this.
//
// Functions are not thread-safe unless otherwise noted.
class AMIO_CLASS Transport : public ke::RefcountedThreadsafe<Transport>
{
 public:
  virtual ~Transport()
  {}

  // Initiates a read operation on the supplied buffer. If the operation fails
  // to initiate, then false is returned and r->Error is set. If the operation
  // cannot complete immediately, |r->Completed| will be false and all other
  // fields will be 0.
  //
  // If the operation can complete immediately, |r->Completed| will be true and
  // all fields will be filled as if they had come from the IOListener OnRead
  // event. If |r->Context| is non-null, then it is equal to |context| and no
  // event will be delivered through the poller. Otherwise, |context| is null
  // and the event will be delivered through the poller.
  //
  // Contexts must not be re-used until an IOResult is returned with the
  // context, either through immediate completion or delivery through an event
  // listener.
  //
  // Thread-safe with respect to other Transport operations, and Poller::Poll().
  virtual bool Read(IOResult *r, ke::Ref<IOContext> context, void *buffer, size_t length) = 0;

  // Initiates a write operation using the supplied buffer. If the operation
  // fails to initiate, then false is returned and r->Error is set. If the
  // operation cannot complete immediately, |r->Completed| will be false and
  // all other fields will be 0.
  //
  // If the operation can complete immediately, |r->Completed| will be true and
  // all fields will be filled as if they had come from the IOListener OnWrite
  // event. If |r->Context| is non-null, then it is equal to |context| and no
  // event will be delivered through the poller. Otherwise, |context| is null
  // and the event will be delivered through the poller.
  //
  // Contexts must not be re-used until an IOResult is returned with the
  // context, either through immediate completion or delivery through an event
  // listener.
  //
  // Thread-safe with respect to other Transport operations, and Poller::Poll().
  virtual bool Write(IOResult *r, ke::Ref<IOContext> context, const void *buffer, size_t length) = 0;

  // Helper version of Read() that automatically allocates a new context.
  //
  // An optional data value may be communicated through the event.
  //
  // Thread-safe with respect to other Transport operations, and Poller::Poll().
  IOResult Read(void *buffer, size_t length, uintptr_t data = 0);

  // Helper version of Write() that automatically allocates a new context.
  //
  // An optional data value may be communicated through the event.
  //
  // Thread-safe with respect to other Transport operations, and Poller::Poll().
  IOResult Write(const void *buffer, size_t length, uintptr_t data = 0);

  // Close the transport. This does not guarantee any pending IO events will
  // be cancelled; events may still be posted to the poller if a transport is
  // closed before all pending operations complete. However, the poller will
  // discard any events on closed transports.
  virtual void Close() = 0;

  // Returns true if the handle has been closed.
  virtual bool Closed() = 0;

  // Returns the underlying handle associated with this transport. If the
  // transport has been closed, INVALID_HANDLE_VALUE is returned instead.
  //
  // IO operations must not be performed outside of the Transport API,
  // otherwise the poller will (almost certainly) crash.
  virtual HANDLE Handle() = 0;

  // Access to internal types.
  virtual WinTransport *toWinTransport() = 0;
};

// Status listeners for transports.
class AMIO_CLASS IOListener : public ke::RefcountedThreadsafe<IOListener>
{
 public:
  virtual ~IOListener()
  {}

  // Receives any read events posted from a Read() operation on a transport.
  virtual void OnRead(ke::Ref<Transport> transport, IOResult &io)
  {}

  // Receives any write events posted from a Write() operation on a transport.
  virtual void OnWrite(ke::Ref<Transport> transport, IOResult &io)
  {}
};

enum TransportFlags
{
  // Override the default behavior of transports and do not automatically
  // close their underlying operating system resources.
  kTransportNoAutoClose       = 0x00000001,
  
  // Use immediate delivery. This cannot be specified when instantiating
  // transports from existing handles.
  kTransportImmediateDelivery = 0x00000002,

  kTransportDefaultFlags = 0x00000000
};

// Transport factory methods.
class AMIO_CLASS TransportFactory
{
 public:
  // Create a transport around an existing IO handle. The Handle must be
  // compatible with WriteFile/ReadFile and IO Completion Ports. IO operations
  // must not be performed outside of the transport API.
  static PassRef<IOError> CreateFromFile(
    Ref<Transport> *outp,
    HANDLE handle,
    TransportFlags flags = kTransportDefaultFlags
  );

  // Creates a transport around an existing socket.
  static PassRef<IOError> CreateFromSocket(
    Ref<Transport> *outp,
    SOCKET socket,
    TransportFlags flags = kTransportDefaultFlags
  );
};

// Polling objects. Transports cannot be removed from polling objects once
// attached. Once attached, IO operations on the transport will always
// associate the underlying poller.
//
// Note that contexts are not destroyed when a poller is destroyed, because
// of the way asynchronous I/O works in Windows. Pending events may be sitting
// in the kernel's queue, or in the poller's queue. It is the user's
// responsibility to make sure these are flushed, otherwise once the poller is
// deleted or no longer in use, the C++ memory for the contexts (and their
// transports) will be leaked.
//
// Functions are not thread-safe unless otherwise noted.
class AMIO_CLASS Poller : public ke::RefcountedThreadsafe<Poller>
{
 public:
  virtual ~Poller()
  {}

  // Poll for new events. If timeoutMs is 0, then polling will not block. If
  // timeoutMs is greater than 0, it may block for at most that many
  // milliseconds. If timeoutMs is kNoTimeout, it may block indefinitely.
  //
  // Poll() may be called from any thread.
  virtual PassRef<IOError> Poll(int timeoutMs) = 0;

  // Register a transport to an IO listener. The transport is permanently
  // attached to the poller; it is automatically removed when the transport
  // is closed.
  //
  // Attach() may be called from any thread.
  virtual PassRef<IOError> Attach(Ref<Transport> transport, Ref<IOListener> listener) = 0;

  // Wait for all pending IO events to complete, and discard them. This will
  // ensure that no memory is leaked before deleting a Poller. All other polling
  // threads should be stopped before calling this function. This will also
  // block until all events have been received.
  virtual void WaitAndDiscardPendingEvents() = 0;

  // Attempt to put attached transports into immediate delivery mode. If
  // immediate delivery mode is enabled, Read() and Write() calls may return
  // immediately (if able) without posting an event to the poller. Immediate
  // delivery is only available on Windows Vista or higher, or Windows Server
  // 2008 or higher.
  //
  // It is recommended to use this mode when possible as it means less events
  // being delivered. However, code must be careful to account for when it is
  // not available (see the comment above IOResult). Code must also be careful
  // not to starve a non-polling thread by continually reading data.
  //
  // Returns true on success, false if immediate delivery mode is not
  // available.
  virtual bool EnableImmediateDelivery() = 0;

  // This is the same as EnableImmediateDelivery, except that if a transport
  // cannot enter immediate delivery mode, it will fail to attach. This allows
  // you to handle less edge cases.
  virtual bool RequireImmediateDelivery() = 0;
};

class AMIO_CLASS PollerFactory
{
 public:
  // Create a poller. By default the backing implementation uses IOCP
  // (I/O Completion Ports) for a single thread, using the default batch
  // version if supported.
  static PassRef<IOError> CreatePoller(Ref<Poller> *poller);

  // Create an I/O Completion Port poller.
  static PassRef<IOError> CreateCompletionPort(Ref<Poller> *poller, size_t nConcurrentThreads);
};

// Flags for Socket:CreateFrom.
enum SocketFlags
{
  // Override the default behavior of sockets and do not automatically
  // close their underlying operating system resources.
  kSocketNoAutoClose       = 0x00000001,
  
  kSocketDefaultFlags      = 0x00000000
};

class WinSocket;

// For sockets, and sockets only, Windows has Unix-like I/O API. We make that
// available here, as it is more convenient, albeit less efficient than IOCP.
// This API is not compatible with the Transport-based API, and it is not
// threadsafe.
class AMIO_CLASS Socket : public ke::RefcountedThreadsafe<Socket>
{
 public:
  virtual ~Socket()
  {}

  // Create a socket around an existing socket handle.
  static PassRef<IOError> CreateFrom(Ref<Socket> *outp, SOCKET s, SocketFlags flags);

  // Attempts to read a number of bytes from the socket into the provided
  // |buffer|, up to |maxlength| bytes. If any bytes are read, the number
  // of bytes is set in |result| accordingly.
  //
  // If the connection has been closed, |Closed| will be true in |result|.
  // 
  // If an error occurs, |Error| will be set in |result|, and the result will
  // be false.
  virtual bool Read(IOResult *result, void *buffer, size_t maxlength) = 0;

  // Attempts to write a number of bytes to the socket. If the socket
  // is connectionless (such as a datagram socket), all bytes are guaranteed
  // to be sent, unless the message is too large. For streams, only a partial
  // number of bytes may be sent. The number of bytes sent may be 0 without
  // an error occurring.
  //
  // By default, socket pollers do not listen for write events until a write
  // event would block. To initiate write status events, you must attempt to
  // call Write() at least once.
  //
  // If an error occurs, |Error| will be set in |result|, and the result will
  // be false.
  virtual bool Write(IOResult *result, const void *buffer, size_t maxlength) = 0;

  // Closes the transport.
  virtual void Close() = 0;

  // Returns whether or not the transport has been closed.
  virtual bool Closed() const = 0;

  // Return the underlying socket.
  virtual SOCKET Handle() const = 0;

  // Internal function to cast sockets to their underlying type.
  virtual WinSocket *toWinSocket() = 0;
};

// Used to receive notifications about status changes.
class AMIO_CLASS SocketListener : public ke::RefcountedThreadsafe<SocketListener>
{
 public:
  virtual ~SocketListener()
  {}

  // Called when data is available for non-blocking reading.
  virtual void OnReadReady(ke::Ref<Socket> socket)
  {}

  // Called when data is available for non-blocking sending.
  virtual void OnWriteReady(ke::Ref<Socket> socket)
  {}

  // Called when a connection has been closed by a peer. This is the same as
  // Read() returning a Closed status, however, some message pumps can detect
  // this as its own event and return it earlier.
  //
  // If this event has been received, the socket is automatically
  // deregistered beforehand.
  virtual void OnHangup(ke::Ref<Socket> socket)
  {}

  // Called when an error state is received.
  //
  // If this event has been received, the socket is automatically
  // deregistered beforehand.
  virtual void OnError(ke::Ref<Socket> socket, ke::Ref<IOError> error)
  {}
};

// Poller variant only for sockets. SocketPollers are not thread-safe.
class AMIO_CLASS SocketPoller
{
 public:
   // Poll for new events. If |timeoutMs| is greater than zero, Poll() may block
   // for at most that many milliseconds. If the poller has no sockets registered 
   // Poll() will exit immediately without an error.
   //
   // An error is returned if the poll itself failed; individual read/write
   // failures are propagated through status listeners.
   //
   // Poll() is not re-entrant.
   virtual PassRef<IOError> Poll(int timeoutMs = kNoTimeout) = 0;

   // Registers a socket with the pump. A socket can be registered to at
   // most one pump at any given time. Only sockets created via
   // SocketFactory can be registered.
   virtual PassRef<IOError> Attach(Ref<Socket> socket, Ref<SocketListener> listener) = 0;

   // Deregisters a socket from a pump. This happens automatically if the
   // socket is closed, a status error or hangup is generated, or a Read()
   // operation returns Ended. It is safe to deregister a socket multiple
   // times.
   virtual void Detach(Ref<Socket> socket) = 0;
};

class AMIO_CLASS SocketPollerFactory
{
  // Creates the best socket poller available.
  static PassRef<IOError> CreatePoller();

  // Create a poll() poller. This can only be used with socket-based
  // transports. maxEventsPerPoll defaults to 64 if left as 0.
  //
  // Only available on Windows vista or higher.
  //
  // Due to a bug (http://curl.haxx.se/mail/lib-2012-10/0038.html) in WSAPoll,
  // we do not create a WSAPoll-based socket poller by default, even if it is
  // available.
  static PassRef<IOError> CreateSocketPollImpl(Ref<Poller> *poller, size_t maxEventsPerPoll = 0);

  // Create a select() poller. This can only be used with socket-based
  // transports.
  static PassRef<IOError> CreateSocketSelectImpl(Ref<Poller> *poller);
};

} // namespace amio

#endif // _include_amio_windows_header_h_