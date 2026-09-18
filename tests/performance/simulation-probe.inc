// Included only by run-simulation-probe.py, never by the shipped game.
#include <Game.h>
#include <GameInitSettings.h>
#include <Network/GameStateDigest.h>
#include <players/AIDecisionLog.h>
#include <mod/ModManager.h>
#include <chrono>
#include <memory>
#include <algorithm>

int runSimulationProbe() {
    ModManager::instance().setActiveMod("dunecity");
    auto game = std::make_unique<Game>();
    currentGame = game.get();
    game->bReplay = true; // Suppress public match analytics and interactive briefings.
    game->initGame(GameInitSettings(std::string(std::getenv("SIM_PROBE_SAVE"))));
    game->bReplay = false;
    game->initializeGameLoop();
    game->bPause=false;
    const int cycles = std::stoi(std::getenv("SIM_PROBE_CYCLES"));
    if (cycles <= 0) throw std::runtime_error("Cycle count must be positive");
    std::vector<double> times;
    using Clock = std::chrono::steady_clock;
    SDL_Log("SIM_PROBE_STATE: %s", GameStateDigest::describe(game->computeStateDigest()).c_str());
    for (int i=0; i<cycles; ++i) {
        const auto begin = Clock::now();
        const auto cycle=game->getGameCycleCount();
        game->updateGameState();
        if (game->getGameCycleCount()!=cycle+1) throw std::runtime_error("Simulation did not advance");
        times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-begin).count());
        AITelemetry::log().flushPerformance(game->getGameCycleCount());
        if ((i+1)%100 == 0)
            SDL_Log("SIM_PROBE_STATE: %s", GameStateDigest::describe(game->computeStateDigest()).c_str());
    }
    if (!game->saveGame(std::getenv("SIM_PROBE_OUTPUT"))) throw std::runtime_error("Save failed");
    std::sort(times.begin(),times.end());
    double total=0; for (double t:times) total+=t;
    SDL_Log("SIM_PROBE_PASS: cycles=%d total_ms=%.3f mean_ms=%.3f p99_ms=%.3f max_ms=%.3f",cycles,total,total/cycles,times[(times.size()-1)*99/100],times.back());
    game.reset(); currentGame=nullptr; pLocalPlayer=nullptr;
    return MENU_QUIT_DEFAULT;
}
