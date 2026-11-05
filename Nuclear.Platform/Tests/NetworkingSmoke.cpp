#include <Nuclear.Platform/Networking.h>
#include <algorithm>
#include <array>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

using namespace Nuclear::Platform::Networking;
using namespace std::chrono_literals;

static_assert(!std::is_copy_constructible_v<Socket>);
static_assert(!std::is_copy_assignable_v<Socket>);
static_assert(std::is_nothrow_move_constructible_v<Socket>);
static_assert(std::is_nothrow_move_assignable_v<Socket>);

static void Require(bool condition, const std::string& message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

static void RequireSuccess(NetworkResult result, const char* operation)
{
	Require(static_cast<bool>(result), std::string(operation) + ": " + DescribeError(result));
}

static void RequireError(NetworkResult result, NetworkError expected, const char* operation)
{
	Require(result.mError == expected, std::string(operation) + ": " + DescribeError(result));
	Require(result.mBytesTransferred == 0, "Failed operation reported valid payload bytes");
}

static void WaitReady(Socket& socket, SocketEvents interest)
{
	SocketEvents ready = SocketEvents::None;
	RequireSuccess(socket.Wait(interest, 2s, ready), "Readiness wait");
	Require((ready & (interest | SocketEvents::Error)) != SocketEvents::None, "Readiness result had no requested event");
}

static void CheckAddresses()
{
	Endpoint endpoint;
	RequireSuccess(ParseEndpoint("127.0.0.1", 27015, endpoint), "Parse IPv4");
	Require(endpoint == Endpoint::Loopback(27015), "IPv4 byte order or port changed");
	Require(endpoint.ToString() == "127.0.0.1:27015", "IPv4 formatting failed");
	RequireSuccess(ParseEndpoint("::1", 27016, endpoint), "Parse IPv6");
	Require(endpoint == Endpoint::Loopback(27016, AddressFamily::IPv6), "IPv6 loopback changed");
	Require(endpoint.ToString() == "[::1]:27016", "IPv6 formatting failed");
	RequireSuccess(ParseEndpoint("fe80::1", 42, endpoint, 7), "Parse scoped IPv6");
	Require(endpoint.ToString() == "[fe80::1%7]:42", "IPv6 scope formatting failed");
	const auto previous = endpoint;
	RequireError(ParseEndpoint("not-an-address", 10, endpoint), NetworkError::InvalidArgument, "Reject invalid numeric address");
	RequireError(ParseEndpoint("127.0.0.1", 10, endpoint, 3), NetworkError::InvalidArgument, "Reject IPv4 scope");
	RequireError(ParseEndpoint("::1%3", 10, endpoint), NetworkError::InvalidArgument, "Reject embedded scope suffix");
	RequireError(ParseEndpoint(std::string("127.0.0.1\0ignored", 17), 10, endpoint), NetworkError::InvalidArgument, "Reject embedded null");
	Require(endpoint == previous, "Failed parsing modified the output endpoint");
	Require(Endpoint::Any(0, AddressFamily::IPv6).ToString() == "[::]:0", "IPv6 wildcard formatting failed");
	Require(Endpoint::Any(0, AddressFamily::Any).ToString().empty(), "Resolution-only family became an endpoint");

	std::vector<Endpoint> endpoints;
	RequireSuccess(ResolveHost("localhost", 27015, endpoints, AddressFamily::IPv4, SocketType::TCP), "Resolve localhost");
	Require(!endpoints.empty(), "Localhost had no IPv4 addresses");
	for (const auto& address : endpoints)
	{
		Require(address.mFamily == AddressFamily::IPv4 && address.mPort == 27015, "Resolver lost address family or port");
	}
	RequireSuccess(ResolveHost("::1", 27016, endpoints, AddressFamily::IPv6), "Resolve numeric IPv6");
	Require(endpoints.size() == 1 && endpoints.front() == Endpoint::Loopback(27016, AddressFamily::IPv6), "IPv6 resolution mismatch");
	const auto oldEndpoints = endpoints;
	RequireError(ResolveHost("", 0, endpoints), NetworkError::InvalidArgument, "Reject empty hostname");
	Require(endpoints == oldEndpoints, "Failed resolution modified output addresses");
}

static void CheckInvalidOperations()
{
	Socket empty;
	Socket output;
	Endpoint endpoint;
	std::array<std::byte, 16> buffer{};
	SocketEvents ready = SocketEvents::Readable;
	RequireError(empty.ReceiveFrom(buffer, endpoint), NetworkError::NotOpen, "Receive on empty socket");
	RequireError(empty.Send(buffer), NetworkError::NotOpen, "Send on empty socket");
	RequireError(empty.Accept(output), NetworkError::NotOpen, "Accept on empty socket");
	RequireError(empty.FinishConnect(), NetworkError::NotOpen, "Connect completion on empty socket");
	RequireError(empty.Wait(SocketEvents::Readable, 0ms, ready), NetworkError::NotOpen, "Wait on empty socket");
	Require(ready == SocketEvents::None, "Wait did not clear output readiness");
	RequireError(empty.Open(AddressFamily::Any), NetworkError::InvalidArgument, "Reject unspecified socket family");
	RequireError(empty.Open(AddressFamily::IPv4, static_cast<SocketType>(42)), NetworkError::InvalidArgument, "Reject unknown transport");
	Require(!empty.IsOpen(), "Invalid Open allocated a socket");
	empty.Close();
	empty.Close();

	RequireSuccess(empty.Open(), "Open UDP for invalid-operation checks");
	RequireError(empty.Open(), NetworkError::InvalidArgument, "Reject reopening an owned socket");
	RequireError(empty.Listen(), NetworkError::InvalidArgument, "Reject UDP listen");
	RequireError(empty.Connect(Endpoint::Loopback(5)), NetworkError::InvalidArgument, "Reject TCP connect on UDP");
	RequireError(empty.Send(buffer), NetworkError::InvalidArgument, "Reject stream send on UDP");
	RequireError(empty.SetNoDelay(true), NetworkError::InvalidArgument, "Reject TCP option on UDP");
	RequireError(empty.Shutdown(), NetworkError::InvalidArgument, "Reject stream shutdown on UDP");
	RequireError(empty.Bind(Endpoint::Any(0, AddressFamily::IPv6)), NetworkError::InvalidArgument, "Reject family mismatch");
	RequireError(empty.SetBufferSizes(0, 100), NetworkError::InvalidArgument, "Reject invalid buffer size");
	RequireError(empty.Wait(SocketEvents::None, 0ms, ready), NetworkError::InvalidArgument, "Reject empty readiness mask");
	RequireError(empty.Wait(SocketEvents::Readable, -1ms, ready), NetworkError::InvalidArgument, "Reject negative wait");
	RequireError(empty.ReceiveFrom({}, endpoint), NetworkError::InvalidArgument, "Reject empty receive buffer");
}

static void CheckUDP(AddressFamily family)
{
	Socket receiver;
	Socket sender;
	RequireSuccess(receiver.Open(family), "Open UDP receiver");
	RequireSuccess(sender.Open(family), "Open UDP sender");
	RequireSuccess(receiver.Bind(Endpoint::Loopback(0, family)), "Bind UDP receiver");
	RequireSuccess(sender.Bind(Endpoint::Loopback(0, family)), "Bind UDP sender");
	RequireSuccess(sender.SetBufferSizes(16384, 16384), "Set UDP buffers");
	if (family == AddressFamily::IPv4)
	{
		RequireSuccess(sender.SetBroadcast(true), "Enable UDP broadcast");
		RequireSuccess(sender.SetBroadcast(false), "Disable UDP broadcast");
	}
	else
	{
		RequireError(sender.SetBroadcast(true), NetworkError::InvalidArgument, "Reject IPv6 broadcast");
	}
	Endpoint destination;
	Endpoint source;
	RequireSuccess(receiver.GetLocalEndpoint(destination), "Get bound UDP endpoint");
	RequireSuccess(sender.GetLocalEndpoint(source), "Get sender endpoint");
	Require(destination.mPort && source.mPort, "Ephemeral port was not assigned");

	std::array<std::byte, 32> buffer{};
	Endpoint observed = Endpoint::Loopback(99, family);
	const auto sentinel = observed;
	const auto wouldBlock = receiver.ReceiveFrom(buffer, observed);
	RequireError(wouldBlock, NetworkError::WouldBlock, "Empty nonblocking UDP receive");
	Require(wouldBlock.mNativeError != 0 && observed == sentinel, "WouldBlock lost diagnostics or modified sender");
	SocketEvents ready;
	RequireError(receiver.Wait(SocketEvents::Readable, 0ms, ready), NetworkError::TimedOut, "Poll quiet UDP socket");
	Require(ready == SocketEvents::None, "Timed-out poll retained readiness");

	const std::array<std::byte, 5> payload{ std::byte{1}, std::byte{0}, std::byte{3}, std::byte{4}, std::byte{255} };
	auto sent = sender.SendTo(payload, destination);
	RequireSuccess(sent, "Send UDP datagram");
	Require(sent.mBytesTransferred == payload.size(), "Datagram send was partial");
	WaitReady(receiver, SocketEvents::Readable);
	auto received = receiver.ReceiveFrom(buffer, observed);
	RequireSuccess(received, "Receive UDP datagram");
	Require(received.mBytesTransferred == payload.size() && observed == source, "Datagram length/source changed");
	Require(std::equal(payload.begin(), payload.end(), buffer.begin()), "UDP payload changed");
	RequireError(receiver.ReceiveFrom(buffer, observed), NetworkError::WouldBlock, "Datagram boundary was not preserved");

	RequireSuccess(sender.SendTo({}, destination), "Send empty datagram");
	WaitReady(receiver, SocketEvents::Readable);
	received = receiver.ReceiveFrom(buffer, observed);
	RequireSuccess(received, "Receive empty datagram");
	Require(received.mBytesTransferred == 0, "Empty UDP datagram was treated as a disconnect");

	RequireSuccess(sender.SendTo(buffer, destination), "Send oversized-for-buffer datagram");
	WaitReady(receiver, SocketEvents::Readable);
	RequireError(receiver.ReceiveFrom(std::span(buffer).first(2), observed), NetworkError::MessageTooLarge, "Reject truncated datagram");
	RequireError(receiver.ReceiveFrom(buffer, observed), NetworkError::WouldBlock, "Oversized datagram was not consumed");
	RequireSuccess(sender.SendTo(payload, destination), "Send after truncation");
	WaitReady(receiver, SocketEvents::Readable);
	RequireSuccess(receiver.ReceiveFrom(buffer, observed), "Receive after truncation");

	Socket duplicate;
	RequireSuccess(duplicate.Open(family), "Open duplicate bind socket");
	RequireError(duplicate.Bind(destination), NetworkError::AddressInUse, "Duplicate bind should fail");
	const auto originalPort = source;
	Socket moved(std::move(sender));
	Require(!sender.IsOpen() && moved.IsOpen(), "Move construction did not transfer ownership");
	Socket assigned;
	RequireSuccess(assigned.Open(family), "Open move-assignment target");
	assigned = std::move(moved);
	Require(!moved.IsOpen() && assigned.IsOpen(), "Move assignment did not transfer ownership");
	RequireSuccess(assigned.GetLocalEndpoint(source), "Get moved socket endpoint");
	Require(source == originalPort, "Move changed socket identity");
	assigned.Close();
	assigned.Close();
	RequireSuccess(assigned.Open(family), "Reopen closed socket");
	RequireSuccess(assigned.Bind(originalPort), "Close did not release the UDP port");
	RequireSuccess(assigned.SendTo(payload, destination), "Other socket's close invalidated networking runtime");
	WaitReady(receiver, SocketEvents::Readable);
	RequireSuccess(receiver.ReceiveFrom(buffer, observed), "Receive after runtime-reference changes");
}

static void TransferStream(Socket& sender, Socket& receiver)
{
	std::vector<std::byte> payload(256 * 1024);
	for (std::size_t index = 0; index < payload.size(); ++index)
	{
		payload[index] = static_cast<std::byte>(index % 251);
	}
	std::vector<std::byte> received(payload.size());
	std::size_t sent = 0;
	std::size_t read = 0;
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (read < payload.size())
	{
		Require(std::chrono::steady_clock::now() < deadline, "TCP transfer stalled");
		bool progressed = false;
		if (sent < payload.size())
		{
			const auto result = sender.Send(std::span<const std::byte>(payload).subspan(sent));
			if (result)
			{
				sent += result.mBytesTransferred;
				progressed = result.mBytesTransferred != 0;
			}
			else
			{
				RequireError(result, NetworkError::WouldBlock, "TCP send under backpressure");
			}
		}
		const auto chunk = std::min<std::size_t>(511, received.size() - read);
		const auto result = receiver.Receive(std::span(received).subspan(read, chunk));
		if (result)
		{
			read += result.mBytesTransferred;
			progressed = progressed || result.mBytesTransferred != 0;
		}
		else
		{
			RequireError(result, NetworkError::WouldBlock, "TCP receive while transferring");
		}
		if (!progressed)
		{
			std::this_thread::sleep_for(1ms);
		}
	}
	Require(sent == payload.size() && received == payload, "TCP stream lost or reordered bytes");
}

static void CheckTCP(AddressFamily family)
{
	Socket listener;
	Socket client;
	Socket accepted;
	RequireSuccess(listener.Open(family, SocketType::TCP), "Open TCP listener");
	RequireSuccess(listener.Bind(Endpoint::Loopback(0, family)), "Bind TCP listener");
	RequireSuccess(listener.Listen(), "Listen TCP");
	RequireError(listener.Accept(accepted), NetworkError::WouldBlock, "Accept with no pending peer");
	Require(!accepted.IsOpen(), "Failed accept modified output socket");
	Endpoint destination;
	RequireSuccess(listener.GetLocalEndpoint(destination), "Get TCP listener endpoint");
	RequireSuccess(client.Open(family, SocketType::TCP), "Open TCP client");
	const auto connectResult = client.Connect(destination);
	if (connectResult.mError == NetworkError::InProgress)
	{
		RequireError(client.SetBlocking(true), NetworkError::InvalidArgument, "Reject mode change during connect");
		WaitReady(client, SocketEvents::Writable);
		RequireSuccess(client.FinishConnect(), "Finish TCP connection");
	}
	else
	{
		RequireSuccess(connectResult, "Connect TCP");
	}
	WaitReady(listener, SocketEvents::Readable);
	Endpoint peer;
	RequireSuccess(listener.Accept(accepted, &peer), "Accept TCP peer");
	Endpoint local;
	Endpoint remote;
	RequireSuccess(client.GetLocalEndpoint(local), "Get TCP client endpoint");
	RequireSuccess(client.GetRemoteEndpoint(remote), "Get TCP peer endpoint");
	Require(peer == local && remote == destination, "TCP peer endpoint mismatch");
	std::array<std::byte, 8> buffer{};
	RequireError(accepted.Receive(buffer), NetworkError::WouldBlock, "Accepted socket must inherit nonblocking mode");
	RequireError(accepted.Receive({}), NetworkError::InvalidArgument, "Reject empty stream receive buffer");
	RequireSuccess(client.Send({}), "Empty TCP send");
	RequireSuccess(client.SetNoDelay(true), "Enable TCP_NODELAY");
	RequireSuccess(client.SetBufferSizes(4096, 4096), "Set TCP buffers");
	RequireError(client.SendTo(buffer, destination), NetworkError::InvalidArgument, "Reject datagram send on TCP");
	RequireError(listener.Accept(client), NetworkError::InvalidArgument, "Accept cannot overwrite an owned socket");
	TransferStream(client, accepted);

	RequireSuccess(client.Shutdown(ShutdownMode::Send), "Half-close TCP send direction");
	WaitReady(accepted, SocketEvents::Readable);
	RequireError(accepted.Receive(buffer), NetworkError::ConnectionClosed, "Peer should receive orderly EOF");
	const std::array<std::byte, 1> reply{ std::byte{7} };
	RequireSuccess(accepted.Send(reply), "Send reply after peer half-close");
	WaitReady(client, SocketEvents::Readable);
	const auto result = client.Receive(buffer);
	RequireSuccess(result, "Receive reply after local half-close");
	Require(result.mBytesTransferred == 1 && buffer[0] == reply[0], "Half-close reply changed");
	accepted.Close();
	WaitReady(client, SocketEvents::Readable);
	RequireError(client.Receive(buffer), NetworkError::ConnectionClosed, "Peer close should report EOF");
}

static void CheckRefusedConnection()
{
	// Reserve a TCP port without listening, so another process cannot claim it mid-test.
	Socket reserved;
	Socket client;
	RequireSuccess(reserved.Open(AddressFamily::IPv4, SocketType::TCP), "Open refusal reservation");
	RequireSuccess(reserved.Bind(Endpoint::Loopback()), "Reserve unused TCP port");
	Endpoint endpoint;
	RequireSuccess(reserved.GetLocalEndpoint(endpoint), "Get reserved TCP port");
	RequireSuccess(client.Open(AddressFamily::IPv4, SocketType::TCP), "Open refused TCP client");
	auto result = client.Connect(endpoint);
	if (result.mError == NetworkError::InProgress)
	{
		// Windows may retry a refused local SYN before reporting its final error.
		SocketEvents ready;
		RequireSuccess(client.Wait(SocketEvents::Writable, 5s, ready), "Wait for refused connection result");
		result = client.FinishConnect();
	}
	RequireError(result, NetworkError::ConnectionRefused, "Failed connect must not appear successful after readiness");
	Require(result.mNativeError != 0, "Refused connection lost native diagnostics");
}

int main()
{
	try
	{
		// Deliberately never initializes the window/event backend.
		CheckAddresses();
		CheckInvalidOperations();
		std::cout << "Address parsing, resolution, and invalid-operation checks passed.\n";
		CheckUDP(AddressFamily::IPv4);
		std::cout << "IPv4 UDP checks passed.\n";
		CheckUDP(AddressFamily::IPv6);
		std::cout << "IPv6 UDP checks passed.\n";
		CheckTCP(AddressFamily::IPv4);
		std::cout << "IPv4 TCP checks passed.\n";
		CheckTCP(AddressFamily::IPv6);
		std::cout << "IPv6 TCP checks passed.\n";
		CheckRefusedConnection();

		std::vector<std::future<void>> workers;
		for (int index = 0; index < 4; ++index)
		{
			workers.push_back(std::async(std::launch::async, [] { CheckUDP(AddressFamily::IPv4); }));
		}
		for (auto& worker : workers)
		{
			worker.get();
		}
		CheckUDP(AddressFamily::IPv4);
		std::cout << "Networking checks passed: IPv4/IPv6 UDP/TCP, DNS, readiness, ownership, concurrency, and headless lifetime.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << "\n";
		return 1;
	}
}
