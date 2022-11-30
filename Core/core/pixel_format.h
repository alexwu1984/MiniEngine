#pragma once

namespace core
{
    inline constexpr unsigned int pixel_format_fourcc(const char (&fourcc)[5])
    {
        return (static_cast<unsigned int>(fourcc[0])) |
            (static_cast<unsigned int>(fourcc[1]) << 8) |
            (static_cast<unsigned int>(fourcc[2]) << 16) |
            (static_cast<unsigned int>(fourcc[3]) << 24);
    }

    inline constexpr unsigned int pixel_format_fourcc(char a, char b, char c, char d)
    {
        return (static_cast<unsigned int>(a)) |
            (static_cast<unsigned int>(b) << 8) |
            (static_cast<unsigned int>(c) << 16) |
            (static_cast<unsigned int>(d) << 24);
    }

    enum class pixel_format
    {
        none = 0,
        y8 = pixel_format_fourcc("y8  "),
        rgb = pixel_format_fourcc("24RG"),
        bgr = pixel_format_fourcc("24BG"),
        argb = pixel_format_fourcc("ARGB"),
        abgr = pixel_format_fourcc("ABGR"),
        bgra = pixel_format_fourcc("BGRA"),
        rgba = pixel_format_fourcc("RGBA"),
        xrgb = pixel_format_fourcc("XRGB"),
        xbgr = pixel_format_fourcc("XBGR"),
        bgrx = pixel_format_fourcc("BGRX"),
        rgbx = pixel_format_fourcc("RGBX"),

        i420 = pixel_format_fourcc("I420"),
        i422 = pixel_format_fourcc("I422"),
        i444 = pixel_format_fourcc("I444"),
        nv12 = pixel_format_fourcc("NV12"),
        nv21 = pixel_format_fourcc("NV21"),

        yu12 = pixel_format_fourcc("YU12"),
        yuv2 = pixel_format_fourcc("YUV2"),
        yuy2 = pixel_format_fourcc("YUY2"),
        yuyv = pixel_format_fourcc("YUYV"),
        yv12 = pixel_format_fourcc("YV12"),
        yvyu = pixel_format_fourcc("YVYU"),
        uyuv = pixel_format_fourcc("UYUV"),
        uyvy = pixel_format_fourcc("UYVY"),
        vyvu = pixel_format_fourcc("VYVU"),
        vuvy = pixel_format_fourcc("VUVY"),

        png = pixel_format_fourcc("PNG "),
        jpeg = pixel_format_fourcc("JPEG"),
        mjpg = pixel_format_fourcc("MJPG"),
        h264 = pixel_format_fourcc("H264"),
        h265 = pixel_format_fourcc("H265"),
        hevc = pixel_format_fourcc("HEVC"),
        x264 = pixel_format_fourcc("x264"),
        y420 = pixel_format_fourcc("y420"),

        raw = pixel_format_fourcc("raw "),
        fourcc_i420 = pixel_format_fourcc("I420"),
        fourcc_i422 = pixel_format_fourcc("I422"),
        fourcc_i444 = pixel_format_fourcc("I444"),
        fourcc_i400 = pixel_format_fourcc("I400"),
        fourcc_nv21 = pixel_format_fourcc("NV21"),
        fourcc_nv12 = pixel_format_fourcc("NV12"),
        fourcc_yuy2 = pixel_format_fourcc("YUY2"),
        fourcc_uyvy = pixel_format_fourcc("UYVY"),
        fourcc_m420 = pixel_format_fourcc("M420"),
        fourcc_argb = pixel_format_fourcc("ARGB"),
        fourcc_abgr = pixel_format_fourcc("ABGR"),
        fourcc_bgra = pixel_format_fourcc("BGRA"),
        fourcc_rgba = pixel_format_fourcc("RGBA"),
        fourcc_ar30 = pixel_format_fourcc("AR30"), // 10 bit per channel. 2101010.
        fourcc_ab30 = pixel_format_fourcc("AB30"), // ABGR version of 10 bit
        fourcc_rgbp = pixel_format_fourcc("RGBP"), // rgb565 LE.
        fourcc_rgb0 = pixel_format_fourcc("RGB0"), // argb1555 LE.
        fourcc_r444 = pixel_format_fourcc("R444"), // argb4444 LE.
        fourcc_mjpg = pixel_format_fourcc("MJPG"),
        fourcc_yv12 = pixel_format_fourcc("YV12"),
        fourcc_yv16 = pixel_format_fourcc("YV16"),
        fourcc_yv24 = pixel_format_fourcc("YV24"),
        fourcc_yu12 = pixel_format_fourcc("YU12"),
        fourcc_yu16 = pixel_format_fourcc("YU16"), // Alias for I422.
        fourcc_yu24 = pixel_format_fourcc("YU24"), // Alias for I444.
        fourcc_j420 = pixel_format_fourcc("J420"),
        fourcc_j400 = pixel_format_fourcc("J400"),
        fourcc_h420 = pixel_format_fourcc("H420"),
        fourcc_h422 = pixel_format_fourcc("H422"),
        fourcc_iyuv = pixel_format_fourcc("IYUV"), // Alias for I420.
        fourcc_yuyv = pixel_format_fourcc("YUYV"), // Alias for YUY2.
        fourcc_yuvs = pixel_format_fourcc("YUVS"), // Alias for YUY2 on Mac.
        fourcc_hdyc = pixel_format_fourcc("HDYV"), // Alias for UYVY.
        fourcc_jpeg = pixel_format_fourcc("JPEG"), // Alias for JPEG.
        fourcc_dmb1 = pixel_format_fourcc("DMB1"), // Alias for MJPG on Mac.
        fourcc_ba81 = pixel_format_fourcc("BA81"), // Alias for BGGR.
        fourcc_bgb3 = pixel_format_fourcc("BGB3"), // Alias for RAW.
        fourcc_bgr3 = pixel_format_fourcc("BGR3"), // Alias for 24BG.
        fourcc_cm32 = pixel_format_fourcc(0, 0, 0, 32), // Alias for BGRA kCMPixelFormat_32ARGB
        fourcc_cm24 = pixel_format_fourcc(0, 0, 0, 24), // Alias for RAW kCMPixelFormat_24RGB
        fourcc_l555 = pixel_format_fourcc("L555"), // Alias for RGBO.
        fourcc_l565 = pixel_format_fourcc("L565"), // Alias for RGBP.
        fourcc_l551 = pixel_format_fourcc("L551"), // Alias for RGB0.
    };

    enum class sample_format
    {
        none = 0,
        uint8,
        sint8,
        uint16,
        sint16,
        uint32,
        sint32,
        float32,
        float64,
    };
}
