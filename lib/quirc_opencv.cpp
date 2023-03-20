/*
 * SPDX-FileCopyrightText: 2017-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
//
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <unistd.h>
#include "quirc.h"
#include "quirc_opencv.h"
#include "quirc_internal.h"
#include "quirc_opencv_c.h"

static const char *TAG = "QRcode-opencv";
#include "esp_log.h"

quirc_opencv::quirc_opencv() : matrix(3, 3), point(3, 1), point_(1, 3, CV_32F), matrix_(3, 3, CV_32F)
{
}

quirc_opencv::~quirc_opencv()
{
}

void quirc_opencv::warpPerspective(uint8_t *src, cv::Size src_size, uint8_t *dst, cv::Size dst_size, cv::Mat &M)
{
    // Clear initial data
    matrix *= 0;
    point *= 0;

    for (int x = 0; x < 3; x++)
    {
        for (int y = 0; y < 3; y++)
        {
            matrix(y, x) = M.at<double>(y, x);
        }
    }
    matrix = matrix.inverse();

    for (int y = 0; y < dst_size.height; y++)
    {
        // std::cout << "my_warpPerspective1 y = " << y << std::endl;
        for (int x = 0; x < dst_size.width; x++)
        {
            point(0, 0) = x;
            point(1, 0) = y;
            point(2, 0) = 1;
            dspm::Mat aaa = matrix * point;
            float rest = 1 / aaa(2, 0);
            uint16_t x__ = rint(aaa(0, 0) * rest);
            uint16_t y__ = rint(aaa(1, 0) * rest);

            if ((x__ < src_size.width) &&
                (y__ < src_size.height))
            {
                dst[y * dst_size.width + x] = src[y__ * src_size.width + x__];
            }
        }
    }
}

bool quirc_opencv::ConvertImage(struct quirc *qr)
{
    if (qr->num_capstones < 3)
        return false;

    struct quirc_grid *gr = &qr->grids[0];

    struct quirc_capstone *a = &(qr->capstones[gr->caps[0]]);
    struct quirc_capstone *b = &(qr->capstones[gr->caps[1]]);
    struct quirc_capstone *c = &(qr->capstones[gr->caps[2]]);

    float dx = 7;
    float dw = qr->grid_size - dx;
    points_src.clear();
    points_dest.clear();

    if (qr->align_point.x != 0)
    {
        // center a
        points_src.push_back(cv::Point2f(a->center.x, a->center.y));
        points_dest.push_back(cv::Point2f(3, 3 + dw));
        // center b
        points_src.push_back(cv::Point2f(b->center.x, b->center.y));
        points_dest.push_back(cv::Point2f(3, 3));
        // center c
        points_src.push_back(cv::Point2f(c->center.x, c->center.y));
        points_dest.push_back(cv::Point2f(3 + dw, 3));

        points_src.push_back(cv::Point2f(qr->align_point.x, qr->align_point.y));
        points_dest.push_back(cv::Point2f(-0.5 + dw, -0.5 + dw));
    }
    else
    {
        // point a
        points_src.push_back(cv::Point2f(a->corners[0].x, a->corners[0].y));
        points_dest.push_back(cv::Point2f(-0.5, -0.5 + dw));
        points_src.push_back(cv::Point2f(a->corners[1].x, a->corners[1].y));
        points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5 + dw));
        points_src.push_back(cv::Point2f(a->corners[2].x, a->corners[2].y));
        points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5 + dx + dw));
        points_src.push_back(cv::Point2f(a->corners[3].x, a->corners[3].y));
        points_dest.push_back(cv::Point2f(-0.5, -0.5 + dx + dw));
        // point b
        points_src.push_back(cv::Point2f(b->corners[0].x, b->corners[0].y));
        points_dest.push_back(cv::Point2f(-0.5, -0.5));
        points_src.push_back(cv::Point2f(b->corners[1].x, b->corners[1].y));
        points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5));
        points_src.push_back(cv::Point2f(b->corners[2].x, b->corners[2].y));
        points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5 + dx));
        points_src.push_back(cv::Point2f(b->corners[3].x, b->corners[3].y));
        points_dest.push_back(cv::Point2f(-0.5, -0.5 + dx));
        // point c
        points_src.push_back(cv::Point2f(c->corners[0].x, c->corners[0].y));
        points_dest.push_back(cv::Point2f(-0.5 + dw, -0.5));
        points_src.push_back(cv::Point2f(c->corners[1].x, c->corners[1].y));
        points_dest.push_back(cv::Point2f(-0.5 + dx + dw, -0.5));
        points_src.push_back(cv::Point2f(c->corners[2].x, c->corners[2].y));
        points_dest.push_back(cv::Point2f(-0.5 + dx + dw, -0.5 + dx));
        points_src.push_back(cv::Point2f(c->corners[3].x, c->corners[3].y));
        points_dest.push_back(cv::Point2f(-0.5 + +dw, -0.5 + dx));
    }

    ESP_LOGD(TAG, "Grid size = %i, qr->h=%i, qr->w=%i", qr->grid_size, qr->h, qr->w);
	cv::Mat temp;
    cv::Mat H12 = cv::findHomography(points_src, points_dest, cv::RANSAC, 3, temp, 20, 0.999); 
    float diff = 0;
    {
        if (qr->align_point.x != 0)
        {
            // point a
            points_src.push_back(cv::Point2f(a->corners[0].x, a->corners[0].y));
            points_dest.push_back(cv::Point2f(-0.5, -0.5 + dw));
            points_src.push_back(cv::Point2f(a->corners[1].x, a->corners[1].y));
            points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5 + dw));
            points_src.push_back(cv::Point2f(a->corners[2].x, a->corners[2].y));
            points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5 + dx + dw));
            points_src.push_back(cv::Point2f(a->corners[3].x, a->corners[3].y));
            points_dest.push_back(cv::Point2f(-0.5, -0.5 + dx + dw));
            // point b
            points_src.push_back(cv::Point2f(b->corners[0].x, b->corners[0].y));
            points_dest.push_back(cv::Point2f(-0.5, -0.5));
            points_src.push_back(cv::Point2f(b->corners[1].x, b->corners[1].y));
            points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5));
            points_src.push_back(cv::Point2f(b->corners[2].x, b->corners[2].y));
            points_dest.push_back(cv::Point2f(-0.5 + dx, -0.5 + dx));
            points_src.push_back(cv::Point2f(b->corners[3].x, b->corners[3].y));
            points_dest.push_back(cv::Point2f(-0.5, -0.5 + dx));
            // point c
            points_src.push_back(cv::Point2f(c->corners[0].x, c->corners[0].y));
            points_dest.push_back(cv::Point2f(-0.5 + dw, -0.5));
            points_src.push_back(cv::Point2f(c->corners[1].x, c->corners[1].y));
            points_dest.push_back(cv::Point2f(-0.5 + dx + dw, -0.5));
            points_src.push_back(cv::Point2f(c->corners[2].x, c->corners[2].y));
            points_dest.push_back(cv::Point2f(-0.5 + dx + dw, -0.5 + dx));
            points_src.push_back(cv::Point2f(c->corners[3].x, c->corners[3].y));
            points_dest.push_back(cv::Point2f(-0.5 + +dw, -0.5 + dx));
        }
        H12.convertTo(matrix_, matrix_.type());
        for (int i = 0; i < points_src.size(); i++)
        {
            point_.at<float>(0, 0) = points_src[i].x;
            point_.at<float>(0, 1) = points_src[i].y;
            point_.at<float>(0, 2) = 1;
            cv::Mat aaa = matrix_ * point_.t();
            float x__ = aaa.at<float>(0, 0) / aaa.at<float>(2, 0);
            float y__ = aaa.at<float>(1, 0) / aaa.at<float>(2, 0);
            diff += abs(x__ - points_dest[i].x);
            diff += abs(y__ - points_dest[i].y);
        }
        ESP_LOGD(TAG,"Diff = %f" , diff);

        // Check if the error is low
        if (diff > 8)
            return false;
    }

    warpPerspective(qr->image, cv::Size(qr->w, qr->h) , qr->pixels, cv::Size(qr->grid_size, qr->grid_size), H12);

    uint8_t threshold = otsu_pic(qr->pixels, qr->grid_size, qr->grid_size, 0, 0, qr->grid_size, qr->grid_size);

    ESP_LOGD(TAG, "==>Final threshold = %i", threshold);

    for (size_t y = 0; y < qr->grid_size; y++)
    {
        for (int x = 0; x < qr->grid_size; x++)
        {
            uint8_t av_val = qr->pixels[y*qr->grid_size + x];
            if (av_val <= threshold)
            {
                qr->pixels[qr->grid_size * y + x] = 0x01;
            }
            else
            {
                qr->pixels[qr->grid_size * y + x] = 0x00;
            }
        }
    }
    return true;
}

static quirc_opencv* s_quirc_opencv;

extern "C" void quirc_opencv_init(void)
{
    s_quirc_opencv = new quirc_opencv();
}
extern "C" void quirc_opencv_rectify(struct quirc* qr)
{
    s_quirc_opencv->ConvertImage(qr);
}
