#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>

namespace solution
{
	class Controller
	{
	public:

		using protocol_t = boost::asio::ip::tcp;

		using endpoint_t = protocol_t::endpoint;

		using ip_address_t = boost::asio::ip::address;

		using socket_t = protocol_t::socket;

		using sockets_container_t = std::vector < socket_t > ;

	private:

		struct REQUEST
		{
			std::uint8_t  control_sum      = 0x00;
			std::uint8_t  protocol_version = 0x03;
			std::uint8_t  command_type     = 0x00;
			std::uint8_t  command_id       = 0x00;
			std::uint16_t data_length      = 0x00;
		};

		struct PASSWORD
		{
			std::uint8_t  control_sum      = 0x00;
			std::uint8_t  protocol_version = 0x03;
			std::uint8_t  command_type     = 0x00;
			std::uint8_t  command_id       = 0x00;
			std::uint16_t data_length      = 0x08;

			std::uint8_t  data[8] = {};
		};

		struct RESPONSE
		{
			std::uint8_t  control_sum      = 0x00;
			std::uint8_t  protocol_version = 0x03;
			std::uint8_t  command_type     = 0x01;
			std::uint8_t  command_id       = 0x00;
			std::uint16_t data_length      = 0x07;

			std::uint8_t  data[7] = {};
		};

		struct Command_MOVE
		{
			std::uint8_t  control_sum      = 0x00;
			std::uint8_t  protocol_version = 0x03;
			std::uint8_t  command_type     = 0x02;
			std::uint8_t  command_id       = 0x00;
			std::uint16_t data_length      = 0x04;

			std::uint8_t  data[4] = {};
		};

		struct SMSD_Command
		{
			std::uint32_t reserve : 3;
			std::uint32_t action  : 1;
			std::uint32_t command : 6;
			std::uint32_t data    : 22;
		};

		struct STATUS
		{
			std::uint8_t  control_sum      = 0x00;
			std::uint8_t  protocol_version = 0x03;
			std::uint8_t  command_type     = 0x02;
			std::uint8_t  command_id       = 0x00;
			std::uint16_t data_length      = 0x07;

			std::uint8_t  data[7] = {};
		};

	public:

		explicit Controller(const std::initializer_list < 
			std::pair < std::string, unsigned int > > & endpoints)
		{
			m_sockets.reserve(std::size(endpoints));

			for (const auto & [ip_address, port] : endpoints)
			{
				endpoint_t endpoint(ip_address_t::from_string(ip_address), port);

				socket_t socket(m_io_service, endpoint.protocol());

				socket.connect(endpoint);

				std::cout << "Socket on [" << ip_address << ":" << port << "] " <<
					"successfully created" << std::endl;

				receive(socket, sizeof(REQUEST));

				PASSWORD password;
				
				password.data[0] = 0xEF;
				password.data[1] = 0xCD;
				password.data[2] = 0xAB;
				password.data[3] = 0x89;
				password.data[4] = 0x67;
				password.data[5] = 0x45;
				password.data[6] = 0x23;
				password.data[7] = 0x01;

				send(socket, password);
				
				if (receive(socket, sizeof(RESPONSE) - 1) == ok_access)
				{
					std::cout << "Controller on [" << ip_address << ":" << 
						port << "] " << "ready to work" << std::endl;
				}
				else
				{
					std::cout << "Controller on [" << ip_address << ":" <<
						port << "] " << "failed to login" << std::endl;
				}

				m_sockets.push_back(std::move(socket));
			}
		}

		~Controller() noexcept = default;

	public:

		void move(char direction, std::uint32_t speed = 0)
		{
			SMSD_Command smsd_command_1;
			SMSD_Command smsd_command_2;

			smsd_command_1.reserve = 0;
			smsd_command_2.reserve = 0;

			smsd_command_1.action = 0;
			smsd_command_2.action = 0;

			smsd_command_1.data = std::min(std::max(speed, min_speed), max_speed);
			smsd_command_2.data = std::min(std::max(speed, min_speed), max_speed);

			switch (direction)
			{
			case 'f': case 'F':
			{
				smsd_command_1.command = 0x0E;
				smsd_command_2.command = 0x0F;

				break;
			}
			case 'b': case 'B':
			{
				smsd_command_1.command = 0x0F;
				smsd_command_2.command = 0x0E;

				break;
			}
			case 's': case 'S':
			{
				smsd_command_1.command = 0x0E;
				smsd_command_2.command = 0x0F;

				smsd_command_1.data = 0;
				smsd_command_2.data = 0;

				break;
			}
			default:
			{
				return;
			}
			}

			Command_MOVE command_move_1;
			Command_MOVE command_move_2;

			memcpy(command_move_1.data, &smsd_command_1, sizeof(smsd_command_1));
			memcpy(command_move_2.data, &smsd_command_2, sizeof(smsd_command_2));

			send(m_sockets[0], command_move_1);

			receive(m_sockets[0], sizeof(STATUS) - 1);

			send(m_sockets[1], command_move_2);

			receive(m_sockets[1], sizeof(STATUS) - 1);
		}

