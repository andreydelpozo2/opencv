/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "test_precomp.hpp"

#ifdef HAVE_CUDA

//////////////////////////////////////////////////////
// FGDStatModel

namespace cv
{
    template<> void Ptr<CvBGStatModel>::delete_obj()
    {
        cvReleaseBGStatModel(&obj);
    }
}

PARAM_TEST_CASE(FGDStatModel, cv::gpu::DeviceInfo, std::string, Channels)
{
    cv::gpu::DeviceInfo devInfo;
    std::string inputFile;
    int out_cn;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        cv::gpu::setDevice(devInfo.deviceID());

        inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "video/" + GET_PARAM(1);

        out_cn = GET_PARAM(2);
    }
};

GPU_TEST_P(FGDStatModel, Update)
{
    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;
    cap >> frame;
    ASSERT_FALSE(frame.empty());

    IplImage ipl_frame = frame;
    cv::Ptr<CvBGStatModel> model(cvCreateFGDStatModel(&ipl_frame));

    cv::gpu::GpuMat d_frame(frame);
    cv::gpu::FGDStatModel d_model(out_cn);
    d_model.create(d_frame);

    cv::Mat h_background;
    cv::Mat h_foreground;
    cv::Mat h_background3;

    cv::Mat backgroundDiff;
    cv::Mat foregroundDiff;

    for (int i = 0; i < 5; ++i)
    {
        cap >> frame;
        ASSERT_FALSE(frame.empty());

        ipl_frame = frame;
        int gold_count = cvUpdateBGStatModel(&ipl_frame, model);

        d_frame.upload(frame);

        int count = d_model.update(d_frame);

        ASSERT_EQ(gold_count, count);

        cv::Mat gold_background(model->background);
        cv::Mat gold_foreground(model->foreground);

        if (out_cn == 3)
            d_model.background.download(h_background3);
        else
        {
            d_model.background.download(h_background);
            cv::cvtColor(h_background, h_background3, cv::COLOR_BGRA2BGR);
        }
        d_model.foreground.download(h_foreground);

        ASSERT_MAT_NEAR(gold_background, h_background3, 1.0);
        ASSERT_MAT_NEAR(gold_foreground, h_foreground, 0.0);
    }
}

INSTANTIATE_TEST_CASE_P(GPU_Video, FGDStatModel, testing::Combine(
    ALL_DEVICES,
    testing::Values(std::string("768x576.avi")),
    testing::Values(Channels(3), Channels(4))));

//////////////////////////////////////////////////////
// MOG

namespace
{
    IMPLEMENT_PARAM_CLASS(UseGray, bool)
    IMPLEMENT_PARAM_CLASS(LearningRate, double)
}

PARAM_TEST_CASE(MOG, cv::gpu::DeviceInfo, std::string, UseGray, LearningRate, UseRoi)
{
    cv::gpu::DeviceInfo devInfo;
    std::string inputFile;
    bool useGray;
    double learningRate;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        cv::gpu::setDevice(devInfo.deviceID());

        inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "video/" + GET_PARAM(1);

        useGray = GET_PARAM(2);

        learningRate = GET_PARAM(3);

        useRoi = GET_PARAM(4);
    }
};

GPU_TEST_P(MOG, Update)
{
    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;
    cap >> frame;
    ASSERT_FALSE(frame.empty());

    cv::gpu::MOG_GPU mog;
    cv::gpu::GpuMat foreground = createMat(frame.size(), CV_8UC1, useRoi);

    cv::BackgroundSubtractorMOG mog_gold;
    cv::Mat foreground_gold;

    for (int i = 0; i < 10; ++i)
    {
        cap >> frame;
        ASSERT_FALSE(frame.empty());

        if (useGray)
        {
            cv::Mat temp;
            cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
            cv::swap(temp, frame);
        }

        mog(loadMat(frame, useRoi), foreground, (float)learningRate);

        mog_gold(frame, foreground_gold, learningRate);

        ASSERT_MAT_NEAR(foreground_gold, foreground, 0.0);
    }
}

INSTANTIATE_TEST_CASE_P(GPU_Video, MOG, testing::Combine(
    ALL_DEVICES,
    testing::Values(std::string("768x576.avi")),
    testing::Values(UseGray(true), UseGray(false)),
    testing::Values(LearningRate(0.0), LearningRate(0.01)),
    WHOLE_SUBMAT));

//////////////////////////////////////////////////////
// MOG2

PARAM_TEST_CASE(MOG2, cv::gpu::DeviceInfo, std::string, UseGray, UseRoi)
{
    cv::gpu::DeviceInfo devInfo;
    std::string inputFile;
    bool useGray;
    bool useRoi;

    virtual void SetUp()
    {
        devInfo = GET_PARAM(0);
        cv::gpu::setDevice(devInfo.deviceID());

        inputFile = std::string(cvtest::TS::ptr()->get_data_path()) + "video/" + GET_PARAM(1);

        useGray = GET_PARAM(2);

        useRoi = GET_PARAM(3);
    }
};

