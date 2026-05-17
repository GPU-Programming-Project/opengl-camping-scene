#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform bool blurEnabled;

void main()
{
    if (blurEnabled) {
        float offset = 1.0 / 800.0;
        vec2 offsets[9] = vec2[](
            vec2(-offset,  offset),  // top-left
            vec2( 0.0f,    offset),  // top-center
            vec2( offset,  offset),  // top-right
            vec2(-offset,  0.0f),    // center-left
            vec2( 0.0f,    0.0f),    // center-center
            vec2( offset,  0.0f),    // center-right
            vec2(-offset, -offset),  // bottom-left
            vec2( 0.0f,   -offset),  // bottom-center
            vec2( offset, -offset)   // bottom-right
        );

        // Gaussian kernel
        float kernel[9] = float[](
            1.0/16, 2.0/16, 1.0/16,
            2.0/16, 4.0/16, 2.0/16,
            1.0/16, 2.0/16, 1.0/16
        );

        vec3 col = vec3(0.0);
        for (int i = 0; i < 9; i++)
            col += vec3(texture(screenTexture, TexCoords + offsets[i])) * kernel[i];

        FragColor = vec4(col, 1.0);
    } else {
        FragColor = texture(screenTexture, TexCoords);
    }
}
