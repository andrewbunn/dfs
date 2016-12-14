#pragma once

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include "asio.hpp"
#include "Players.h"
#include "parsing.h"

#define SIMULATION_VECTOR_LEN (SIMULATION_COUNT * 128)
using asio::ip::tcp;
using asio::ip::udp;


int selectorCore(
    const vector<Player>& p,
    const vector<vector<uint8_t>>& allLineups,
    int corrIdx,
    const array<float, SIMULATION_VECTOR_LEN>& projs, const array<float, SIMULATION_VECTOR_LEN>& stdevs,
    int lineupsIndexStart, int lineupsIndexEnd, // request data
    lineup_set& bestset    // request data
);

vector<Players2> generateLineupN(const vector<Player>& p, vector<string>& disallowedPlayers, Players2 currentPlayers, int budgetUsed, double& msTime);

enum { max_length = 1024 };
//class session
//    : public std::enable_shared_from_this<session>
//{
//public:
//    session(tcp::socket socket)
//        : socket_(std::move(socket))
//    {
//    }
//
//    void start(
//        const vector<Player>& p,
//        const vector<vector<uint8_t>>& allLineups,
//        const int corrIdx,
//        const array<float, SIMULATION_VECTOR_LEN>& projs, const array<float, SIMULATION_VECTOR_LEN>& stdevs
//        )
//    {
//        do_read(
//            p,
//            allLineups,
//            corrIdx,
//            projs,
//            stdevs);
//    }
//
//private:
//    void do_read(
//        const vector<Player>& p,
//        const vector<vector<uint8_t>>& allLineups,
//        const int corrIdx,
//        const array<float, SIMULATION_VECTOR_LEN>& projs, const array<float, SIMULATION_VECTOR_LEN>& stdevs
//    )
//    {
//        auto self(shared_from_this());
//
//        //socket_.async_receive(asio::buffer(data_, max_length),
//        async_read_until(socket_, b, '\0',
//            [&, this, self](std::error_code ec, std::size_t length)
//        {
//            if (!ec)
//            {
//                // here's where we process request
//                do_write(
//                    p,
//                    allLineups,
//                    corrIdx,
//                    projs,
//                    stdevs,
//                    length);
//            }
//        });
//    }
//
//    void do_write(
//        const vector<Player>& p,
//        const vector<vector<uint8_t>>& allLineups,
//        const int corrIdx,
//        const array<float, SIMULATION_VECTOR_LEN>& projs, const array<float, SIMULATION_VECTOR_LEN>& stdevs,
//        std::size_t length)
//    {
//        auto self(shared_from_this());
//        // actually process data here then return result
//        int lineupsIndexStart;
//        int lineupsIndexEnd; // request data
//        int setLen;
//        array<char, max_length> indicesArr;
//        std::string s((std::istreambuf_iterator<char>(&b)), std::istreambuf_iterator<char>());
//
//        //sscanf(data_, "%d %d %d: %s", &lineupsIndexStart, &lineupsIndexEnd, &setLen, &indicesArr[0]);
//        sscanf(s.c_str(), "%d %d %d: %s", &lineupsIndexStart, &lineupsIndexEnd, &setLen, &indicesArr[0]);
//        lineup_set bestset;
//
//        char* indices = &indicesArr[0];
//        for (int i = 0; i < setLen; i++)
//        {
//            int index;
//            sscanf(indices, "%d", &index);
//            bestset.set.push_back(allLineups[index]);
//            indices = strchr(indices, ',') + 1;
//        }
//
//        int resultIndex = selectorCore(
//            p,
//            allLineups,
//            corrIdx,
//            projs,
//            stdevs,
//            lineupsIndexStart, 
//            lineupsIndexEnd, // request data
//            bestset    // request data
//        );
//
//        // convert bestset to string (just last set)
//        //auto it = find(allLineups.begin(), allLineups.end(), bestset.set[bestset.set.size() - 1]);
//        //int resultIndex = distance(allLineups.begin(), it);
//
//        sprintf(data_, "%d %f", resultIndex, bestset.ev);
//
//        //socket_.async_send(/* this should be result*/asio::buffer(data_, max_length),
//        //    [&, this, self](std::error_code ec, std::size_t /*length*/)
//
//        async_write(socket_,/* this should be result*/asio::buffer(data_, max_length),
//            [&, this, self](std::error_code ec, std::size_t /*length*/)
//        {
//            if (!ec)
//            {
//                do_read(
//                    p,
//                    allLineups,
//                    corrIdx,
//                    projs,
//                    stdevs);
//            }
//        });
//    }
//
//    asio::streambuf b;
//    tcp::socket socket_;
//    char data_[max_length];
//};


