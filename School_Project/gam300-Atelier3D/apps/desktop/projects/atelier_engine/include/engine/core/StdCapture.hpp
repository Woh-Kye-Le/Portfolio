/* !
@file    	    StdCapture.hpp
@author  	    zhihenghenrey.hu@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares StdCapture, which redirects stdout/stderr into a Logger while still
    echoing the original output to the terminal.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <array>
#include <cstdio>
#include <thread>

namespace Engine::Core
{
    class Logger;

    /*!
    \brief  Sends everything printed to stdout and stderr into a Logger, one message per line:
            printf, puts, std::cout, std::clog, std::cerr, fprintf(stderr), std::print, and output
            from third-party libraries. stdout lines are logged as Info, stderr lines as Error, in
            category General and tagged with their source. The terminal still shows them (see
            Terminal in LogSinks.hpp).

            How: stdout/stderr are redirected into pipes at the file descriptor level and a reader
            thread per pipe turns what arrives into log lines. Only one StdCapture can run at a
            time.

            Limits:
             - Captured lines have no file:line, so prefer LOG_INFO / LOG_WARN / LOG_ERROR in our
               own code.
             - On a hard crash, the last lines still inside the pipe can be lost; LOG_* messages
               are not affected.
             - Lines keep their order within stdout and within stderr, but a stdout line and a
               stderr line printed at almost the same moment can appear in either order (each
               stream has its own pipe).
             - OutputDebugString and direct console writes (WriteConsoleW to CONOUT$) are not
               captured.
             - A stream that has no file descriptor (GUI app without a console) is left alone.
    */
    class StdCapture
    {
    public:
        /*!
        \brief   Constructs a capture bound to logger; Start() must be called separately.
        \param   logger Logger that captured stdout/stderr lines are written to.
        */
        explicit StdCapture(Logger& logger);

        /*!
        \brief   Stops the capture if still running, restoring stdout and stderr.
        */
        ~StdCapture();

        StdCapture(const StdCapture&) = delete;
        StdCapture& operator=(const StdCapture&) = delete;

        /*!
        \brief   Redirects stdout and stderr into the logger and starts the reader threads.
        \return  false if another StdCapture is already running or neither stream could be
                 redirected, true otherwise.
        */
        bool Start();

        /*!
        \brief   Restores stdout and stderr and logs any lines still in flight. Safe to call
                 twice.
        */
        void Stop();

        /*!
        \brief   Returns true while stdout/stderr are redirected into the logger.
        */
        bool IsRunning() const noexcept { return running_; }

    private:
        // One per stream (stdout and stderr), each with its own reader thread
        struct Channel
        {
            std::FILE* stream = nullptr;
            int fd = -1;      // stdout / stderr descriptor that gets redirected
            int savedFd = -1; // copy of the original descriptor (the terminal)
            bool isError = false;
            std::thread reader;
        };

        /*!
        \brief   Redirects channel's stream into a pipe and starts its reader thread.
        \param   channel Channel to redirect.
        \return  true on success.
        */
        bool Redirect(Channel& channel);

        /*!
        \brief   Restores channel's stream to its original descriptor and joins its reader thread.
        \param   channel Channel to restore.
        */
        void Restore(Channel& channel);

        /*!
        \brief   Reader-thread body: reads from readFd until it closes, logging each complete line.
        \param   readFd Read end of the redirect pipe.
        \param   isError Logs at Error level when true, Info level when false.
        */
        void ReadLoop(int readFd, bool isError);

        Logger& logger_;
        std::array<Channel, 2> channels_;
        bool running_ = false;
    };
}
