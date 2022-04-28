/*
 * SPDX-FileCopyrightText: 2017-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
//
#ifndef _quirc_opencv_h_
#define _quirc_opencv_h_

#include <stdint.h>
#include "quirc.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include <opencv2/calib3d/calib3d.hpp>

#include "esp_dsp.h"

class quirc_opencv
{
    dspm::Mat matrix;
    dspm::Mat point;
    
    std::vector<cv::Point2f> points_src;
    std::vector<cv::Point2f> points_dest;
    cv::Mat point_;
    cv::Mat matrix_;

    public:
        quirc_opencv();
        virtual ~quirc_opencv();
        // The method take imput image and convert it to the QR size image
        // and save to the result data array
        bool ConvertImage(struct quirc* qr);
    private:
        // Internal method to tonvert input image to QR size image
        void warpPerspective(uint8_t* src, cv::Size src_size, uint8_t* dst, cv::Size dst_size, cv::Mat& M);

};

#endif // _quirc_opencv_h_