class server
{
public:
    server(asio::io_service& io_service, short port,
        const vector<Player>& p,
        const unordered_map<string, uint8_t> playerIndices,
        const int corrIdx,
        const array<float, SIMULATION_VECTOR_LEN>& projs, const array<float, SIMULATION_VECTOR_LEN>& stdevs)
        : socket_(io_service, udp::endpoint(udp::v4(), port)),
        p(p),
        playerIndices(playerIndices),
        corrIdx(corrIdx),
        projs(projs),
        stdevs(stdevs)
    {
        do_receive();
    }

    void do_receive()
    {
        socket_.async_receive_from(
            asio::buffer(data_, max_length), sender_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd)
        {
            if (!ec && bytes_recvd > 0)
            {
                do_send(bytes_recvd);
            }
            else
            {
                do_receive();
            }
        });
    }

    void do_send(std::size_t length)
    {
        cout << "Got request: " << endl;
        cout << data_ << endl;
        if (strncmp(data_, "select", 6) == 0)
        {
            // initialize sharedouput == output
            vector<vector<uint8_t>> allLineups = parseLineups("\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio 2013\\Projects\\dfs\\progproblems\\sharedoutput.csv", playerIndices);
            // read lineups file
            // actually process data here then return result
            int lineupsIndexStart;
            int lineupsIndexEnd; // request data
            int setLen;
            array<char, max_length> indicesArr;
            //std::string s((std::istreambuf_iterator<char>(&b)), std::istreambuf_iterator<char>());

            sscanf(data_, "select %d %d %d: %s", &lineupsIndexStart, &lineupsIndexEnd, &setLen, &indicesArr[0]);
            //sscanf(s.c_str(), "%d %d %d: %s", &lineupsIndexStart, &lineupsIndexEnd, &setLen, &indicesArr[0]);
            lineup_set bestset;

            char* indices = &indicesArr[0];
            for (int i = 0; i < setLen; i++)
            {
                int index;
                sscanf(indices, "%d", &index);
                bestset.set.push_back(allLineups[index]);
                indices = strchr(indices, ',') + 1;
            }

            int resultIndex = selectorCore(
                p,
                allLineups,
                corrIdx,
                projs,
                stdevs,
                lineupsIndexStart,
                lineupsIndexEnd, // request data
                bestset    // request data
            );

            // convert bestset to string (just last set)
            //auto it = find(allLineups.begin(), allLineups.end(), bestset.set[bestset.set.size() - 1]);
            //int resultIndex = distance(allLineups.begin(), it);

            sprintf(data_, "%d %f", resultIndex, bestset.ev);
            cout << "Response: " << endl;
            cout << data_ << endl;
        }
        else if (strncmp(data_, "optimize", 8) == 0)
        {
            int playersRemoveLen;
            array<char, max_length> indicesArr;

            sscanf(data_, "optimize %d: %s", &playersRemoveLen, &indicesArr[0]);

            vector<string> playersToRemove;
            char* indices = &indicesArr[0];
            for (int i = 0; i < playersRemoveLen; i++)
            {
                int index;
                sscanf(indices, "%d", &index);
                playersToRemove.push_back(p[index].name);
                indices = strchr(indices, ',') + 1;
            }
            
            double msTime = 0;
            vector<Players2> lineups = generateLineupN(p, playersToRemove, Players2(), 0, msTime);
            writeLineupsData("\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio 2013\\Projects\\dfs\\progproblems\\sharedlineups.csv", lineups);
            // just echo back to master to indicate file is ready
        }

        socket_.async_send_to(
            asio::buffer(data_, length), sender_endpoint_,
            [this](std::error_code /*ec*/, std::size_t /*bytes_sent*/)
        {
            do_receive();
        });
    }

private:
    const vector<Player> p;
    const unordered_map<string, uint8_t> playerIndices;
    const int corrIdx;
    const array<float, SIMULATION_VECTOR_LEN> projs;
    const array<float, SIMULATION_VECTOR_LEN> stdevs;
    udp::socket socket_;
    udp::endpoint sender_endpoint_;
    enum { max_length = 1024 };
    char data_[max_length];
};
