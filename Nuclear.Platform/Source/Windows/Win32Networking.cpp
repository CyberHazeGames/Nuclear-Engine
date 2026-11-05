#include "Win32Networking.h"
#include <algorithm>
#include <cstring>
#include <mutex>

namespace Nuclear::Platform::Networking::Detail
{
	Runtime::Runtime()
	{
		WSADATA data{};
		const int error = WSAStartup(MAKEWORD(2, 2), &data);
		if (error)
		{
			mStartup = NativeError(error);
		}
		else if (LOBYTE(data.wVersion) != 2 || HIBYTE(data.wVersion) != 2)
		{
			WSACleanup();
			mStartup = { NetworkError::NotSupported, WSAVERNOTSUPPORTED };
		}
	}

	Runtime::~Runtime()
	{
		if (mStartup)
		{
			WSACleanup();
		}
	}

	SocketState::~SocketState()
	{
		if (mHandle != INVALID_SOCKET)
		{
			closesocket(mHandle);
		}
	}

	std::shared_ptr<Runtime> AcquireRuntime()
	{
		static std::mutex mutex;
		static std::weak_ptr<Runtime> cachedRuntime;
		std::lock_guard lock(mutex);
		auto runtime = cachedRuntime.lock();
		if (!runtime)
		{
			runtime = std::make_shared<Runtime>();
			cachedRuntime = runtime;
		}
		return runtime;
	}

	NetworkResult NativeError(int error)
	{
		NetworkError category = NetworkError::SystemError;
		switch (error)
		{
		case 0: return {};
		case WSAEWOULDBLOCK:
			category = NetworkError::WouldBlock;
			break;
		case WSAEINPROGRESS:
		case WSAEALREADY:
			category = NetworkError::InProgress;
			break;
		case WSAESHUTDOWN:
			category = NetworkError::ConnectionClosed;
			break;
		case WSAECONNREFUSED:
			category = NetworkError::ConnectionRefused;
			break;
		case WSAECONNRESET:
		case WSAECONNABORTED:
			category = NetworkError::ConnectionReset;
			break;
		case WSAENOTCONN:
			category = NetworkError::NotConnected;
			break;
		case WSAETIMEDOUT:
			category = NetworkError::TimedOut;
			break;
		case WSAEADDRINUSE:
			category = NetworkError::AddressInUse;
			break;
		case WSAEADDRNOTAVAIL:
			category = NetworkError::AddressNotAvailable;
			break;
		case WSAENETUNREACH:
		case WSAEHOSTUNREACH:
		case WSAENETDOWN:
			category = NetworkError::NetworkUnreachable;
			break;
		case WSAEMSGSIZE:
			category = NetworkError::MessageTooLarge;
			break;
		case WSAEINVAL:
		case WSAEFAULT:
			category = NetworkError::InvalidArgument;
			break;
		case WSAENOTSOCK:
			category = NetworkError::NotOpen;
			break;
		case WSAEAFNOSUPPORT:
		case WSAEPROTONOSUPPORT:
		case WSAEOPNOTSUPP:
		case WSAVERNOTSUPPORTED:
			category = NetworkError::NotSupported;
			break;
		case WSAHOST_NOT_FOUND:
		case WSANO_DATA:
			category = NetworkError::HostNotFound;
			break;
		}
		return { category, error };
	}

	bool IsFamily(AddressFamily family, bool allowAny)
	{
		return family == AddressFamily::IPv4 || family == AddressFamily::IPv6 ||
			(allowAny && family == AddressFamily::Any);
	}

	bool IsSocketType(SocketType type)
	{
		return type == SocketType::UDP || type == SocketType::TCP;
	}

	int NativeFamily(AddressFamily family)
	{
		if (family == AddressFamily::IPv4)
		{
			return AF_INET;
		}
		return family == AddressFamily::IPv6 ? AF_INET6 : AF_UNSPEC;
	}

