#pragma once
#include "internal/SCCommon.h"
#include <memory>
#include "TargetConditionals.h"
#include <ApplicationServices/ApplicationServices.h>

namespace SL {
    namespace Screen_Capture {
        
        class NSMouseProcessor: public BaseFrameProcessor {
            int Last_x =0;
            int Last_y =0;
            std::unique_ptr<unsigned char[]> OldImageBuffer;
        public:
            const int MaxCursurorSize=32;
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data);
            DUPL_RETURN ProcessFrame();

        };

    }
}
