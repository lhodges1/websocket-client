#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/write.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <boost/json/stream_parser.hpp>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <future>
#include <ctime>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;
using json = nlohmann::json;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class Session {
    ssl::context _ctx{ ssl::context::sslv23_client };
    std::string _filename = "./cacert.pem";
    std::string _wsUrl = "/ws/";
    const char* _host;
    const char* _port;
    std::string _endpoint;
    tcp::resolver _resolver;
    websocket::stream<ssl::stream<tcp::socket>> _ws;
    std::string _symbol;
    boost::beast::multi_buffer _b;
    std::thread _socketThread;
    bool _gate;
    std::mutex _mtx;
    int _resetSocketTime;
    std::thread _reconnectThread;
public:
    Session(std::string symbol, net::io_context& ioc, const char* host, const char* port, std::string endpoint) :
        _resolver{ ioc },
        _ws{ ioc, _ctx }
    {
        _ctx.set_verify_mode(ssl::verify_none);
        _symbol = symbol;
        _host = host;
        _port = port;
        _endpoint = endpoint;
        _gate = true;
        handshake();
        async();
    }
    void handshake() {
        try
        {
            auto const results = _resolver.resolve(_host, _port);
            boost::asio::connect(_ws.next_layer().next_layer(), results.begin(), results.end());
            _ws.next_layer().handshake(ssl::stream_base::client);
            _endpoint = _wsUrl + _symbol + _endpoint;
            _ws.handshake(_host, _endpoint);
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            //log this exception
        }
    }
    void run() {
        while (_gate) {
            try {
                _ws.read(_b);
                std::cout << buffers_to_string(_b.data()) << std::endl;
                _b.consume(_b.size());
            }
            catch (const std::exception& e) {
                std::cout << e.what() << std::endl;
                //log exception stuff
            }
        }
        std::cout << "Thread finished running" << std::endl;
    }
    void async() {
        _socketThread = std::thread([this]() { run(); });
        _reconnectThread = std::thread([this]() {reconnect(); });
        _reconnectThread.detach();
    }
    void reconnect() {
        //sleep this thread for 23 hours (82800ms)
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (_gate = true) {
            std::cout << "reconnect thread running" << std::endl;
            close();
            writeGate(true);
            handshake();
            async();
        }
        std::cout << "reconnect thread finished" << std::endl;
    }
    void writeGate(bool newValue) {
        std::lock_guard<std::mutex> lock(_mtx);
        _gate = newValue;
    }
    void close() {
        try {
            _b.consume(_b.size());
            writeGate(false);
            _socketThread.join();
            std::cout << "thread joined" << std::endl;
            _ws.next_layer().lowest_layer().close();
            //_ws.close(websocket::close_code::normal);
            std::cout << "socket closed" << std::endl;
        }
        catch (std::exception& const e) {
            std::cout << e.what() << std::endl;
        }
    }
    std::string getSymbol() {
        return _symbol;
    }
};

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    net::io_context ioc;
    std::string symbol = "btcusdt";
    const char* host = "fstream.binance.com";
    const char* port = "443";
    std::string endpoint = "@markPrice@1s";

    auto socket = std::make_shared<Session>(symbol, ioc, host, port, endpoint);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::this_thread::sleep_for(std::chrono::seconds(10));
    //ioc.run();

    return 0;
}