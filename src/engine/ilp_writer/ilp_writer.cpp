#include "ilp_writer.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <netdb.h>
#include <cstring>

namespace trading
{

    ILPWriter::ILPWriter(const ILPConfig &config)
        : config_(config), running_(false), socket_fd_(-1)
    {
        std::cout << "ILPWriter created for " << config_.questdb_host << ":" << config_.questdb_port << std::endl;
    }

    ILPWriter::~ILPWriter()
    {
        stop();
    }

    bool ILPWriter::start()
    {   
        //So what is exactly happening in the start function as far as I understand is that 1st we assign a variable socket file disputer where we initialize the socket, inside this variable. Now, if it is successful in initialization, then, It returns a positive integer, but if it returns a negative integer, it basically prints that we fail to create a socket and then it returns a false return type in the if statement. Now, we define a server address variable. And then inside it's member variables, we assign the socket internet version protocol, and socket internet port.Now, if our host is a local host. Then we assign the socket address inside the server address variable to be the local host address. But if the Host is not local host, then we extract the host name from the config variable, and then we mention the protocol version, and then we use the inet_pton function to retrieve the socket internet address, which is a binary value of IPV4 and store it in the socket internet address. Now if this the conversion is successful, and if the storage is successful, then it would return a positive integer, but if it returns something less than 0, then, We define a pointer host entry, Which gets the socket address of host by gethostbyname by DNS lookup through our config_.questdb_host, and if it is successful, then we use the memory copy function to store it inside socket internet address member variable of the server address variable. And if it is not successful, and if the host entry variable returns a null pointer, then we print invalid address or host name. Now, Finally, when we have got all server address variable information, And We have got our socket file disputer variable up and running. Then we basically try to connect the socket on the server address And if it is successful, then, we set the running variable to true, and print out ILP writer connected to quest DB, or we print fail to connect to quest DB.

        if (running_.load())
        {
            return false;
        }

        // Create socket connection to QuestDB
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0)
        {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET; // protocol version, IPv4, etc.
        server_addr.sin_port = htons(config_.questdb_port);

        // Handle hostname resolution (e.g., "localhost" -> "127.0.0.1")
        if (config_.questdb_host == "localhost")
        {
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        }
        else if (inet_pton(AF_INET, config_.questdb_host.c_str(), &server_addr.sin_addr) <= 0)
        {
            // Try hostname resolution
            struct hostent* host_entry = gethostbyname(config_.questdb_host.c_str());
            if (host_entry == nullptr)
            {
                std::cerr << "Invalid address/hostname: " << config_.questdb_host << std::endl;
                close(socket_fd_);
                return false;
            }

            memcpy(&server_addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
        }

        if (connect(socket_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cerr << "Failed to connect to QuestDB at " << config_.questdb_host
                      << ":" << config_.questdb_port << std::endl;
            close(socket_fd_);
            return false;
        }

        running_.store(true);
        std::cout << "ILP Writer connected to QuestDB" << std::endl;
        return true;
    }

    void ILPWriter::stop()
    {
        if (!running_.load())
        {
            return;
        }

        // Flush any remaining data
        flush();

        if (socket_fd_ >= 0)
        {
            close(socket_fd_);
            socket_fd_ = -1;
        }

        running_.store(false);
        std::cout << "ILP Writer stopped" << std::endl;
    }

    bool ILPWriter::is_running() const
    {
        return running_.load();
    }

    void ILPWriter::write_tick(const TickData &tick)
    {
        if (!running_.load())
        {
            return;
        }

        // Convert tick to ILP format and add to batch
        std::string ilp_line = tick.to_ilp_format();
        current_batch_.push_back(ilp_line);
 
        // Send batch if it's full
        if (current_batch_.size() >= config_.batch_size)
        {
            flush();
        }
    }

    void ILPWriter::flush()
    {
        if (current_batch_.empty() || !running_.load())
        {
            return;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        bool success = send_batch(current_batch_);
        if (success)
        {
            stats_.lines_sent += current_batch_.size();
            stats_.batches_sent++;
        }
        else
        {
            stats_.errors++;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        std::cout << "Sent batch of " << current_batch_.size() << " ticks in "
                  << duration.count() << " microseconds" << std::endl;

        current_batch_.clear();
    }

    bool ILPWriter::send_batch(const std::vector<std::string> &lines)
    {
        if (socket_fd_ < 0)
        {
            return false;
        }

        std::string batch_data;
        for (const auto &line : lines)
        {
            batch_data += line + "\n";
        }

        ssize_t sent = send(socket_fd_, batch_data.c_str(), batch_data.length(), 0);
        if (sent < 0)
        {
            std::cerr << "Failed to send data to QuestDB" << std::endl;
            return false;
        }

        return true;
    }

} // namespace trading