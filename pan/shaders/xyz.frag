#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

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

layout(location = 0) out vec4 outColor;

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
    for (int i = 0; i < dimension.rasterCount; i++) {
        x += k * illuminant.data[i] * clamp(rasters[i].data[pY * dimension.rasterX + pX], 0.0, 1.0) * sensor.x[i];
        y += k * illuminant.data[i] * clamp(rasters[i].data[pY * dimension.rasterX + pX], 0.0, 1.0) * sensor.y[i];
        z += k * illuminant.data[i] * clamp(rasters[i].data[pY * dimension.rasterX + pX], 0.0, 1.0) * sensor.z[i];
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

vec3 gammaCorrectLinearRGB(vec3 rgb) {
    vec3 sRGB;
    float gamma = 1.0 / 2.4;
    sRGB.r = (rgb.r > 0.00304) ? (1.055 * pow(rgb.r, gamma) - 0.055) : (12.92 * rgb.r);
    sRGB.g = (rgb.g > 0.00304) ? (1.055 * pow(rgb.g, gamma) - 0.055) : (12.92 * rgb.g);
    sRGB.b = (rgb.b > 0.00304) ? (1.055 * pow(rgb.b, gamma) - 0.055) : (12.92 * rgb.b);
    return sRGB;
}

vec3 adjustContrast(vec3 sRGB, float contrast) {
    return 0.5 + contrast * (sRGB - 0.5);
}

void main() {
    int pixelX = int(fragTexCoord.x * float(dimension.rasterX - 1));
    int pixelY = int(fragTexCoord.y * float(dimension.rasterY - 1));

    vec3 xyz = computeTristimulus(pixelX, pixelY);
    vec3 rgb = XYZToLinearRGB(xyz);
    vec3 sRGB = gammaCorrectLinearRGB(rgb);

    outColor = vec4(adjustContrast(sRGB, 1.7), 1.0);
}