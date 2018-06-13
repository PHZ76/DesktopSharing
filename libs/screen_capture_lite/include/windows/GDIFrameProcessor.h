#pragma once
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <memory>
#include "GDIHelpers.h"

namespace SL {
    namespace Screen_Capture {

        class GDIFrameProcessor : public BaseFrameProcessor {
            HDCWrapper MonitorDC;
            HDCWrapper CaptureDC;
            HBITMAPWrapper CaptureBMP;
            Monitor SelectedMonitor;
            HWND SelectedWindow;
            std::unique_ptr<unsigned char[]> NewImageBuffer;

            std::shared_ptr<Thread_Data> Data;
        public:
            void Pause() {}
            void Resume() {}
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data, const Monitor& monitor);
            DUPL_RETURN ProcessFrame(const Monitor& currentmonitorinfo);
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data, const Window& selectedwindow);
            DUPL_RETURN ProcessFrame(Window& selectedwindow);
        };
    }
}