#pragma once

#include <stdint.h>


namespace er::scaling
{
    class Score {
    public:
        static int computeScore(uint8_t scaling);
    };
}

