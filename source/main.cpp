/* main.cpp
Copyright (c) 2014 by Michael Zahniser

Main function for Endless Sky, a space exploration and combat RPG.

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Audio.h"
#include "Command.h"
#include "Conversation.h"
#include "ConversationPanel.h"
#include "DataFile.h"
#include "DataNode.h"
#include "Dialog.h"
#include "Font.h"
#include "FrameTimer.h"
#include "GameData.h"
#include "GameWindow.h"
#include "MenuPanel.h"
#include "Panel.h"
#include "PlayerInfo.h"
#include "Preferences.h"
#include "Screen.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "UI.h"

#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#    include <windows.h>
#endif

#ifdef __EMSCRIPTEN__
#    include <emscripten.h>
#endif

using namespace std;

void PrintHelp();
void PrintVersion();
void GameLoop(PlayerInfo& player, Conversation& conversation, bool& debugMode);
Conversation LoadConversation();
#ifdef _WIN32
void InitConsole();
#endif

// Entry point for the EndlessSky executable
int main(int argc, char* argv[])
{
#ifdef __EMSCRIPTEN__
    EM_ASM(FS.mkdir('/saves');
           FS.mount(IDBFS, {}, '/saves');

           // sync from persisted state into memory and then
           // run the 'test' function
           FS.syncfs(
               true, function(err) {
                   assert(!err);
                   const contents = FS.lookupPath('saves').node.contents;
                   const numFiles = Object.keys(contents).length;
                   console.log(
                       numFiles ? numFiles : "No",
                       "save files found in IndexedDB.");
               }););
#endif
    // Handle command-line arguments
#ifdef _WIN32
    if (argc > 1)
        InitConsole();
#endif
    Conversation conversation;
    bool debugMode = false;
    bool loadOnly = false;
    for (const char* const* it = argv + 1; *it; ++it)
    {
        string arg = *it;
        if (arg == "-h" || arg == "--help")
        {
            PrintHelp();
            return 0;
        }
        else if (arg == "-v" || arg == "--version")
        {
            PrintVersion();
            return 0;
        }
        else if (arg == "-t" || arg == "--talk")
            conversation = LoadConversation();
        else if (arg == "-d" || arg == "--debug")
            debugMode = true;
        else if (arg == "-p" || arg == "--parse-save")
            loadOnly = true;
    }

    // Begin loading the game data. Exit early if we are not using the UI.
    if (!GameData::BeginLoad(argv))
        return 0;

    // Load player data, including reference-checking.
    PlayerInfo player;
    bool checkedReferences = player.LoadRecent();
    if (loadOnly)
    {
        if (!checkedReferences)
            GameData::CheckReferences();
        cout << "Parse completed." << endl;
        return 0;
    }

    // On Windows, make sure that the sleep timer has at least 1 ms resolution
    // to avoid irregular frame rates.
#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    Preferences::Load();

    if (!GameWindow::Init())
        return 1;

    GameData::LoadShaders();

    // Show something other than a blank window.
    GameWindow::Step();

    Audio::Init(GameData::Sources());

    // This is the main loop where all the action begins.
    try
    {
        GameLoop(player, conversation, debugMode);
    }
    catch (const runtime_error& error)
    {
        GameWindow::ExitWithError(error.what());
        return 1;
    }

    // Remember the window state.
    Preferences::Set("maximized", GameWindow::IsMaximized());
    Preferences::Set("fullscreen", GameWindow::IsFullscreen());
    Screen::SetRaw(GameWindow::Width(), GameWindow::Height());
    Preferences::Save();

    GameWindow::Quit();
    Audio::Quit();

    return 0;
}

UI gamePanels;
UI menuPanels;

struct context
{
    context()
        : timer(frameRate)
    {
    }
    bool showCursor = true;
    int cursorTime = 0;
    int frameRate = 60;
    FrameTimer timer;
    bool isPaused = false;
    bool isFastForward = false;

    // If fast forwarding, keep track of whether the current frame should be
    // drawn.
    int skipFrame = 0;

    // Limit how quickly full-screen mode can be toggled.
    int toggleTimeout = 0;
    UI* gamePanels;
    UI* menuPanels;
    bool debugMode;
    PlayerInfo* player;
} c;

void GameLoop(PlayerInfo& player, Conversation& conversation, bool& debugMode)
{
    // gamePanels is used for the main panel where you fly your spaceship.
    // All other game content related dialogs are placed on top of the gamePanels.
    // If there are both menuPanels and gamePanels, then the menuPanels take
    // priority over the gamePanels. The gamePanels will not be shown until
    // the stack of menuPanels is empty.

    // menuPanels is used for the panels related to pilot creation, preferences,
    // game loading and game saving.

    menuPanels.Push(new MenuPanel(player, gamePanels));
    if (!conversation.IsEmpty())
        menuPanels.Push(new ConversationPanel(player, conversation));

    c.gamePanels = &gamePanels;
    c.menuPanels = &menuPanels;
    c.debugMode = debugMode;
    c.player = &player;

    auto gameloop = [](void* data) {
        context* c = static_cast<context*>(data);

        while (true) {
            if (c->toggleTimeout)
                --c->toggleTimeout;

            // Handle any events that occurred in this frame.
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                // int mpe = c->menuPanels->IsEmpty();
                // int gpe = c->gamePanels->IsEmpty();
                UI& activeUI = (c->menuPanels->IsEmpty() ? *c->gamePanels : *c->menuPanels);

                // If the mouse moves, reset the cursor movement timeout.
                if (event.type == SDL_MOUSEMOTION)
                    c->cursorTime = 0;

                if (c->debugMode && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKQUOTE)
                {
                    c->isPaused = !c->isPaused;
                }
                else if (
                    event.type == SDL_KEYDOWN && c->menuPanels->IsEmpty() && Command(event.key.keysym.sym).Has(Command::MENU)
                    && !c->gamePanels->IsEmpty() && c->gamePanels->Top()->IsInterruptible())
                {
                    // User pressed the Menu key.
                    c->menuPanels->Push(shared_ptr<Panel>(new MenuPanel(*c->player, *c->gamePanels)));
                }
                else if (event.type == SDL_QUIT)
                {
                    c->menuPanels->Quit();
                }
                else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    // The window has been resized. Adjust the raw screen size
                    // and the OpenGL viewport to match.
                    GameWindow::AdjustViewport();
                }
                else if (activeUI.Handle(event))
                {
                    // The UI handled the event.
                }
                else if (
                    event.type == SDL_KEYDOWN && !c->toggleTimeout
                    && (Command(event.key.keysym.sym).Has(Command::FULLSCREEN)
                        || (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))))
                {
                    c->toggleTimeout = 30;
                    GameWindow::ToggleFullscreen();
                }
                else if (
                    event.type == SDL_KEYDOWN && !event.key.repeat && (Command(event.key.keysym.sym).Has(Command::FASTFORWARD)))
                {
                    c->isFastForward = !c->isFastForward;
                }
            }
            SDL_Keymod mod = SDL_GetModState();
            Font::ShowUnderlines(mod & KMOD_ALT);

            // In full-screen mode, hide the cursor if inactive for ten seconds,
            // but only if the player is flying around in the main view.
            bool inFlight = (c->menuPanels->IsEmpty() && c->gamePanels->Root() == c->gamePanels->Top());
            ++c->cursorTime;
            bool shouldShowCursor = (!GameWindow::IsFullscreen() || c->cursorTime < 600 || !inFlight);
            if (shouldShowCursor != c->showCursor)
            {
                c->showCursor = shouldShowCursor;
                SDL_ShowCursor(c->showCursor);
            }

            // Tell all the panels to step forward, then draw them.
            ((!c->isPaused && c->menuPanels->IsEmpty()) ? c->gamePanels : c->menuPanels)->StepAll();

            // Caps lock slows the frame rate in debug mode.
            // Slowing eases in and out over a couple of frames.
            if ((mod & KMOD_CAPS) && inFlight && c->debugMode)
            {
#ifndef __EMSCRIPTEN__
                if (c->frameRate > 10)
                {
                    c->frameRate = max(c->frameRate - 5, 10);
                    c->timer.SetFrameRate(c->frameRate);
                }
#endif
            }
            else
            {
#ifndef __EMSCRIPTEN__
                if (c->frameRate < 60)
                {
                    c->frameRate = min(c->frameRate + 5, 60);
                    c->timer.SetFrameRate(c->frameRate);
                }
#endif

                if (c->isFastForward && inFlight)
                {
                    c->skipFrame = (c->skipFrame + 1) % 3;
                    if (c->skipFrame)
                        continue;
                }
            }
            break;
        }

        Audio::Step();

        // Events in this frame may have cleared out the menu, in which case
        // we should draw the game panels instead:
        (c->menuPanels->IsEmpty() ? c->gamePanels : c->menuPanels)->DrawAll();
        if (c->isFastForward)
            SpriteShader::Draw(SpriteSet::Get("ui/fast forward"), Screen::TopLeft() + Point(10., 10.));

        GameWindow::Step();

#ifndef __EMSCRIPTEN__
        c->timer.Wait();
#endif
    };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(gameloop, (void*)&c, 0, 1);
#else
    // IsDone becomes true when the game is quit.
    while (!menuPanels.IsDone())
    {
        gameloop(&c);
    }
#endif

    // If player quit while landed on a planet, save the game if there are
    // changes.
    if (player.GetPlanet() && gamePanels.CanSave())
        player.Save();
}

void PrintHelp()
{
    cerr << endl;
    cerr << "Command line options:" << endl;
    cerr << "    -h, --help: print this help message." << endl;
    cerr << "    -v, --version: print version information." << endl;
    cerr << "    -s, --ships: print table of ship statistics, then exit." << endl;
    cerr << "    -w, --weapons: print table of weapon statistics, then exit." << endl;
    cerr << "    -t, --talk: read and display a conversation from STDIN." << endl;
    cerr << "    -r, --resources <path>: load resources from given directory." << endl;
    cerr << "    -c, --config <path>: save user's files to given directory." << endl;
    cerr << "    -d, --debug: turn on debugging features (e.g. Caps Lock slows "
            "down instead of speeds up)."
         << endl;
    cerr << "    -p, --parse-save: load the most recent saved game and inspect "
            "it for content errors"
         << endl;
    cerr << endl;
    cerr << "Report bugs to: <https://github.com/endless-sky/endless-sky/issues>" << endl;
    cerr << "Home page: <https://endless-sky.github.io>" << endl;
    cerr << endl;
}

void PrintVersion()
{
    cerr << endl;
    cerr << "Endless Sky 0.9.13-alpha" << endl;
    cerr << "License GPLv3+: GNU GPL version 3 or later: "
            "<https://gnu.org/licenses/gpl.html>"
         << endl;
    cerr << "This is free software: you are free to change and redistribute it." << endl;
    cerr << "There is NO WARRANTY, to the extent permitted by law." << endl;
    cerr << endl;
}

Conversation LoadConversation()
{
    Conversation conversation;
    DataFile file(cin);
    for (const DataNode& node : file)
        if (node.Token(0) == "conversation")
        {
            conversation.Load(node);
            break;
        }

    const map<string, string> subs = { { "<bunks>", "[N]" },
                                       { "<cargo>", "[N tons of Commodity]" },
                                       { "<commodity>", "[Commodity]" },
                                       { "<date>", "[Day Mon Year]" },
                                       { "<day>", "[The Nth of Month]" },
                                       { "<destination>", "[Planet in the Star system]" },
                                       { "<fare>", "[N passengers]" },
                                       { "<first>", "[First]" },
                                       { "<last>", "[Last]" },
                                       { "<origin>", "[Origin Planet]" },
                                       { "<passengers>", "[your passengers]" },
                                       { "<planet>", "[Planet]" },
                                       { "<ship>", "[Ship]" },
                                       { "<system>", "[Star]" },
                                       { "<tons>", "[N tons]" } };
    return conversation.Substitute(subs);
}

#ifdef _WIN32
void InitConsole()
{
    const int UNINITIALIZED = -2;
    bool redirectStdout = _fileno(stdout) == UNINITIALIZED;
    bool redirectStderr = _fileno(stderr) == UNINITIALIZED;
    bool redirectStdin = _fileno(stdin) == UNINITIALIZED;

    // Bail if stdin, stdout, and stderr are already initialized (e.g. writing to
    // a file)
    if (!redirectStdout && !redirectStderr && !redirectStdin)
        return;

    // Bail if we fail to attach to the console
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole())
        return;

    // Perform console redirection.
    if (redirectStdout && freopen("CONOUT$", "w", stdout))
        setvbuf(stdout, nullptr, _IOFBF, 4096);
    if (redirectStderr && freopen("CONOUT$", "w", stderr))
        setvbuf(stderr, nullptr, _IOLBF, 1024);
    if (redirectStdin && freopen("CONIN$", "r", stdin))
        setvbuf(stdin, nullptr, _IONBF, 0);
}
#endif
