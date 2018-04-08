#include <iostream>
#include <map>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "string.h"
#include "version.h"
#include "bulk.h"

using namespace std;
using boost::asio::ip::tcp;

class bulk_participant
{
public:
    virtual ~bulk_participant() {}
};

typedef std::shared_ptr<bulk_participant> bulk_participant_ptr;

class bulk_room
{
public:
    void set_bulk_size(size_t bulk_size)
    {
        bulk_.set_bulk_size(bulk_size);
    }

    void join(bulk_participant_ptr participant)
    {
        participants_.emplace(std::make_pair(participant, bulk::BulkSessionProcessor()));
    }

    void leave(bulk_participant_ptr participant)
    {
        participants_.erase(participant);
        if (participants_.empty())
        {
            if(commands.cmds.metrics.commands > 0)
            {
                unique_lock<mutex> lk(m);
                bulk_.dump_block(commands);
            }
        }
    }

    void deliver(const bulk_participant_ptr participant, std::string& msg)
    {
        unique_lock<mutex> lk(m);
        bulk_.add_line(msg, participants_[participant], commands);
    }

private:
    std::map<bulk_participant_ptr, bulk::BulkSessionProcessor> participants_;
    bulk::BulkSessionProcessor commands;
    bulk::BulkContext bulk_;
    std::mutex m;
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

    size_t read_complete(char * buf, const error_code & err, size_t bytes)
    {
        if ( err) return 0;
        bool found = std::find(buf, buf + bytes, '\n') < buf + bytes;
        buf[bytes] = '\0';
        return found ? 0 : 1;
    }

    void do_read_message()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(str),
                                //boost::bind(&bulk_session::read_complete, this, str, _1, _2),
                                [this](const boost::system::error_code & err, size_t bytes)->size_t
        {
            if ( err) return 0;
            bool found = std::find(str, str + bytes, '\n') < str + bytes;
            str[bytes] = '\0';
            return found ? 0 : 1;
        },
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                std::string s = string{str};
                s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
                room_.deliver(self, s);

                do_read_message();
            }
            else
            {
                room_.leave(shared_from_this());
            }
        });
    }

    tcp::socket socket_;
    bulk_room &room_;
    char str[512];
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
        room_.set_bulk_size(bulk_size_);
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
                std::make_shared<bulk_session>(std::move(socket_), room_)->start();
            }
            else
            {
                cout << "accept error " << ec.message() << endl;
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    bulk_room room_;
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
        else if (argc == 3)
        {
            port = atoi(argv[1]);
            bulk_size = atoi(argv[2]);
            //cout << "bulk_server starting on port: " << port << ", bulk size: " << bulk_size << endl;
        }
        else
        {
            std::cerr << "Usage: bulk_server <port> <bulk_size>\n";
            return 1;
        }

        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), port);

        bulk_server server(io_service, endpoint, bulk_size);

        vector<thread> thread_pool;
        for(int i = 0; i < 3; ++i)
        {
            thread_pool.emplace_back(thread([&] {io_service.run();}));
        }

        for (auto &thr : thread_pool)
        {
            thr.join();
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