	bool EncodeEndpoint(const Endpoint& endpoint, sockaddr_storage& address, int& length)
	{
		address = {};
		if (endpoint.mFamily == AddressFamily::IPv4 && endpoint.mScopeId == 0)
		{
			auto& ipv4 = reinterpret_cast<sockaddr_in&>(address);
			ipv4.sin_family = AF_INET;
			ipv4.sin_port = htons(endpoint.mPort);
			std::memcpy(&ipv4.sin_addr, endpoint.mAddress.data(), 4);
			length = sizeof(ipv4);
			return true;
		}
		if (endpoint.mFamily == AddressFamily::IPv6)
		{
			auto& ipv6 = reinterpret_cast<sockaddr_in6&>(address);
			ipv6.sin6_family = AF_INET6;
			ipv6.sin6_port = htons(endpoint.mPort);
			ipv6.sin6_scope_id = endpoint.mScopeId;
			std::memcpy(&ipv6.sin6_addr, endpoint.mAddress.data(), 16);
			length = sizeof(ipv6);
			return true;
		}
		return false;
	}

	bool DecodeEndpoint(const sockaddr* address, Endpoint& endpoint)
	{
		Endpoint result;
		if (address->sa_family == AF_INET)
		{
			const auto& ipv4 = *reinterpret_cast<const sockaddr_in*>(address);
			result.mFamily = AddressFamily::IPv4;
			result.mPort = ntohs(ipv4.sin_port);
			std::memcpy(result.mAddress.data(), &ipv4.sin_addr, 4);
		}
		else if (address->sa_family == AF_INET6)
		{
			const auto& ipv6 = *reinterpret_cast<const sockaddr_in6*>(address);
			result.mFamily = AddressFamily::IPv6;
			result.mPort = ntohs(ipv6.sin6_port);
			result.mScopeId = ipv6.sin6_scope_id;
			std::memcpy(result.mAddress.data(), &ipv6.sin6_addr, 16);
		}
		else
		{
			return false;
		}
		endpoint = result;
		return true;
	}
}

namespace Nuclear::Platform::Networking
{
	Endpoint Endpoint::Any(std::uint16_t port, AddressFamily family)
	{
		Endpoint result;
		result.mFamily = family;
		result.mPort = port;
		return result;
	}

	Endpoint Endpoint::Loopback(std::uint16_t port, AddressFamily family)
	{
		auto result = Any(port, family);
		if (family == AddressFamily::IPv4)
		{
			result.mAddress[0] = 127;
			result.mAddress[3] = 1;
		}
		else if (family == AddressFamily::IPv6)
		{
			result.mAddress[15] = 1;
		}
		return result;
	}

	std::string Endpoint::ToString() const
	{
		auto runtime = Detail::AcquireRuntime();
		if (!runtime->mStartup || !Detail::IsFamily(mFamily) || (mFamily == AddressFamily::IPv4 && mScopeId))
		{
			return {};
		}
		char text[INET6_ADDRSTRLEN]{};
		if (!InetNtopA(Detail::NativeFamily(mFamily), mAddress.data(), text, sizeof(text)))
		{
			return {};
		}
		if (mFamily == AddressFamily::IPv4)
		{
			return std::string(text) + ":" + std::to_string(mPort);
		}
		const auto scope = mScopeId ? "%" + std::to_string(mScopeId) : std::string{};
		return "[" + std::string(text) + scope + "]:" + std::to_string(mPort);
	}

