#pragma once

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#define ASIO_NO_EXCEPTIONS
#define ASIO_NO_TYPEID

#include "Optimizer.h"
#include "asio.hpp"
#include "parsing.h"

#define SIMULATION_VECTOR_LEN (SIMULATION_COUNT * 128)
using asio::ip::tcp;
using asio::ip::udp;

int selectorCore(const vector<Player> &p,
                 const vector<vector<uint8_t>> &allLineups,
                 const vector<uint8_t> &corrPairs,
                 const vector<float> &corrCoeffs,
                 const array<float, 128> &projs,
                 const array<float, 128> &stdevs, int lineupsIndexStart,
                 int lineupsIndexEnd, // request data
                 lineup_set &bestset  // request data
);

vector<OptimizerLineup> generateLineupN(const vector<Player> &p,
                                        vector<string> &disallowedPlayers,
                                        OptimizerLineup currentPlayers,
                                        int budgetUsed, double &msTime);

enum { max_length = 1024 };

class server {
public:
  server(asio::io_service &io_service, short port, const vector<Player> &p,
         const unordered_map<string, uint8_t> playerIndices,
         const vector<uint8_t> &corrPairs, const vector<float> &corrCoeffs,
         const array<float, 128> &projs, const array<float, 128> &stdevs)
      : socket_(io_service, udp::endpoint(udp::v4(), port)), p(p),
        playerIndices(playerIndices), corrPairs(corrPairs),
        corrCoeffs(corrCoeffs), projs(projs), stdevs(stdevs) {
    do_receive();
  }

  void do_receive() {
    socket_.async_receive_from(
        asio::buffer(data_, max_length), sender_endpoint_,
        [this](std::error_code ec, std::size_t bytes_recvd) {
          if (!ec && bytes_recvd > 0) {
            do_send(bytes_recvd);
          } else {
            do_receive();
          }
        });
  }

  void do_send(std::size_t length) {
    cout << "Got request: " << endl;
    cout << data_ << endl;
    if (strncmp(data_, "select", 6) == 0) {
      // initialize sharedouput == output
      vector<vector<uint8_t>> allLineups =
          parseLineups("\\\\bunn\\Users\\andrewbunn\\Documents\\Visual Studio "
                       "2013\\Projects\\dfs\\progproblems\\sharedoutput.csv",
                       playerIndices);
      // read lineups file
      // actually process data here then return result
      int lineupsIndexStart;
      int lineupsIndexEnd; // request data
      int setLen;
      array<char, max_length> indicesArr;
      // std::string s((std::istreambuf_iterator<char>(&b)),
      // std::istreambuf_iterator<char>());

      sscanf(data_, "select %d %d %d: %s", &lineupsIndexStart, &lineupsIndexEnd,
             &setLen, &indicesArr[0]);
      // sscanf(s.c_str(), "%d %d %d: %s", &lineupsIndexStart, &lineupsIndexEnd,
      // &setLen, &indicesArr[0]);

      if (setLen > 0) {
        vector<uint8_t> lineup;
        char *indices = &indicesArr[0];
        for (int i = 0; i < setLen; i++) {
          int index;
          sscanf(indices, "%d", &index);
          lineup.push_back(index);
          indices = strchr(indices, ',') + 1;
        }
        bestset.set.push_back(lineup);
      }

      int resultIndex = selectorCore(p, allLineups, corrPairs, corrCoeffs,
                                     projs, stdevs, lineupsIndexStart,
                                     lineupsIndexEnd, // request data
                                     bestset          // request data
      );

      // convert bestset to string (just last set)
      // auto it = find(allLineups.begin(), allLineups.end(),
      // bestset.set[bestset.set.size() - 1]); int resultIndex =
      // distance(allLineups.begin(), it);

      sprintf(data_, "%d %f", resultIndex, bestset.ev);
      // undo last add
      bestset.set.resize(bestset.set.size() - 1);
      cout << "Response: " << endl;
      cout << data_ << endl;
    } else if (strncmp(data_, "optimize", 8) == 0) {
      int playersRemoveLen;
      array<char, max_length> indicesArr;

      sscanf(data_, "optimize %d: %s", &playersRemoveLen, &indicesArr[0]);

      vector<string> playersToRemove;
      char *indices = &indicesArr[0];
      for (int i = 0; i < playersRemoveLen; i++) {
        int index;
        sscanf(indices, "%d", &index);
        playersToRemove.push_back(p[index].name);
        indices = strchr(indices, ',') + 1;
      }

      double msTime = 0;
      vector<OptimizerLineup> lineups =
          generateLineupN(p, playersToRemove, OptimizerLineup(), 0, msTime);
      writeLineupsData("\\\\ANBUNN5\\Users\\andrewbunn\\dfs\\progproblems\\shar"
                       "edlineups.csv",
                       lineups);
      // just echo back to master to indicate file is ready
      cout << "Wrote lineups data" << endl;
    }

    socket_.async_send_to(asio::buffer(data_, length), sender_endpoint_,
                          [this](std::error_code /*ec*/,
                                 std::size_t /*bytes_sent*/) { do_receive(); });
  }

private:
  lineup_set bestset;
  ;
  const vector<Player> p;
  const unordered_map<string, uint8_t> playerIndices;
  const vector<uint8_t> corrPairs;
  const vector<float> corrCoeffs;
  const array<float, 128> projs;
  const array<float, 128> stdevs;
  udp::socket socket_;
  udp::endpoint sender_endpoint_;
  enum { max_length = 1024 };
  char data_[max_length];
};
