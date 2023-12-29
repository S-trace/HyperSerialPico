/* stats.h
*
*  MIT License
*
*  Copyright (c) 2023 awawa-dev
*
*  https://github.com/awawa-dev/HyperSerialPico

*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include <cmath>

// statistics (stats sent only when there is no communication)
class
{
	unsigned long startTime = 0;
	uint16_t goodFrames = 0;
	uint16_t showFrames = 0;
	uint16_t totalFrames = 0;
	uint16_t finalGoodFrames = 0;
	uint16_t finalShowFrames = 0;
	uint16_t finalTotalFrames = 0;

    // uint32_t will wrap here each 2^24 (16777216) frames, slightly more than 36 hours at 125 FPS.
    // I'm think uint64_t is OK here.
    uint64_t milliampsSum = 0;
    uint64_t underpowerDesiredMilliampsSum = 0;
    uint64_t underpowerFramesCount = 0;
    uint64_t framesCount = 0;

    long double underpowerPercentSum = 0; // The count of frames which were limited in current (only limitted frames).
    long double powerPercentSum = 0; // The power cap sum for all the frames (including non-limited ones).


public:
		/**
		 * @brief Get the start time of the current period
		 *
		 * @return unsigned long
		 */
		inline unsigned long getStartTime()
		{
			return startTime;
		}

		/**
		 * @brief Detected new frame
		 *
		 */
		inline void increaseTotal()
		{
			totalFrames++;
		}

		/**
		 * @brief The frame is received and shown
		 *
		 */
		inline void increaseShow()
		{
			showFrames++;
		}

        /**
    * @brief The frame is processed by current limiter
    *
    */
        void updatePowerStats(float powerPercentage, float underpowerPercentage,
                              uint32_t milliamps, uint32_t underpowerDesiredMilliamps)
        {
            framesCount++;
            if (std::fabs(1 - underpowerPercentage) < 0.00001) {
                underpowerFramesCount++; // Count the frame as current-limited
                underpowerPercentSum          += underpowerPercentage;       // How much percents it was limited
                underpowerDesiredMilliampsSum += underpowerDesiredMilliamps; // How much current it wanted to consume
            }

            powerPercentSum += powerPercentage; // How bright is the frame
            milliampsSum    += milliamps;       // How much current it should consume after limiting
            underpowerDesiredMilliampsSum += underpowerDesiredMilliamps;
        }

		/**
		 * @brief The frame is received correctly (not yet displayed)
		 *
		 */
		inline void increaseGood()
		{
			goodFrames++;
		}

		/**
		 * @brief Get number of correctly received frames
		 *
		 * @return uint16_t
		 */
		inline uint16_t getGoodFrames()
		{
			return goodFrames;
		}

		/**
		 * @brief Period restart, save current statistics ans send them later if there is no incoming communication
		 *
		 * @param currentTime
		 */
		void update(unsigned long currentTime)
		{
			if (totalFrames > 0)
			{
				finalShowFrames = showFrames;
				finalGoodFrames = std::min(goodFrames, totalFrames);
				finalTotalFrames = totalFrames;
			}

			startTime = currentTime;
			goodFrames = 0;
			totalFrames = 0;
			showFrames = 0;
		}

		/**
		 * @brief Print last saved statistics to the serial port
		 *
		 * @param curTime
		 * @param taskHandle
		 */
		void print(unsigned long curTime, TaskHandle_t taskHandle1, TaskHandle_t taskHandle2)
		{
			char output[128];

			startTime = curTime;
			goodFrames = 0;
			totalFrames = 0;
			showFrames = 0;

            // Will be between 0 and 1, so float is fine here.
            float powerPercentAverage = (float) ((double) powerPercentSum / (double) framesCount) * 100;
            // Up to 4.3 megaamperes, should be pretty enough. uint16_t will wrap after just 65.5 amperes
            uint32_t milliampsAverage = (uint32_t)((double) milliampsSum / (double) framesCount);

            float underpowerPercentAverage = (float) ((double) underpowerPercentSum / (double) underpowerFramesCount) * 100;
            uint32_t underpowerRequestedMilliampsAverage = (uint32_t)((double) underpowerDesiredMilliampsSum / (double) underpowerFramesCount);

			snprintf(output, sizeof(output), "HyperHDR frames: %u (FPS), receiv.: %u, good: %u, incompl.: %u, mem1: %i, mem2: %i, heap: %zu\r\n",
                     finalShowFrames, finalTotalFrames, finalGoodFrames, (finalTotalFrames - finalGoodFrames),
						(taskHandle1 != nullptr) ? uxTaskGetStackHighWaterMark(taskHandle1) : 0,
						(taskHandle2 != nullptr) ? uxTaskGetStackHighWaterMark(taskHandle2) : 0,
                     xPortGetFreeHeapSize()
                     );
            printf(output);

            snprintf(output, sizeof(output),"Current limiter: %u frames total (%u underpower ones, %f%%),\r\n",
                     framesCount, underpowerFramesCount, underpowerFramesCount * 100 / framesCount);
            printf(output);

            snprintf(output, sizeof(output), "%u mA average (%u mA avg were requested, %f%% more than you have for now)\r\n",
                     milliampsAverage, underpowerRequestedMilliampsAverage, underpowerRequestedMilliampsAverage * 100 / milliampsAverage);
            printf(output);

            snprintf(output, sizeof(output), "%f%% average load (limitted by %f%%)\r\n",
                     powerPercentAverage, underpowerPercentAverage);
            printf(output);

			#if defined(NEOPIXEL_RGBW)
				calibrationConfig.printCalibration();
			#endif
        }

		/**
		 * @brief Reset statistics
		 *
		 */
		void reset(unsigned long currentTime)
		{
			startTime = currentTime;

			finalShowFrames = 0;
			finalGoodFrames = 0;
			finalTotalFrames = 0;

			goodFrames = 0;
			totalFrames = 0;
			showFrames = 0;

            milliampsSum = 0;
            underpowerDesiredMilliampsSum = 0;
            underpowerFramesCount = 0;
            framesCount = 0;

            underpowerPercentSum = 0;
            powerPercentSum = 0;
        }

		void lightReset(unsigned long curTime, bool hasData)
		{
			if (hasData)
				startTime = curTime;

			goodFrames = 0;
			totalFrames = 0;
			showFrames = 0;
		}

} statistics;

#endif