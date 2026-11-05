#pragma once

#include <Nuclear.Platform/Export.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Nuclear::Platform::Networking
{
	/// Internet address families. Any is only valid as a hostname-resolution filter.
	enum class AddressFamily { Any, IPv4, IPv6 };

	/// UDP preserves datagram boundaries; TCP provides an ordered byte stream.
	enum class SocketType { UDP, TCP };

	/// Portable operation outcomes. WouldBlock and InProgress are normal asynchronous states.
	enum class NetworkError
	{
		None,
		WouldBlock,
		InProgress,
		ConnectionClosed,
		ConnectionRefused,
		ConnectionReset,
		NotConnected,
		TimedOut,
		AddressInUse,
		AddressNotAvailable,
		NetworkUnreachable,
		MessageTooLarge,
		InvalidArgument,
		NotOpen,
		NotSupported,
		HostNotFound,
		SystemError
	};

	/// Result of one operation; diagnostics belong to the result rather than global state.
	struct NetworkResult
	{
		NetworkError mError = NetworkError::None; ///< None means the operation succeeded.
		int mNativeError = 0; ///< Backend error for diagnostics; zero for locally checked failures.
		std::size_t mBytesTransferred = 0; ///< Valid send/receive bytes on success; zero on failure.

		/// Tests for success. Check mError explicitly when handling asynchronous outcomes.
		explicit operator bool() const { return mError == NetworkError::None; }
	};

	/// Numeric address and port. No native sockaddr type appears in the public API.
	struct NPAPI Endpoint
	{
		AddressFamily mFamily = AddressFamily::IPv4; ///< IPv4 or IPv6; Any is not a usable endpoint.
		std::array<std::uint8_t, 16> mAddress{}; ///< Network-order bytes; IPv4 uses the first four.
		std::uint16_t mPort = 0; ///< Host-order port; bind to zero to request an ephemeral port.
		std::uint32_t mScopeId = 0; ///< IPv6 interface scope; zero for unscoped addresses and IPv4.

		/// Returns the wildcard bind address for the requested concrete family.
		static Endpoint Any(std::uint16_t port = 0, AddressFamily family = AddressFamily::IPv4);
		/// Returns 127.0.0.1 or ::1, suitable for local communication and tests.
		static Endpoint Loopback(std::uint16_t port = 0, AddressFamily family = AddressFamily::IPv4);
		/// Formats address:port, or [address%scope]:port for IPv6; empty for invalid families.
		std::string ToString() const;
		/// Compares the numeric family, address, port, and IPv6 scope.
		bool operator==(const Endpoint&) const = default;
	};

	/// Parses a numeric IPv4/IPv6 address without DNS. Output is unchanged on failure.
	/// Supply IPv6 scope separately; brackets, port suffixes, and zone suffixes are rejected.
	NPAPI NetworkResult ParseEndpoint(const std::string& address, std::uint16_t port,
		Endpoint& endpoint, std::uint32_t scopeId = 0);

	/// Resolves an ASCII hostname or numeric address synchronously; call off the frame thread.
	/// Replaces endpoints on success, preserves it on failure, and removes duplicate results.
	NPAPI NetworkResult ResolveHost(const std::string& host, std::uint16_t port,
		std::vector<Endpoint>& endpoints, AddressFamily family = AddressFamily::Any,
		SocketType type = SocketType::UDP);

	/// Returns a human-readable description, including the native error when available.
	NPAPI std::string DescribeError(const NetworkResult& result);

	/// Readiness indications. Readable also covers TCP EOF; confirm it with Receive().
	enum class SocketEvents : std::uint8_t { None = 0, Readable = 1, Writable = 2, Error = 4 };

	/// Combines readiness interests or indications.
	constexpr SocketEvents operator|(SocketEvents left, SocketEvents right)
	{
		return static_cast<SocketEvents>(static_cast<unsigned>(left) | static_cast<unsigned>(right));
	}

	/// Selects the readiness bits shared by both arguments.
	constexpr SocketEvents operator&(SocketEvents left, SocketEvents right)
	{
		return static_cast<SocketEvents>(static_cast<unsigned>(left) & static_cast<unsigned>(right));
	}

	/// TCP shutdown direction. Send allows the peer to read pending bytes before EOF.
	enum class ShutdownMode { Receive, Send, Both };

	namespace Detail { struct SocketState; }

	/// Owns one socket and its networking runtime reference; movable, never copyable.
	/// Networking initializes lazily and works without Platform::Initialize() or a window.
	/// Different sockets may be used on different threads. Synchronize all access to the same
	/// Socket, including moves and Close(). The foundation DLL must outlive its sockets.
	class NPAPI Socket
	{
	public:
		/// Constructs an empty socket without initializing the networking runtime.
		Socket() = default;
		/// Closes the socket and releases its runtime reference.
		~Socket();
		/// Transfers ownership, leaving the source empty.
		Socket(Socket&& other) noexcept;
		/// Closes this socket, then transfers ownership, leaving the source empty.
		Socket& operator=(Socket&& other) noexcept;
		Socket(const Socket&) = delete;
		Socket& operator=(const Socket&) = delete;

		/// Opens a nonblocking socket. This object must be empty and family must be concrete.
		/// IPv6 sockets are IPv6-only; open a separate IPv4 socket when serving both families.
		NetworkResult Open(AddressFamily family = AddressFamily::IPv4, SocketType type = SocketType::UDP);
		/// Closes immediately at the API boundary; repeated calls are safe. No implicit flush.
		void Close();
		/// Returns whether this object currently owns a native socket.
		bool IsOpen() const;

		/// Changes blocking mode. Blocking operations belong on a worker thread.
		/// Mode changes during an outstanding nonblocking connect are rejected.
		NetworkResult SetBlocking(bool blocking);
		/// Binds an address of the socket's family; port zero requests an available local port.
		NetworkResult Bind(const Endpoint& endpoint);
		/// Starts a TCP listener. Backlog must be positive; bind before calling for a chosen port.
		NetworkResult Listen(int backlog = 32);
		/// Accepts one TCP connection into an empty output socket; WouldBlock means no pending peer.
		/// The accepted socket inherits this wrapper's blocking mode. Peer is optional and borrowed.
		NetworkResult Accept(Socket& accepted, Endpoint* peer = nullptr);
		/// Starts a TCP connection. InProgress requires Wait(Writable, ...) and FinishConnect().
		/// After a failed connection, close and reopen before retrying another endpoint.
		NetworkResult Connect(const Endpoint& endpoint);
		/// Checks an outstanding TCP connect without blocking; writable alone does not prove success.
		/// Returns InProgress until ready, then the actual connection result.
		NetworkResult FinishConnect();

		/// Waits for requested readiness; errors are always monitored. Timeout is 0..INT_MAX ms.
		/// Zero polls immediately. TimedOut means no readiness; ready is cleared on every call.
		/// An Error indication during connect must be interpreted with FinishConnect().
		NetworkResult Wait(SocketEvents interest, std::chrono::milliseconds timeout, SocketEvents& ready) const;

		/// Sends TCP bytes once. Success may transfer only a prefix; retain/retry the remainder.
		/// An empty span succeeds with zero bytes. TCP message framing belongs to the caller.
		NetworkResult Send(std::span<const std::byte> data);
		/// Receives TCP bytes once into a nonempty buffer. ConnectionClosed means orderly peer EOF.
		NetworkResult Receive(std::span<std::byte> buffer);
		/// Sends exactly one UDP datagram; an empty span sends a valid zero-length datagram.
		/// Success reports the entire payload. Reliability, ordering, and packet budgets are caller policy.
		NetworkResult SendTo(std::span<const std::byte> data, const Endpoint& destination);
		/// Receives one UDP datagram into a nonempty buffer and writes its sender on success.
		/// A zero-byte success is a valid empty datagram. An oversized datagram is consumed and
		/// returns MessageTooLarge with zero valid bytes; discard any modified buffer contents.
		NetworkResult ReceiveFrom(std::span<std::byte> buffer, Endpoint& sender);

		/// Obtains the bound address/port; output is unchanged on failure.
		NetworkResult GetLocalEndpoint(Endpoint& endpoint) const;
		/// Obtains the connected TCP peer address/port; output is unchanged on failure.
		NetworkResult GetRemoteEndpoint(Endpoint& endpoint) const;
		/// Enables/disables TCP_NODELAY for latency-sensitive streams; TCP sockets only.
		NetworkResult SetNoDelay(bool enabled);
		/// Enables/disables UDP broadcast; IPv4 UDP sockets only.
		NetworkResult SetBroadcast(bool enabled);
		/// Requests positive OS send/receive buffer sizes in bytes; native limits may adjust them.
		/// If the receive setting fails, a successful send-buffer setting remains in effect.
		NetworkResult SetBufferSizes(int sendBytes, int receiveBytes);
		/// Shuts down one/both TCP directions while retaining ownership until Close().
		NetworkResult Shutdown(ShutdownMode mode = ShutdownMode::Both);

	private:
		Detail::SocketState* pState = nullptr; ///< Owned backend state; includes the runtime lifetime token.
	};
}
