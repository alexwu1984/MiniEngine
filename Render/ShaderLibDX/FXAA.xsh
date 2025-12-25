#ifndef _XIN_SHADER_FXAA_H_
#define _XIN_SHADER_FXAA_H_ 1


#ifndef XIN_FXAA_QUALITY
#define XIN_FXAA_QUALITY 1
#endif

#ifndef XIN_FXAA_CONSOLE
#define XIN_FXAA_CONSOLE 0
#endif

#ifndef XIN_FXAA_DEBUG
#define XIN_FXAA_DEBUG 0
#endif

#ifndef XIN_FXAA_DEBUG_NORMAL
#define XIN_FXAA_DEBUG_NORMAL 0
#endif

#ifndef XIN_FXAA_QUALITY_LEVEL
#define XIN_FXAA_QUALITY_LEVEL 1
#endif

static const int FXAA_QUALITY_SAMPLE_COUNTS[17] = { 3, 4, 5, 6, 7, 8,  3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, };
static const float FXAA_QUALITY_SAMPLE_STEPS[17][12] =
{
    // medium dither
    /* Level  1 */{ 1.5, 4.5, 16.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  2 */{ 1.0, 2.5, 5.5, 17.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  3 */{ 1.0, 2.5, 4.5, 8.5, 20.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  4 */{ 1.0, 2.5, 4.5, 6.5, 10.5, 22.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  5 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 12.5, 24.5, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  6 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 10.5, 14.5, 26.5, 0x0, 0x0, 0x0, 0x0 },
	// Low dither
    /* Level  7 */{ 1.5, 3.5, 11.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  8 */{ 1.0, 2.5, 4.5, 12.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level  9 */{ 1.0, 2.5, 4.5, 6.5, 14.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level 10 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 16.5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level 11 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 11.5, 19.5, 0x0, 0x0, 0x0, 0x0, 0x0 },
    /* Level 12 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 10.5, 14.5, 22.5, 0x0, 0x0, 0x0, 0x0 },
    /* Level 13 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 10.5, 12.5, 16.5, 24.5, 0x0, 0x0, 0x0 },
    /* Level 14 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 10.5, 12.5, 14.5, 18.5, 26.5, 0x0, 0x0 },
    /* Level 15 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 10.5, 12.5, 14.5, 16.5, 20.5, 28.5, 0x0 },
    /* Level 16 */{ 1.0, 2.5, 4.5, 6.5, 8.5, 10.5, 12.5, 14.5, 16.5, 18.5, 22.5, 30.5 },
    // extreme quality
    /* Level 17 */{ 1.0, 2.0, 3.0, 4.0, 5.0, 6.5, 8.5, 10.5, 12.5, 14.5, 18.5, 26.5 },
};

float CalcSubpixShift(float AABlendFactor, float FXAASubpixel)
{
    return saturate(pow(smoothstep(0, 1, AABlendFactor), 2) * FXAASubpixel * 2);
}

float CalcSubpixShiftQuality(float AABlendFactor, float FXAASubpixel)
{
    float SubpixD = 3.0 - 2.0 * AABlendFactor;
    float SubpixE = AABlendFactor * AABlendFactor;
    float SubpixF = SubpixD * SubpixE;
    float SubpixG = SubpixF * SubpixF;
    float SubpixH = SubpixG * FXAASubpixel;
    return SubpixH;

}

#endif // _XIN_SHADER_FXAA_H_
