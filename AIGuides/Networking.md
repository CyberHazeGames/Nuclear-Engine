# Platform networking

[Networking.h](../Nuclear.Platform/include/Nuclear.Platform/Networking.h) provides the transport foundation for future engine networking and replication. The public API is in `Nuclear::Platform::Networking`, uses standard C++20 types, and exposes no native socket handles or operating-system headers. Only the Windows Winsock implementation exists today.

## Responsibilities and lifetime

- `Endpoint` stores IPv4/IPv6 address bytes in network order, a port in host order, and an optional numeric IPv6 interface scope. `ParseEndpoint` parses numeric addresses without DNS. `ResolveHost` resolves an ASCII hostname or numeric address and removes duplicate endpoints; it is synchronous and belongs off the frame thread.
- `Socket` owns its native handle. It is noncopyable, movable, and closes on destruction; move assignment first closes the destination's previous socket. Call `Open` on an empty socket, and use `Close` before reopening or retrying a failed connection.
- Winsock starts lazily and is shared through internal runtime references held by each socket and resolver operation. A socket closes before its runtime reference is released. No window, `Platform::Initialize`, event pump, or main-thread service is required. Networking is independent of window shutdown. Keep Nuclear.Platform loaded until all its sockets are destroyed.
- Different sockets may be used on different threads. Externally synchronize every operation on the same socket, including `Close`, moves, and destruction. The wrapper does not provide concurrent cancellation.
- The foundation owns address translation, socket operations, readiness, native diagnostics, and basic socket options. Engine code will own peer/session identities, connection policy, message formats, serialization, replication, reliability, sequencing, bandwidth budgets, and encryption/authentication. None of those higher-level protocols are implemented here.

## UDP and game-loop use

`Open` creates a nonblocking socket by default. Bind an IPv4/IPv6 endpoint; port zero requests an ephemeral local port, retrievable through `GetLocalEndpoint`. IPv6 sockets are IPv6-only, so bind a separate IPv4 socket when serving both families. `SetBroadcast` is available only for IPv4 UDP.

Each `SendTo` sends one datagram, and each `ReceiveFrom` consumes one. Datagrams may be lost, duplicated, reordered, or exceed a path's practical packet size. Keep replication packet budgets in the future engine transport/session layer; a successful local send is not a delivery acknowledgement.

`ReceiveFrom` requires a nonempty buffer and writes the sender only on success. A zero-byte success represents a valid empty datagram. `MessageTooLarge` means an oversized datagram was consumed; discard the buffer contents, which may have been modified, and do not interpret them as a complete packet. `WouldBlock` means no datagram is currently queued.

For example, an engine can bound the packets processed each tick:

```cpp
#include <Nuclear.Platform/Networking.h>
#include <array>
#include <iostream>

void PumpPackets(Nuclear::Platform::Networking::Socket& socket)
{
    using namespace Nuclear::Platform::Networking;
    std::array<std::byte, 1200> buffer{};

    for (int packet = 0; packet < 64; ++packet)
    {
        Endpoint sender;
        const auto result = socket.ReceiveFrom(buffer, sender);
        if (result.mError == NetworkError::WouldBlock)
        {
            break;
        }
        if (result.mError == NetworkError::MessageTooLarge)
        {
            continue;
        }
        if (!result)
        {
            std::cerr << DescribeError(result) << '\n';
            break;
        }

        const auto payload = std::span<const std::byte>(buffer).first(result.mBytesTransferred);
        // Pass payload and sender to the engine's packet decoder before reusing buffer.
    }
}
```

This example's 1200-byte buffer and 64-packet tick limit are caller policy, not protocol constants enforced by Nuclear.Platform.

## TCP and readiness

For a server, `Open(..., SocketType::TCP)`, bind, `Listen`, and `Accept` into an empty socket. A nonblocking listener returns `WouldBlock` when no connection is pending. Accepted sockets inherit the wrapper's blocking mode.

For a client, call `Connect`. Immediate success establishes the stream; `InProgress` means the connection is pending. Use `Wait(SocketEvents::Writable, timeout, ready)`, then **call `FinishConnect` even if readiness reports an error**. The Windows implementation checks `SO_ERROR`; writable/error readiness alone must not be treated as a successful connection. `FinishConnect` itself polls without blocking and returns `InProgress` until the attempt completes. After failure, close/reopen before retrying.

`Wait` accepts 0..INT_MAX milliseconds. Zero polls immediately, and `TimedOut` leaves `ready` empty. Errors are monitored even when only readable/writable readiness was requested. A readable TCP socket can represent orderly EOF; `Receive` distinguishes that with `ConnectionClosed`.

`Send` and `Receive` perform one operation and may transfer only part of a stream payload. Retain unsent bytes, accumulate received bytes, and implement message framing above the foundation. Empty `Send` succeeds with zero bytes; empty receive buffers are rejected. `Shutdown(ShutdownMode::Send)` half-closes a TCP stream so the peer receives EOF after queued bytes, while replies can still be received. The handle remains owned until `Close`.

Use `SetNoDelay` for TCP latency policy and `SetBufferSizes` to request positive send/receive buffer sizes. Native limits may adjust buffer requests. Blocking mode is available for worker-thread use through `SetBlocking`; changing it while a connect is pending is rejected. No implicit send-all loops, background receive threads, or unbounded waits are introduced by the default API.

## Errors and implementation

Every operation returns `NetworkResult`: a portable error category, original native diagnostic code where available, and the number of valid transferred bytes on success. `WouldBlock`, `InProgress`, `TimedOut`, and `ConnectionClosed` have distinct meanings. Output endpoints and resolution lists remain unchanged on failure. `DescribeError` formats a result without consulting mutable global last-error state.

Windows code is split between [Win32Networking.cpp](../Nuclear.Platform/Source/Windows/Win32Networking.cpp) for runtime/address/resolution/error services and [Win32Socket.cpp](../Nuclear.Platform/Source/Windows/Win32Socket.cpp) for transport operations. [Win32Networking.h](../Nuclear.Platform/Source/Windows/Win32Networking.h) contains private Winsock types. The platform project links the Windows SDK's `ws2_32.lib`; there is no new external networking library. Future backends must preserve the documented ownership, nonblocking, endpoint, datagram, partial-stream, and error contracts.

## Validation

The separate [headless networking test project](../Nuclear.Platform/Nuclear.Platform.NetworkTests.vcxproj) is outside the default solution build. In the Visual Studio developer environment described in [BuildAndValidation.md](BuildAndValidation.md), run:

```powershell
msbuild Nuclear.Platform/Nuclear.Platform.NetworkTests.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
./Bin/DebugX64/Nuclear.Platform.NetworkTests.exe
```

Use `Release` and `Bin/ReleaseX64` for the optimized build. Tests use ephemeral loopback-only ports, offline localhost/numeric address resolution, and bounded waits. They cover IPv4/IPv6 UDP/TCP payloads, empty/truncated datagrams, nonblocking readiness, refused connections, partial stream processing, TCP half-close/EOF, move/reopen ownership, invalid inputs, and concurrent independent socket lifetimes. They deliberately never initialize the window backend. LAN/WAN connectivity, packet loss recovery, replication, protocol compatibility, authentication, and encryption are not validated or implemented by these tests.

Validation on 2026-11-06: these headless checks passed in Debug and Release on Windows x64, including both IP families. `BuildSupport/Build.ps1 -Configuration Debug` also built the full engine/editor/sample solution with the new networking exports. A full Release engine build and external-network interoperability were not tested.
