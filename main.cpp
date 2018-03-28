#include <iostream>
#include <list>

#include <boost/asio.hpp>

#include "string.h"
#include "version.h"

using namespace std;
using boost::asio::ip::tcp;

class chat_session
  : //public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket/*, chat_room& room*/)
    : socket_(std::move(socket))
      //room_(room)
  {
      cout << "chat_session socket: " << socket.native() << endl;
  }

//  void start()
//  {
//    room_.join(shared_from_this());
//    do_read_header();
//  }

//  void deliver(const chat_message& msg)
//  {
//    bool write_in_progress = !write_msgs_.empty();
//    write_msgs_.push_back(msg);
//    if (!write_in_progress)
//    {
//      do_write();
//    }
//  }

private:
//  void do_read_header()
//  {
//    auto self(shared_from_this());
//    boost::asio::async_read(socket_,
//        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
//        [this, self](boost::system::error_code ec, std::size_t /*length*/)
//        {
//          if (!ec && read_msg_.decode_header())
//          {
//            do_read_body();
//          }
//          else
//          {
//            room_.leave(shared_from_this());
//          }
//        });
//  }

//  void do_read_body()
//  {
//    auto self(shared_from_this());
//    boost::asio::async_read(socket_,
//        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
//        [this, self](boost::system::error_code ec, std::size_t /*length*/)
//        {
//          if (!ec)
//          {
//            room_.deliver(read_msg_);
//            do_read_header();
//          }
//          else
//          {
//            room_.leave(shared_from_this());
//          }
//        });
//  }

//  void do_write()
//  {
//    auto self(shared_from_this());
//    boost::asio::async_write(socket_,
//        boost::asio::buffer(write_msgs_.front().data(),
//          write_msgs_.front().length()),
//        [this, self](boost::system::error_code ec, std::size_t /*length*/)
//        {
//          if (!ec)
//          {
//            write_msgs_.pop_front();
//            if (!write_msgs_.empty())
//            {
//              do_write();
//            }
//          }
//          else
//          {
//            room_.leave(shared_from_this());
//          }
//        });
//  }

  tcp::socket socket_;
//  chat_room& room_;
//  chat_message read_msg_;
//  chat_message_queue write_msgs_;
};

class chat_server
{
public:
    chat_server(boost::asio::io_service& io_service,
                const tcp::endpoint& endpoint)
        : acceptor_(io_service, endpoint),
          socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        cout << "do_accept!" << endl;
        acceptor_.async_accept(socket_,
        [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                cout << "async_accept!" << endl;
                std::make_shared<chat_session>(std::move(socket_))/*, room_)->start()*/;
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    //chat_room room_;
};

int main(int argc, char* argv[])
{
    try
    {
        int bulk_size = 0;
        int port = 0;

        if ((argc > 1) &&
                (!strncmp(argv[1], "-v", 2) || !strncmp(argv[1], "--version", 9)))
        {
            cout << "version " << version() << endl;
            return 0;
        }
        else if (argc > 1)
        {
            port = atoi(argv[1]);
            bulk_size = atoi(argv[2]);
            cout << "bulk_server starting on port: " << port << ", bulk size: " << bulk_size << endl;
        }
        else
        {
            std::cerr << "Usage: bulk_server <port> <bulk_size>\n";
            return 1;
        }

        boost::asio::io_service io_service;

        std::list<chat_server> servers;
        //for (int i = 1; i < argc; ++i)
        {
            tcp::endpoint endpoint(tcp::v4(), port);
            servers.emplace_back(io_service, endpoint);
        }

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
