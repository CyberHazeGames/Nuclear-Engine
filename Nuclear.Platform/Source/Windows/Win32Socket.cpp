#include "Win32Networking.h"
#include <climits>
#include <utility>

namespace Nuclear::Platform::Networking
{
	static NetworkResult CheckType(const Detail::SocketState* state, SocketType type)
	{
		if (!state)
		{
			return { NetworkError::NotOpen };
		}
		return state->mType == type ? NetworkResult{} : NetworkResult{ NetworkError::InvalidArgument };
	}

	static NetworkResult SetOption(const Detail::SocketState& state, int level, int option, int value)
	{
		if (setsockopt(state.mHandle, level, option, reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		return {};
	}

	Socket::~Socket()
	{
		Close();
	}

	Socket::Socket(Socket&& other) noexcept
		: pState(std::exchange(other.pState, nullptr))
	{
	}

	Socket& Socket::operator=(Socket&& other) noexcept
	{
		if (this != &other)
		{
			Close();
			pState = std::exchange(other.pState, nullptr);
		}
		return *this;
	}

	NetworkResult Socket::Open(AddressFamily family, SocketType type)
	{
		if (pState || !Detail::IsFamily(family) || !Detail::IsSocketType(type))
		{
			return { NetworkError::InvalidArgument };
		}
		auto state = std::make_unique<Detail::SocketState>();
		state->pRuntime = Detail::AcquireRuntime();
		if (!state->pRuntime->mStartup)
		{
			return state->pRuntime->mStartup;
		}
		state->mFamily = family;
		state->mType = type;
		state->mHandle = WSASocketW(Detail::NativeFamily(family), type == SocketType::UDP ? SOCK_DGRAM : SOCK_STREAM,
			type == SocketType::UDP ? IPPROTO_UDP : IPPROTO_TCP, nullptr, 0,
			WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
		if (state->mHandle == INVALID_SOCKET)
		{
			return Detail::NativeError(WSAGetLastError());
		}

		u_long nonblocking = 1;
		if (ioctlsocket(state->mHandle, FIONBIO, &nonblocking) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		if (family == AddressFamily::IPv6)
		{
			const auto result = SetOption(*state, IPPROTO_IPV6, IPV6_V6ONLY, 1);
			if (!result)
			{
				return result;
			}
		}
		pState = state.release();
		return {};
	}

	void Socket::Close()
	{
		delete std::exchange(pState, nullptr);
	}

	bool Socket::IsOpen() const
	{
		return pState != nullptr;
	}

	NetworkResult Socket::SetBlocking(bool blocking)
	{
		if (!pState)
		{
			return { NetworkError::NotOpen };
		}
		if (pState->mConnecting)
		{
			return { NetworkError::InvalidArgument };
		}
		u_long nonblocking = blocking ? 0 : 1;
		if (ioctlsocket(pState->mHandle, FIONBIO, &nonblocking) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		pState->mBlocking = blocking;
		return {};
	}

	NetworkResult Socket::Bind(const Endpoint& endpoint)
	{
		if (!pState)
		{
			return { NetworkError::NotOpen };
		}
		sockaddr_storage address{};
		int length = 0;
		if (endpoint.mFamily != pState->mFamily || !Detail::EncodeEndpoint(endpoint, address, length))
		{
			return { NetworkError::InvalidArgument };
		}
		if (bind(pState->mHandle, reinterpret_cast<const sockaddr*>(&address), length) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		return {};
	}

	NetworkResult Socket::Listen(int backlog)
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		if (backlog <= 0 || pState->mConnected || pState->mConnecting)
		{
			return { NetworkError::InvalidArgument };
		}
		if (listen(pState->mHandle, backlog) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		pState->mListening = true;
		return {};
	}

	NetworkResult Socket::Accept(Socket& accepted, Endpoint* peer)
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		if (accepted.IsOpen() || !pState->mListening)
		{
			return { NetworkError::InvalidArgument };
		}
		sockaddr_storage address{};
		int length = sizeof(address);
		auto state = std::make_unique<Detail::SocketState>();
		state->pRuntime = pState->pRuntime;
		state->mHandle = accept(pState->mHandle, reinterpret_cast<sockaddr*>(&address), &length);
		if (state->mHandle == INVALID_SOCKET)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		state->mFamily = pState->mFamily;
		state->mType = SocketType::TCP;
		state->mBlocking = pState->mBlocking;
		state->mConnected = true;
		u_long nonblocking = state->mBlocking ? 0 : 1;
		if (ioctlsocket(state->mHandle, FIONBIO, &nonblocking) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		Endpoint endpoint;
		if (!Detail::DecodeEndpoint(reinterpret_cast<const sockaddr*>(&address), endpoint))
		{
			return { NetworkError::NotSupported };
		}
		accepted.pState = state.release();
		if (peer)
		{
			*peer = endpoint;
		}
		return {};
	}

	NetworkResult Socket::Connect(const Endpoint& endpoint)
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		if (pState->mConnecting)
		{
			return { NetworkError::InProgress };
		}
		sockaddr_storage address{};
		int length = 0;
		if (pState->mConnected || pState->mListening || endpoint.mFamily != pState->mFamily ||
			!Detail::EncodeEndpoint(endpoint, address, length))
		{
			return { NetworkError::InvalidArgument };
		}
		if (connect(pState->mHandle, reinterpret_cast<const sockaddr*>(&address), length) != SOCKET_ERROR)
		{
			pState->mConnected = true;
			return {};
		}
		result = Detail::NativeError(WSAGetLastError());
		if (result.mError == NetworkError::WouldBlock || result.mError == NetworkError::InProgress)
		{
			pState->mConnecting = true;
			result.mError = NetworkError::InProgress;
		}
		return result;
	}

	NetworkResult Socket::FinishConnect()
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		if (pState->mConnected)
		{
			return {};
		}
		if (!pState->mConnecting)
		{
			return { NetworkError::NotConnected };
		}
		SocketEvents ready;
		result = Wait(SocketEvents::Writable, std::chrono::milliseconds(0), ready);
		if (result.mError == NetworkError::TimedOut)
		{
			return { NetworkError::InProgress };
		}
		if (!result)
		{
			return result;
		}

		int error = 0;
		int length = sizeof(error);
		if (getsockopt(pState->mHandle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		pState->mConnecting = false;
		if (error)
		{
			return Detail::NativeError(error);
		}
		pState->mConnected = (ready & SocketEvents::Writable) != SocketEvents::None;
		return pState->mConnected ? NetworkResult{} : NetworkResult{ NetworkError::NotConnected };
	}

	NetworkResult Socket::Wait(SocketEvents interest, std::chrono::milliseconds timeout, SocketEvents& ready) const
	{
		ready = SocketEvents::None;
		if (!pState)
		{
			return { NetworkError::NotOpen };
		}
		const auto bits = static_cast<unsigned>(interest);
		if (!bits || (bits & ~7u) || timeout.count() < 0 || timeout.count() > INT_MAX)
		{
			return { NetworkError::InvalidArgument };
		}
		fd_set readable{};
		fd_set writable{};
		fd_set errors{};
		if ((interest & SocketEvents::Readable) != SocketEvents::None)
		{
			FD_SET(pState->mHandle, &readable);
		}
		if ((interest & SocketEvents::Writable) != SocketEvents::None)
		{
			FD_SET(pState->mHandle, &writable);
		}
		FD_SET(pState->mHandle, &errors);
		timeval time{};
		time.tv_sec = static_cast<long>(timeout.count() / 1000);
		time.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
		const int count = select(0, &readable, &writable, &errors, &time);
		if (count == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		if (!count)
		{
			return { NetworkError::TimedOut };
		}
		if (FD_ISSET(pState->mHandle, &readable))
		{
			ready = ready | SocketEvents::Readable;
		}
		if (FD_ISSET(pState->mHandle, &writable))
		{
			ready = ready | SocketEvents::Writable;
		}
		if (FD_ISSET(pState->mHandle, &errors))
		{
			ready = ready | SocketEvents::Error;
		}
		return {};
	}

	NetworkResult Socket::Send(std::span<const std::byte> data)
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		if (!pState->mConnected)
		{
			return { pState->mConnecting ? NetworkError::InProgress : NetworkError::NotConnected };
		}
		if (data.size() > INT_MAX)
		{
			return { NetworkError::MessageTooLarge };
		}
		if (data.empty())
		{
			return {};
		}
		const int count = send(pState->mHandle, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
		return count == SOCKET_ERROR ? Detail::NativeError(WSAGetLastError()) :
			NetworkResult{ NetworkError::None, 0, static_cast<std::size_t>(count) };
	}

	NetworkResult Socket::Receive(std::span<std::byte> buffer)
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		if (!pState->mConnected)
		{
			return { pState->mConnecting ? NetworkError::InProgress : NetworkError::NotConnected };
		}
		if (buffer.empty() || buffer.size() > INT_MAX)
		{
			return { NetworkError::InvalidArgument };
		}
		const int count = recv(pState->mHandle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
		if (count == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		return count == 0 ? NetworkResult{ NetworkError::ConnectionClosed } :
			NetworkResult{ NetworkError::None, 0, static_cast<std::size_t>(count) };
	}

	NetworkResult Socket::SendTo(std::span<const std::byte> data, const Endpoint& destination)
	{
		auto result = CheckType(pState, SocketType::UDP);
		if (!result)
		{
			return result;
		}
		if (data.size() > INT_MAX)
		{
			return { NetworkError::MessageTooLarge };
		}
		sockaddr_storage address{};
		int length = 0;
		if (destination.mFamily != pState->mFamily || !Detail::EncodeEndpoint(destination, address, length))
		{
			return { NetworkError::InvalidArgument };
		}
		// Winsock requires a valid buffer pointer even for an empty datagram.
		const char empty = 0;
		const auto* bytes = data.empty() ? &empty : reinterpret_cast<const char*>(data.data());
		const int count = sendto(pState->mHandle, bytes, static_cast<int>(data.size()), 0,
			reinterpret_cast<const sockaddr*>(&address), length);
		return count == SOCKET_ERROR ? Detail::NativeError(WSAGetLastError()) :
			NetworkResult{ NetworkError::None, 0, static_cast<std::size_t>(count) };
	}

	NetworkResult Socket::ReceiveFrom(std::span<std::byte> buffer, Endpoint& sender)
	{
		auto result = CheckType(pState, SocketType::UDP);
		if (!result)
		{
			return result;
		}
		if (buffer.empty() || buffer.size() > INT_MAX)
		{
			return { NetworkError::InvalidArgument };
		}
		sockaddr_storage address{};
		int length = sizeof(address);
		const int count = recvfrom(pState->mHandle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()),
			0, reinterpret_cast<sockaddr*>(&address), &length);
		if (count == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		if (!Detail::DecodeEndpoint(reinterpret_cast<const sockaddr*>(&address), sender))
		{
			return { NetworkError::NotSupported };
		}
		return { NetworkError::None, 0, static_cast<std::size_t>(count) };
	}

	NetworkResult Socket::GetLocalEndpoint(Endpoint& endpoint) const
	{
		if (!pState)
		{
			return { NetworkError::NotOpen };
		}
		sockaddr_storage address{};
		int length = sizeof(address);
		if (getsockname(pState->mHandle, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		return Detail::DecodeEndpoint(reinterpret_cast<const sockaddr*>(&address), endpoint) ?
			NetworkResult{} : NetworkResult{ NetworkError::NotSupported };
	}

	NetworkResult Socket::GetRemoteEndpoint(Endpoint& endpoint) const
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		sockaddr_storage address{};
		int length = sizeof(address);
		if (getpeername(pState->mHandle, reinterpret_cast<sockaddr*>(&address), &length) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		return Detail::DecodeEndpoint(reinterpret_cast<const sockaddr*>(&address), endpoint) ?
			NetworkResult{} : NetworkResult{ NetworkError::NotSupported };
	}

	NetworkResult Socket::SetNoDelay(bool enabled)
	{
		auto result = CheckType(pState, SocketType::TCP);
		return result ? SetOption(*pState, IPPROTO_TCP, TCP_NODELAY, enabled ? 1 : 0) : result;
	}

	NetworkResult Socket::SetBroadcast(bool enabled)
	{
		auto result = CheckType(pState, SocketType::UDP);
		if (!result)
		{
			return result;
		}
		if (pState->mFamily != AddressFamily::IPv4)
		{
			return { NetworkError::InvalidArgument };
		}
		return SetOption(*pState, SOL_SOCKET, SO_BROADCAST, enabled ? 1 : 0);
	}

	NetworkResult Socket::SetBufferSizes(int sendBytes, int receiveBytes)
	{
		if (!pState)
		{
			return { NetworkError::NotOpen };
		}
		if (sendBytes <= 0 || receiveBytes <= 0)
		{
			return { NetworkError::InvalidArgument };
		}
		auto result = SetOption(*pState, SOL_SOCKET, SO_SNDBUF, sendBytes);
		return result ? SetOption(*pState, SOL_SOCKET, SO_RCVBUF, receiveBytes) : result;
	}

	NetworkResult Socket::Shutdown(ShutdownMode mode)
	{
		auto result = CheckType(pState, SocketType::TCP);
		if (!result)
		{
			return result;
		}
		int direction = SD_BOTH;
		switch (mode)
		{
		case ShutdownMode::Receive:
			direction = SD_RECEIVE;
			break;
		case ShutdownMode::Send:
			direction = SD_SEND;
			break;
		case ShutdownMode::Both: break;
		default: return { NetworkError::InvalidArgument };
		}
		if (shutdown(pState->mHandle, direction) == SOCKET_ERROR)
		{
			return Detail::NativeError(WSAGetLastError());
		}
		return {};
	}
}
