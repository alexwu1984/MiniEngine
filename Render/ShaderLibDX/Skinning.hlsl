#pragma pack_matrix(row_major)
#ifdef ID_SKINNING_MATRICES

struct Matrix2
{
    matrix Current;
    matrix Previous;
};

static const int MAX_MATRICES = 512;

cbuffer cbPerSkeleton : register(b2)
{
    Matrix2 PerSkeleton_u_ModelMatrix[MAX_MATRICES];
};

uint ClampJointIndex(float j)
{
    return min(uint(j), MAX_MATRICES - 1);
}

matrix GetCurrentSkinningMatrix(float4 Weights, float4 JointsF)
{
    uint j0 = ClampJointIndex(JointsF.x);
    uint j1 = ClampJointIndex(JointsF.y);
    uint j2 = ClampJointIndex(JointsF.z);
    uint j3 = ClampJointIndex(JointsF.w);
    matrix skinningMatrix =
        Weights.x * PerSkeleton_u_ModelMatrix[j0].Current +
        Weights.y * PerSkeleton_u_ModelMatrix[j1].Current +
        Weights.z * PerSkeleton_u_ModelMatrix[j2].Current +
        Weights.w * PerSkeleton_u_ModelMatrix[j3].Current;
    return skinningMatrix;
}

matrix GetPreviousSkinningMatrix(float4 Weights, float4 JointsF)
{
    uint j0 = ClampJointIndex(JointsF.x);
    uint j1 = ClampJointIndex(JointsF.y);
    uint j2 = ClampJointIndex(JointsF.z);
    uint j3 = ClampJointIndex(JointsF.w);
    matrix skinningMatrix =
        Weights.x * PerSkeleton_u_ModelMatrix[j0].Previous +
        Weights.y * PerSkeleton_u_ModelMatrix[j1].Previous +
        Weights.z * PerSkeleton_u_ModelMatrix[j2].Previous +
        Weights.w * PerSkeleton_u_ModelMatrix[j3].Previous;
    return skinningMatrix;
}

#endif 