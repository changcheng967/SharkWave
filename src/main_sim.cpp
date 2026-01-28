#include "simulation.h"
#include <iostream>
#include <string>

using namespace sharkwave;

int main(int argc, char* argv[]) {
    int numHands = 1000;
    OpponentType opponent = OpponentType::Random;

    // Parse command line args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--hands" && i + 1 < argc) {
            numHands = std::stoi(argv[++i]);
        } else if (arg == "--opponent" && i + 1 < argc) {
            std::string opp = argv[++i];
            if (opp == "random") opponent = OpponentType::Random;
            else if (opp == "tight") opponent = OpponentType::TightPassive;
            else if (opp == "lag") opponent = OpponentType::LooseAggressive;
            else if (opp == "station") opponent = OpponentType::CallingStation;
        } else if (arg == "--help") {
            std::cout << "SharkWave Simulation\n\n";
            std::cout << "Usage: sharkwave_sim [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --hands N     Number of hands to simulate (default: 1000)\n";
            std::cout << "  --opponent T  Opponent type: random, tight, lag, station (default: random)\n";
            std::cout << "  --help        Show this help\n";
            return 0;
        }
    }

    Simulation sim(numHands, opponent);
    sim.run();
    sim.printResults();

    return 0;
}
