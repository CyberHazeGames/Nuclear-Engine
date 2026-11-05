#pragma once

// Winsock2 must precede any Windows.h include in networking translation units.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Nuclear.Platform/Networking.h>
#include <memory>

namespace Nuclear::Platform::Networking::Detail
{
	/// Balances one successful WSAStartup with WSACleanup after its final socket/user.
	struct Runtime
	{
		/// Starts Winsock 2.2 and stores its result without touching window backend state.
		Runtime();
		/// Cleans up only if startup succeeded.
		~Runtime();
		NetworkResult mStartup; ///< Immutable startup outcome after construction.
	};

	/// Native socket ownership and state, allocated and destroyed within the foundation DLL.
	struct SocketState
	{
		/// Closes the native handle before releasing the networking runtime token.
		~SocketState();
		SOCKET mHandle = INVALID_SOCKET; ///< Owned Winsock handle.
		std::shared_ptr<Runtime> pRuntime; ///< Keeps Winsock active through handle destruction.
		AddressFamily mFamily = AddressFamily::IPv4; ///< Concrete socket address family.
		SocketType mType = SocketType::UDP; ///< UDP datagrams or TCP stream.
		bool mBlocking = false; ///< Mode applied to this socket and future accepted sockets.
		bool mConnecting = false; ///< A nonblocking connect awaits completion checking.
		bool mConnected = false; ///< TCP connection has completed or was accepted.
		bool mListening = false; ///< TCP socket accepts peers instead of sending stream data.
	};

	/// Acquires a shared networking lifetime, creating it on first use; safe across threads.
	std::shared_ptr<Runtime> AcquireRuntime();
	/// Maps native errors to stable API categories, retaining the original diagnostic code.
	NetworkResult NativeError(int error);
	/// Checks a public family value, optionally allowing the resolution-only Any value.
	bool IsFamily(AddressFamily family, bool allowAny = false);
	/// Checks a public transport value before passing it to the native API.
	bool IsSocketType(SocketType type);
	/// Returns the Windows family constant; callers must validate family first.
	int NativeFamily(AddressFamily family);
	/// Translates a portable endpoint into native storage; returns false for invalid fields.
	bool EncodeEndpoint(const Endpoint& endpoint, sockaddr_storage& address, int& length);
	/// Translates native IPv4/IPv6 storage into a normalized portable endpoint.
	bool DecodeEndpoint(const sockaddr* address, Endpoint& endpoint);
}
