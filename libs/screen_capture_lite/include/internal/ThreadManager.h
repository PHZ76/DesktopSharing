#pragma once
#include "internal/SCCommon.h"
#include "ScreenCapture.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// this is internal stuff..
namespace SL {
namespace Screen_Capture {
    class ThreadManager {

        std::vector<std::thread> m_ThreadHandles;
        std::shared_ptr<std::atomic_bool> TerminateThreadsEvent;

      public:
        ThreadManager();
        ~ThreadManager();
        void Init(const std::shared_ptr<Thread_Data> &settings);
        void Join();
    };

    template <class T, class F, class... E> bool TryCaptureMouse(const F &data, E... args)
    {
        T frameprocessor;
        frameprocessor.ImageBufferSize = frameprocessor.MaxCursurorSize * frameprocessor.MaxCursurorSize * sizeof(ImageBGRA);
        frameprocessor.ImageBuffer = std::make_unique<unsigned char[]>(frameprocessor.ImageBufferSize);
        auto ret = frameprocessor.Init(data);
        if (ret != DUPL_RETURN_SUCCESS) {
            return false;
        } 
        while (!data->CommonData_.TerminateThreadsEvent) {
            // get a copy of the shared_ptr in a safe way

            std::shared_ptr<Timer> timer;
            if constexpr(sizeof...(args) == 1) {
                timer = std::atomic_load(&data->WindowCaptureData.MouseTimer);
            }
            else {
                timer = std::atomic_load(&data->ScreenCaptureData.MouseTimer);
            }

            timer->start();
            // Process Frame
            ret = frameprocessor.ProcessFrame();
            if (ret != DUPL_RETURN_SUCCESS) {
                if (ret == DUPL_RETURN_ERROR_EXPECTED) {
                    // The system is in a transition state so request the duplication be restarted
                    data->CommonData_.ExpectedErrorEvent = true;
                    std::cout << "Exiting Thread due to expected error " << std::endl;
                }
                else {
                    // Unexpected error so exit the application
                    data->CommonData_.UnexpectedErrorEvent = true;
                    std::cout << "Exiting Thread due to Unexpected error " << std::endl;
                }
                return true;
            }
            timer->wait();
            while (data->CommonData_.Paused) {
                std::this_thread::sleep_for(50ms);
            }
        }
        return true;
    }

    inline bool HasMonitorsChanged(const std::vector<Monitor> &startmonitors, const std::vector<Monitor> &nowmonitors)
    {
        if (startmonitors.size() != nowmonitors.size())
            return true;
        for (size_t i = 0; i < startmonitors.size(); i++) {
            if (startmonitors[i].Height != nowmonitors[i].Height || startmonitors[i].Id != nowmonitors[i].Id ||
                startmonitors[i].Index != nowmonitors[i].Index || startmonitors[i].OffsetX != nowmonitors[i].OffsetX ||
                startmonitors[i].OffsetY != nowmonitors[i].OffsetY || startmonitors[i].Width != nowmonitors[i].Width)
                return true;
        }
        return false;
    }
    template <class T, class F> bool TryCaptureMonitor(const F &data, Monitor &monitor)
    {
        T frameprocessor;   
        frameprocessor.ImageBufferSize = Width(monitor) * Height(monitor) * sizeof(ImageBGRA);
        if (data->ScreenCaptureData.OnFrameChanged) { // only need the old buffer if difs are needed. If no dif is needed, then the
                                                      // image is always new
            frameprocessor.ImageBuffer = std::make_unique<unsigned char[]>(frameprocessor.ImageBufferSize);
        }
        auto startmonitors = GetMonitors();
        auto ret = frameprocessor.Init(data, monitor);
        if (ret != DUPL_RETURN_SUCCESS) {
            return false;
        }
    
        while (!data->CommonData_.TerminateThreadsEvent) {
            // get a copy of the shared_ptr in a safe way
            
            frameprocessor.Resume();
            auto timer = std::atomic_load(&data->ScreenCaptureData.FrameTimer);
            timer->start();
            auto monitors = GetMonitors();
            if (isMonitorInsideBounds(monitors, monitor) && !HasMonitorsChanged(startmonitors, monitors)) {
                ret = frameprocessor.ProcessFrame(monitors[Index(monitor)]);
            }
            else {
                // something happened, rebuild
                ret = DUPL_RETURN_ERROR_EXPECTED;
            }
            if (ret != DUPL_RETURN_SUCCESS) {
                if (ret == DUPL_RETURN_ERROR_EXPECTED) {
                    // The system is in a transition state so request the duplication be restarted
                    data->CommonData_.ExpectedErrorEvent = true;
                    std::cout << "Exiting Thread due to expected error " << std::endl;
                }
                else {
                    // Unexpected error so exit the application
                    data->CommonData_.UnexpectedErrorEvent = true;
                    std::cout << "Exiting Thread due to Unexpected error " << std::endl;
                }
                return true;
            }
            timer->wait();
            while (data->CommonData_.Paused) {
                frameprocessor.Pause();
                std::this_thread::sleep_for(50ms);
            }
        }
        return true;
    }

    template <class T, class F> bool TryCaptureWindow(const F &data, Window &wnd)
    {
        T frameprocessor;
        frameprocessor.ImageBufferSize = wnd.Size.x * wnd.Size.y * sizeof(ImageBGRA);
        if (data->WindowCaptureData.OnFrameChanged) { // only need the old buffer if difs are needed. If no dif is needed, then the
                                                      // image is always new
            frameprocessor.ImageBuffer = std::make_unique<unsigned char[]>(frameprocessor.ImageBufferSize);
        }
        auto ret = frameprocessor.Init(data, wnd);
        if (ret != DUPL_RETURN_SUCCESS) {
            return false;
        }
        while (!data->CommonData_.TerminateThreadsEvent) {
            // get a copy of the shared_ptr in a safe way
            auto timer = std::atomic_load(&data->WindowCaptureData.FrameTimer);
            timer->start();
            ret = frameprocessor.ProcessFrame(wnd);
            if (ret != DUPL_RETURN_SUCCESS) {
                if (ret == DUPL_RETURN_ERROR_EXPECTED) {
                    // The system is in a transition state so request the duplication be restarted
                    data->CommonData_.ExpectedErrorEvent = true;
                    std::cout << "Exiting Thread due to expected error " << std::endl;
                }
                else {
                    // Unexpected error so exit the application
                    data->CommonData_.UnexpectedErrorEvent = true;
                    std::cout << "Exiting Thread due to Unexpected error " << std::endl;
                }
                return true;
            }
            timer->wait();
            while (data->CommonData_.Paused) {
                std::this_thread::sleep_for(50ms);
            }
        }
        return true;
    }

    void RunCaptureMonitor(std::shared_ptr<Thread_Data> data, Monitor monitor);
    void RunCaptureWindow(std::shared_ptr<Thread_Data> data, Window window);

    void RunCaptureMouse(std::shared_ptr<Thread_Data> data);
} // namespace Screen_Capture
} // namespace SL