GPU_TEST_P(MOG2, Update)
{
    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;
    cap >> frame;
    ASSERT_FALSE(frame.empty());

    cv::gpu::MOG2_GPU mog2;
    cv::gpu::GpuMat foreground = createMat(frame.size(), CV_8UC1, useRoi);

    cv::BackgroundSubtractorMOG2 mog2_gold;
    cv::Mat foreground_gold;

    for (int i = 0; i < 10; ++i)
    {
        cap >> frame;
        ASSERT_FALSE(frame.empty());

        if (useGray)
        {
            cv::Mat temp;
            cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
            cv::swap(temp, frame);
        }

        mog2(loadMat(frame, useRoi), foreground);

        mog2_gold(frame, foreground_gold);

        double norm = cv::norm(foreground_gold, cv::Mat(foreground), cv::NORM_L1);

        norm /= foreground_gold.size().area();

        ASSERT_LE(norm, 0.09);
    }
}

GPU_TEST_P(MOG2, getBackgroundImage)
{
    if (useGray)
        return;

    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;

    cv::gpu::MOG2_GPU mog2;
    cv::gpu::GpuMat foreground;

    cv::BackgroundSubtractorMOG2 mog2_gold;
    cv::Mat foreground_gold;

    for (int i = 0; i < 10; ++i)
    {
        cap >> frame;
        ASSERT_FALSE(frame.empty());

        mog2(loadMat(frame, useRoi), foreground);

        mog2_gold(frame, foreground_gold);
    }

    cv::gpu::GpuMat background = createMat(frame.size(), frame.type(), useRoi);
    mog2.getBackgroundImage(background);

    cv::Mat background_gold;
    mog2_gold.getBackgroundImage(background_gold);

    ASSERT_MAT_NEAR(background_gold, background, 0);
}

INSTANTIATE_TEST_CASE_P(GPU_Video, MOG2, testing::Combine(
    ALL_DEVICES,
    testing::Values(std::string("768x576.avi")),
    testing::Values(UseGray(true), UseGray(false)),
    WHOLE_SUBMAT));

//////////////////////////////////////////////////////
// VIBE

PARAM_TEST_CASE(VIBE, cv::gpu::DeviceInfo, cv::Size, MatType, UseRoi)
{
};

GPU_TEST_P(VIBE, Accuracy)
{
    const cv::gpu::DeviceInfo devInfo = GET_PARAM(0);
    cv::gpu::setDevice(devInfo.deviceID());
    const cv::Size size = GET_PARAM(1);
    const int type = GET_PARAM(2);
    const bool useRoi = GET_PARAM(3);

    const cv::Mat fullfg(size, CV_8UC1, cv::Scalar::all(255));

    cv::Mat frame = randomMat(size, type, 0.0, 100);
    cv::gpu::GpuMat d_frame = loadMat(frame, useRoi);

    cv::gpu::VIBE_GPU vibe;
    cv::gpu::GpuMat d_fgmask = createMat(size, CV_8UC1, useRoi);
    vibe.initialize(d_frame);

    for (int i = 0; i < 20; ++i)
        vibe(d_frame, d_fgmask);

    frame = randomMat(size, type, 160, 255);
    d_frame = loadMat(frame, useRoi);
    vibe(d_frame, d_fgmask);

    // now fgmask should be entirely foreground
    ASSERT_MAT_NEAR(fullfg, d_fgmask, 0);
}

INSTANTIATE_TEST_CASE_P(GPU_Video, VIBE, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    testing::Values(MatType(CV_8UC1), MatType(CV_8UC3), MatType(CV_8UC4)),
    WHOLE_SUBMAT));

//////////////////////////////////////////////////////
// GMG

PARAM_TEST_CASE(GMG, cv::gpu::DeviceInfo, cv::Size, MatDepth, Channels, UseRoi)
{
};

GPU_TEST_P(GMG, Accuracy)
{
    const cv::gpu::DeviceInfo devInfo = GET_PARAM(0);
    cv::gpu::setDevice(devInfo.deviceID());
    const cv::Size size = GET_PARAM(1);
    const int depth = GET_PARAM(2);
    const int channels = GET_PARAM(3);
    const bool useRoi = GET_PARAM(4);

    const int type = CV_MAKE_TYPE(depth, channels);

    const cv::Mat zeros(size, CV_8UC1, cv::Scalar::all(0));
    const cv::Mat fullfg(size, CV_8UC1, cv::Scalar::all(255));

    cv::Mat frame = randomMat(size, type, 0, 100);
    cv::gpu::GpuMat d_frame = loadMat(frame, useRoi);

    cv::gpu::GMG_GPU gmg;
    gmg.numInitializationFrames = 5;
    gmg.smoothingRadius = 0;
    gmg.initialize(d_frame.size(), 0, 255);

    cv::gpu::GpuMat d_fgmask = createMat(size, CV_8UC1, useRoi);

    for (int i = 0; i < gmg.numInitializationFrames; ++i)
    {
        gmg(d_frame, d_fgmask);

        // fgmask should be entirely background during training
        ASSERT_MAT_NEAR(zeros, d_fgmask, 0);
    }

    frame = randomMat(size, type, 160, 255);
    d_frame = loadMat(frame, useRoi);
    gmg(d_frame, d_fgmask);

    // now fgmask should be entirely foreground
    ASSERT_MAT_NEAR(fullfg, d_fgmask, 0);
}

INSTANTIATE_TEST_CASE_P(GPU_Video, GMG, testing::Combine(
    ALL_DEVICES,
    DIFFERENT_SIZES,
    testing::Values(MatType(CV_8U), MatType(CV_16U), MatType(CV_32F)),
    testing::Values(Channels(1), Channels(3), Channels(4)),
    WHOLE_SUBMAT));

#endif // HAVE_CUDA
