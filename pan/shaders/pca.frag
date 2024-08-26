#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform Illuminant {
    float data[128];
} illuminant;

layout(binding = 1) uniform Sensor {
    float x[128];
    float y[128];
    float z[128];
} sensor;

layout(binding = 2) uniform Dimension {
    int rasterX;
    int rasterY;
    int rasterCount;
} dimension;

layout(std430, binding = 3) readonly buffer Raster {
    float data[ ];
} rasters[128];

layout(binding = 4) uniform PCA {
    int componentCount;
    int maxComponents;
} pca;

layout(std430, binding = 5) readonly buffer Vector {
    float data[ ];
} vectors[33];

vec3 computeTristimulus(int pX, int pY) {
    // Compute the scaling factor
    float k = 0.0;
    for (int i = 0; i < dimension.rasterCount; i++) {
        k += illuminant.data[i] * sensor.y[i];
    }
    k = 1.0 / k;

    // Compute tri-stimulus
    float x = 0.0;
    float y = 0.0;
    float z = 0.0;
    for (int d = 0; d < pca.componentCount; d++) {
        float xComponent = 0.0;
        float yComponent = 0.0;
        float zComponent = 0.0;
        float pixelPCA = 0.0;
        for (int i = 0; i < dimension.rasterCount; i++) {
            xComponent += k * illuminant.data[i] * vectors[d].data[i] * sensor.x[i];
            yComponent += k * illuminant.data[i] * vectors[d].data[i] * sensor.y[i];
            zComponent += k * illuminant.data[i] * vectors[d].data[i] * sensor.z[i];
            float reflectance = clamp(rasters[i].data[pY * dimension.rasterX + pX], 0.0, 1.0);
            float mean = vectors[pca.maxComponents].data[i];
            pixelPCA += (reflectance - mean) * vectors[d].data[i];
        }
        x += pixelPCA * xComponent;
        y += pixelPCA * yComponent;
        z += pixelPCA * zComponent;
    }

    return vec3(x, y, z);
}

vec3 XYZToLinearRGB(vec3 xyz) {
    vec3 col_0 = vec3( 3.2410, -0.9692,  0.0556);
    vec3 col_1 = vec3(-1.5374,  1.8760, -0.2040);
    vec3 col_2 = vec3(-0.4986,  0.0416,  1.0570);
    vec3 rgb = col_0 * xyz.x + col_1 * xyz.y + col_2 * xyz.z;
    return clamp(rgb, 0.0, 1.0);
}

void main() {
    int pixelX = int(fragTexCoord.x * float(dimension.rasterX - 1));
    int pixelY = int(fragTexCoord.y * float(dimension.rasterY - 1));

    vec3 xyz = computeTristimulus(pixelX, pixelY);
    vec3 rgb = XYZToLinearRGB(xyz);

    outColor = vec4(rgb, 1.0);
}