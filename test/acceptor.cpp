/*

Copyright (c) 2015, Arvid Norberg
All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "simulator/simulator.hpp"
#include <boost/bind.hpp>

using namespace sim::asio;
using namespace sim::chrono;
using sim::simulation;

char send_buffer[10000];
char recv_buffer[10000];
int num_received = 0;
int num_sent = 0;

void on_sent(boost::system::error_code const& ec, std::size_t bytes_transferred
	, ip::tcp::socket& sock)
{
	int millis = int(duration_cast<milliseconds>(high_resolution_clock::now()
		.time_since_epoch()).count());
	if (ec)
	{
		printf("[%4d] send error %s\n", millis, ec.message().c_str());
		return;
	}

	num_sent += bytes_transferred;

	printf("[%4d] sent %d bytes\n", millis, int(bytes_transferred));
	printf("closing\n");
	sock.close();
}

void on_receive(boost::system::error_code const& ec
	, std::size_t bytes_transferred, ip::tcp::socket& sock)
{
	int millis = int(duration_cast<milliseconds>(high_resolution_clock::now()
		.time_since_epoch()).count());
	if (ec)
	{
		printf("[%4d] receive error %s\n", millis, ec.message().c_str());
		return;
	}

	num_received += bytes_transferred;

	printf("[%4d] received %d bytes\n", millis, int(bytes_transferred));

	sock.async_read_some(sim::asio::mutable_buffers_1(recv_buffer, sizeof(recv_buffer))
		, boost::bind(&on_receive, _1, _2, boost::ref(sock)));
}

void incoming_connection(boost::system::error_code const& ec
	, ip::tcp::socket& sock, ip::tcp::endpoint const& ep)
{
	int millis = int(duration_cast<milliseconds>(high_resolution_clock::now()
		.time_since_epoch()).count());
	if (ec)
	{
		printf("[%4d] error while accepting connection: %s\n"
			, millis, ec.message().c_str());
		return;
	}

	boost::system::error_code err;
	ip::tcp::endpoint remote_endpoint = sock.remote_endpoint(err);
	if (ec) printf("[%4d] local_endpoint failed: %s\n", millis, ec.message().c_str());
	ip::tcp::endpoint local_endpoint = sock.local_endpoint(err);
	if (ec) printf("[%4d] remote_endpoint failed: %s\n", millis, ec.message().c_str());
	printf("[%4d] received incoming connection from: %s:%d. local endpoint: %s:%d\n"
		, millis, ep.address().to_string().c_str(), ep.port()
		, local_endpoint.address().to_string().c_str(), local_endpoint.port());
	assert(local_endpoint.port() == 1337);
	assert(local_endpoint.address().to_string() == "40.30.20.10");
	assert(remote_endpoint.port() != 0);
	assert(remote_endpoint.address().to_string() == "10.20.30.40");

	sock.async_read_some(sim::asio::mutable_buffers_1(recv_buffer, sizeof(recv_buffer))
		, boost::bind(&on_receive, _1, _2, boost::ref(sock)));
}

void on_connected(boost::system::error_code const& ec
	, ip::tcp::socket& sock)
{
	int millis = int(duration_cast<milliseconds>(high_resolution_clock::now()
		.time_since_epoch()).count());
	if (ec)
	{
		printf("[%4d] error while connecting: %s\n", millis, ec.message().c_str());
		return;
	}

	boost::system::error_code err;
	ip::tcp::endpoint remote_endpoint = sock.remote_endpoint(err);
	if (ec) printf("[%4d] remote_endpoint failed: %s\n", millis, ec.message().c_str());
	ip::tcp::endpoint local_endpoint = sock.local_endpoint(err);
	if (ec) printf("[%4d] local_endpoint failed: %s\n", millis, ec.message().c_str());
	printf("[%4d] made outgoing connection to: %s:%d. local endpoint: %s:%d\n"
		, millis
		, remote_endpoint.address().to_string().c_str(), remote_endpoint.port()
		, local_endpoint.address().to_string().c_str(), local_endpoint.port());

	assert(remote_endpoint.port() == 1337);
	assert(remote_endpoint.address().to_string() == "40.30.20.10");
	assert(local_endpoint.port() != 0);
	assert(local_endpoint.address().to_string() == "10.20.30.40");

	printf("sending %d bytes\n", int(sizeof(send_buffer)));
	sock.async_write_some(sim::asio::const_buffers_1(send_buffer, sizeof(send_buffer))
		, boost::bind(&on_sent, _1, _2, boost::ref(sock)));
}

int main()
{
	simulation sim;
	io_service incoming_ios(sim, ip::address_v4::from_string("40.30.20.10"));
	io_service outgoing_ios(sim, ip::address_v4::from_string("10.20.30.40"));
	ip::tcp::acceptor listener(incoming_ios);

	int millis = int(duration_cast<milliseconds>(high_resolution_clock::now()
		.time_since_epoch()).count());

	boost::system::error_code ec;
	listener.open(ip::tcp::v4(), ec);
	if (ec) printf("[%4d] open failed: %s\n", millis, ec.message().c_str());
	listener.bind(ip::tcp::endpoint(ip::address(), 1337), ec);
	if (ec) printf("[%4d] bind failed: %s\n", millis, ec.message().c_str());
	listener.listen(10, ec);
	if (ec) printf("[%4d] listen failed: %s\n", millis, ec.message().c_str());

	ip::tcp::socket incoming(incoming_ios);
	ip::tcp::endpoint remote_endpoint;
	listener.async_accept(incoming, remote_endpoint
		, boost::bind(&incoming_connection, _1, boost::ref(incoming)
		, boost::cref(remote_endpoint)));

	printf("[%4d] connecting\n", millis);
	ip::tcp::socket outgoing(outgoing_ios);
	outgoing.open(ip::tcp::v4(), ec);
	if (ec) printf("[%4d] open failed: %s\n", millis, ec.message().c_str());
	outgoing.async_connect(ip::tcp::endpoint(ip::address::from_string("40.30.20.10")
		, 1337), boost::bind(&on_connected, _1, boost::ref(outgoing)));

	sim.run(ec);

	millis = int(duration_cast<milliseconds>(high_resolution_clock::now()
		.time_since_epoch()).count());

	assert(num_received == sizeof(send_buffer));
	assert(num_sent == sizeof(send_buffer));

	printf("[%4d] simulation::run() returned: %s\n"
		, millis, ec.message().c_str());
}