		void stop()
		{
			SMSD_Command smsd_command_1;
			SMSD_Command smsd_command_2;

			smsd_command_1.reserve = 0;
			smsd_command_2.reserve = 0;

			smsd_command_1.action = 0;
			smsd_command_2.action = 0;

			smsd_command_1.command = 0x22;
			smsd_command_2.command = 0x22;

			smsd_command_1.data = 0;
			smsd_command_2.data = 0;
			
			Command_MOVE command_move_1;
			Command_MOVE command_move_2;

			memcpy(command_move_1.data, &smsd_command_1, sizeof(smsd_command_1));
			memcpy(command_move_2.data, &smsd_command_2, sizeof(smsd_command_2));

			send(m_sockets[0], command_move_1);

			receive(m_sockets[0], sizeof(STATUS) - 1);

			send(m_sockets[1], command_move_2);

			receive(m_sockets[1], sizeof(STATUS) - 1);
		}

	private:

		std::uint8_t receive(socket_t & socket, std::size_t length)
		{
			std::uint8_t buffer[1024];

			boost::asio::read(socket, boost::asio::buffer(buffer, length));

			// print_data(buffer, length);

			if (length > 8)
			{
				return (buffer[8]);
			}
			else
			{
				return 0x00;
			}
		}

		void print_data(std::uint8_t * buffer, std::size_t length)
		{
			for (auto i = 0U; i < length; ++i)
			{
				auto c = buffer[i];

				std::uint8_t mask = 128;

				for (auto j = 0U; j < 8U; ++j)
				{
					std::cout << ((mask & c) ? "1" : "0");

					mask >>= 1;
				}

				std::cout << std::endl;
			}

			std::cout << std::endl;
		}

		template < typename T >
		void send(socket_t & socket, T & command)
		{
			update_control_sum(command);

			boost::asio::write(socket, boost::asio::buffer(&command, sizeof(command)));
		}

		template < typename T >
		void update_control_sum(T & command)
		{
			command.control_sum = get_control_sum(
				&command.control_sum, sizeof(command));
		}

		std::uint8_t get_control_sum(std::uint8_t * data, std::uint16_t length)
		{
			std::uint8_t s = 0xFF;

			while (length--) 
			{ 
				s += *(data++);
			}

			return (s ^ 0xFF);
		}

	public:

		const auto & sockets() const noexcept
		{
			return m_sockets;
		}

	private:

		static inline const std::uint8_t ok_access = 0x01;

		static inline const std::uint32_t min_speed = 15;
		static inline const std::uint32_t max_speed = 15600;

	private:

		boost::asio::io_service m_io_service;

		sockets_container_t m_sockets;

		std::uint8_t m_command_id = 0;
	};

} // namespace solution

using Controller = solution::Controller;

int main(int argc, char ** argv)
{
	system("chcp 1251");

	try
	{
		{
			Controller controller({
				{ "192.168.1.2", 5000U}, 
				{ "192.168.1.3", 5001U} });

			char command = ' ';

			while (command != 'e' && command != 'E')
			{
				std::cout << "Enter command (B - backward, S - stop, F - forward, E - exit) : ";

				std::cin >> command;

				switch (command)
				{
				case 'f': case 'F': case 'b': case 'B':
				{
					std::uint32_t speed = 0;

					std::cin >> speed;

					controller.move(command, speed);

					break;
				}
				case 's': case 'S':
				{
					controller.stop();

					break;
				}
				default:
				{
					break;
				}
				}
			}
		}

		system("pause");

		return EXIT_SUCCESS;
	}
	catch (const std::exception & exception)
	{
		std::cerr << exception.what() << std::endl;

		system("pause");

		return EXIT_FAILURE;
	}
	catch (...)
	{
		std::cerr << "unknown exception" << std::endl;

		system("pause");

		return EXIT_FAILURE;
	}
}