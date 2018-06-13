#pragma once
#include "ScreenCapture.h"
#include "internal/SCCommon.h"
#include <memory>
#include "GDIHelpers.h"

namespace SL {
    namespace Screen_Capture {

        class GDIMouseProcessor : public BaseFrameProcessor {

            HDCWrapper MonitorDC;
            HDCWrapper CaptureDC;
            std::shared_ptr<Thread_Data> Data;
            std::unique_ptr<unsigned char[]> NewImageBuffer;
            int Last_x = 0;
            int Last_y = 0;

        public:

            const int MaxCursurorSize = 32;
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data);
            DUPL_RETURN ProcessFrame();

      
        };
    }
}