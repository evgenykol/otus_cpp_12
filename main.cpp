#include <iostream>
#include <deque> //check
#include <set>   //check

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "string.h"
#include "version.h"
#include "bulk.h"

using namespace std;
using boost::asio::ip::tcp;

using chat_message = std::string;

typedef std::deque<chat_message> chat_message_queue;

class bulk_participant
{
public:
    virtual ~bulk_participant() {}
    //virtual void deliver(const chat_message& msg) = 0;
};

typedef std::shared_ptr<bulk_participant> bulk_participant_ptr;

class bulk_room
{
public:
    bulk_room(bulk::BulkContext &bulk) : bulk_(bulk) {}

    void join(bulk_participant_ptr participant)
    {
        participants_.insert(participant);
        //for (auto msg: recent_msgs_)
            //participant->deliver(msg);
    }

    void leave(bulk_participant_ptr participant)
    {
        participants_.erase(participant);
        bulk_.end_input();
    }

    void deliver(const chat_message& msg)
    {
        recent_msgs_.push_back(msg);
        while (recent_msgs_.size() > max_recent_msgs)
            recent_msgs_.pop_front();

//        for (auto participant: participants_)
//            participant->deliver(msg);
    }

private:
    std::set<bulk_participant_ptr> participants_;
    enum { max_recent_msgs = 100 };
    chat_message_queue recent_msgs_;
    bulk::BulkContext &bulk_;
};

class bulk_session
        : public bulk_participant,
          public std::enable_shared_from_this<bulk_session>
{
public:
    bulk_session(tcp::socket socket, bulk_room& room)
        : socket_(std::move(socket)),
          room_(room)
    {
    }

    void start()
    {
        room_.join(shared_from_this());
        do_read_message();
    }

private:
    size_t read_complete(char * buf, const error_code & err, size_t bytes)
    {
        if ( err) return 0;
        bool found = std::find(buf, buf + bytes, '\n') < buf + bytes;
        return found ? 0 : 1;
    }

    void do_read_message()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(str),
                                boost::bind(&bulk_session::read_complete, this, str, _1, _2),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                cout << "do_read_message " << str << " id = " << self <<endl;
                do_read_message();
            }
            else
            {
                cout << "do_read_message ec = " << ec.message() << endl;
                room_.leave(shared_from_this());
            }
        });
    }


    tcp::socket socket_;
    char str[512];
    bulk_room& room_;
    //  chat_message read_msg_;
    //  chat_message_queue write_msgs_;
};

class bulk_server
{
public:
    bulk_server(boost::asio::io_service& io_service,
                const tcp::endpoint& endpoint,
                const int bulk_size_)
        : acceptor_(io_service, endpoint),
          socket_(io_service)
    {
        bulk_ = make_unique<bulk::BulkContext>(bulk_size_);
        room_ = make_unique<bulk_room>(*bulk_);
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(socket_,
                               [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                std::make_shared<bulk_session>(std::move(socket_), *room_)->start();
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    unique_ptr<bulk_room> room_;
    unique_ptr<bulk::BulkContext> bulk_;

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
        tcp::endpoint endpoint(tcp::v4(), port);

        bulk_server server(io_service, endpoint, bulk_size);
        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
