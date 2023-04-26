#pragma pack_matrix(row_major)
#ifdef ID_SKINNING_MATRICES

struct Matrix2
{
    matrix Current;
    matrix Previous;
};

static const int MAX_MATRICES = 200; //the max

cbuffer cbPerSkeleton : register(b2)
{
    Matrix2 PerSkeleton_u_ModelMatrix[MAX_MATRICES];
};

matrix GetCurrentSkinningMatrix(float4 Weights, uint4 Joints)
{
    matrix skinningMatrix =
        Weights.x * PerSkeleton_u_ModelMatrix[Joints.x].Current +
        Weights.y * PerSkeleton_u_ModelMatrix[Joints.y].Current +
        Weights.z * PerSkeleton_u_ModelMatrix[Joints.z].Current +
        Weights.w * PerSkeleton_u_ModelMatrix[Joints.w].Current;
    return skinningMatrix;
}

matrix GetPreviousSkinningMatrix(float4 Weights, uint4 Joints)
{
    matrix skinningMatrix =
        Weights.x * PerSkeleton_u_ModelMatrix[Joints.x].Previous +
        Weights.y * PerSkeleton_u_ModelMatrix[Joints.y].Previous +
        Weights.z * PerSkeleton_u_ModelMatrix[Joints.z].Previous +
        Weights.w * PerSkeleton_u_ModelMatrix[Joints.w].Previous;
    return skinningMatrix;
}

#endif 