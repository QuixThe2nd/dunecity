// Included only by run-simulation-probe.py, never by the shipped game.
#include <Game.h>
#include <GameInitSettings.h>
#include <Network/GameStateDigest.h>
#include <players/AIDecisionLog.h>
#include <mod/ModManager.h>
#include <chrono>
#include <memory>
#include <algorithm>
#include <filesystem>

int runSimulationProbe() {
    if (!settings.general.diagnosticLogs)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SIM_PROBE_ERROR_REPORTING_CHECK: diagnostics disabled, errors retained");
    ModManager::instance().setActiveMod("dunecity");
    auto game = std::make_unique<Game>();
    currentGame = game.get();
    game->bReplay = true; // Suppress public match analytics and interactive briefings.
    game->initGame(GameInitSettings(std::string(std::getenv("SIM_PROBE_SAVE"))));
    game->bReplay = false;
    if (const char* duration=std::getenv("SIM_PROBE_RENDER_SECONDS")) {
        const int seconds=std::stoi(duration);
        if (seconds<=0) throw std::runtime_error("Render duration must be positive");
        const auto title="DuneCity - lag reproduction: "+std::filesystem::path(std::getenv("SIM_PROBE_SAVE")).stem().string();
        SDL_SetWindowTitle(window,title.c_str());
        game->bPause=false;
        std::fprintf(stderr,"SIM_PROBE_RENDER_BEGIN: save=%s seconds=%d tick_ms=%d start_cycle=%u\n",
            std::getenv("SIM_PROBE_SAVE"),seconds,game->getGameSpeed(),game->getGameCycleCount());
        const SDL_TimerID timer=SDL_AddTimer(seconds*1000,[](Uint32,void*) -> Uint32 {
            SDL_Event quit{};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return 0;
        },nullptr);
        if (!timer) throw std::runtime_error("Cannot schedule probe exit");
        game->runMainLoop(); // Identical input, presentation, pacing and simulation loop.
        SDL_RemoveTimer(timer);
        std::fprintf(stderr,"SIM_PROBE_PASS: rendered=1 end_cycle=%u\n",game->getGameCycleCount());
        game.reset();currentGame=nullptr;pLocalPlayer=nullptr;
        return MENU_QUIT_DEFAULT;
    }
    game->initializeGameLoop();
    game->bPause=false;
    const int cycles = std::stoi(std::getenv("SIM_PROBE_CYCLES"));
    if (cycles <= 0) throw std::runtime_error("Cycle count must be positive");
    std::vector<double> times;
    using Clock = std::chrono::steady_clock;
    std::fprintf(stderr,"SIM_PROBE_STATE: %s\n", GameStateDigest::describe(game->computeStateDigest()).c_str());
    for (int i=0; i<cycles; ++i) {
        const auto begin = Clock::now();
        const auto cycle=game->getGameCycleCount();
        game->updateGameState();
        if (game->getGameCycleCount()!=cycle+1) throw std::runtime_error("Simulation did not advance");
        times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-begin).count());
        AITelemetry::log().flushPerformance(game->getGameCycleCount());
        if ((i+1)%100 == 0)
            std::fprintf(stderr,"SIM_PROBE_STATE: %s\n", GameStateDigest::describe(game->computeStateDigest()).c_str());
    }
    if (!game->saveGame(std::getenv("SIM_PROBE_OUTPUT"))) throw std::runtime_error("Save failed");
    std::sort(times.begin(),times.end());
    double total=0; for (double t:times) total+=t;
    std::fprintf(stderr,"SIM_PROBE_PASS: cycles=%d total_ms=%.3f mean_ms=%.3f p99_ms=%.3f max_ms=%.3f\n",cycles,total,total/cycles,times[(times.size()-1)*99/100],times.back());
    game.reset(); currentGame=nullptr; pLocalPlayer=nullptr;
    return MENU_QUIT_DEFAULT;
}