	NetworkResult ParseEndpoint(const std::string& address, std::uint16_t port,
		Endpoint& endpoint, std::uint32_t scopeId)
	{
		if (address.empty() || address.find('\0') != std::string::npos)
		{
			return { NetworkError::InvalidArgument };
		}
		auto runtime = Detail::AcquireRuntime();
		if (!runtime->mStartup)
		{
			return runtime->mStartup;
		}

		Endpoint result;
		result.mPort = port;
		if (InetPtonA(AF_INET, address.c_str(), result.mAddress.data()) == 1 && scopeId == 0)
		{
			endpoint = result;
			return {};
		}
		result.mAddress.fill(0);
		if (InetPtonA(AF_INET6, address.c_str(), result.mAddress.data()) == 1)
		{
			result.mFamily = AddressFamily::IPv6;
			result.mScopeId = scopeId;
			endpoint = result;
			return {};
		}
		return { NetworkError::InvalidArgument };
	}

	NetworkResult ResolveHost(const std::string& host, std::uint16_t port,
		std::vector<Endpoint>& endpoints, AddressFamily family, SocketType type)
	{
		if (host.empty() || host.find('\0') != std::string::npos ||
			!Detail::IsFamily(family, true) || !Detail::IsSocketType(type))
		{
			return { NetworkError::InvalidArgument };
		}
		auto runtime = Detail::AcquireRuntime();
		if (!runtime->mStartup)
		{
			return runtime->mStartup;
		}

		addrinfo hints{};
		hints.ai_family = Detail::NativeFamily(family);
		hints.ai_socktype = type == SocketType::UDP ? SOCK_DGRAM : SOCK_STREAM;
		hints.ai_protocol = type == SocketType::UDP ? IPPROTO_UDP : IPPROTO_TCP;
		hints.ai_flags = AI_NUMERICSERV;
		addrinfo* rawAddresses = nullptr;
		const auto service = std::to_string(port);
		const int error = getaddrinfo(host.c_str(), service.c_str(), &hints, &rawAddresses);
		if (error)
		{
			return Detail::NativeError(error);
		}
		std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(rawAddresses, freeaddrinfo);
		std::vector<Endpoint> resolved;
		for (auto* current = addresses.get(); current; current = current->ai_next)
		{
			Endpoint endpoint;
			if (Detail::DecodeEndpoint(current->ai_addr, endpoint) &&
				std::find(resolved.begin(), resolved.end(), endpoint) == resolved.end())
			{
				resolved.push_back(endpoint);
			}
		}
		if (resolved.empty())
		{
			return { NetworkError::HostNotFound };
		}
		endpoints = std::move(resolved);
		return {};
	}

	std::string DescribeError(const NetworkResult& result)
	{
		std::string description;
		switch (result.mError)
		{
		case NetworkError::None:
			description = "Success";
			break;
		case NetworkError::WouldBlock:
			description = "Operation would block";
			break;
		case NetworkError::InProgress:
			description = "Connection is in progress";
			break;
		case NetworkError::ConnectionClosed:
			description = "Connection closed";
			break;
		case NetworkError::ConnectionRefused:
			description = "Connection refused";
			break;
		case NetworkError::ConnectionReset:
			description = "Connection reset";
			break;
		case NetworkError::NotConnected:
			description = "Socket is not connected";
			break;
		case NetworkError::TimedOut:
			description = "Operation timed out";
			break;
		case NetworkError::AddressInUse:
			description = "Address is already in use";
			break;
		case NetworkError::AddressNotAvailable:
			description = "Address is not available";
			break;
		case NetworkError::NetworkUnreachable:
			description = "Network is unreachable";
			break;
		case NetworkError::MessageTooLarge:
			description = "Message exceeds buffer or transport limit";
			break;
		case NetworkError::InvalidArgument:
			description = "Invalid networking argument";
			break;
		case NetworkError::NotOpen:
			description = "Socket is not open";
			break;
		case NetworkError::NotSupported:
			description = "Networking operation is not supported";
			break;
		case NetworkError::HostNotFound:
			description = "Host has no matching address";
			break;
		default:
			description = "Native networking failure";
			break;
		}
		if (result.mNativeError)
		{
			description += " (native " + std::to_string(result.mNativeError) + ")";
		}
		return description;
	}
}
