#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 VertColor;

struct Material {
    sampler2D texture_diffuse1;
};
uniform Material material;
uniform bool hasTexture;
uniform vec4 baseColorFactor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform bool  isEmissive;
uniform float time;
uniform bool  gammaEnabled;
uniform float meshAlpha;

uniform bool  flashlightOn;
uniform vec3  flashlightPos;
uniform vec3  flashlightDir;
uniform float flashlightCutOff;
uniform float flashlightOuterCutOff;
uniform vec3  flashlightColor;

void main()
{
    vec3 baseColor;
    float alpha = 1.0;

    if (hasTexture) {
        vec4 tex = texture(material.texture_diffuse1, TexCoord);
        baseColor = tex.rgb;
        alpha     = tex.a;
    } else if (VertColor.rgb != vec3(1.0)) {
        baseColor = VertColor.rgb;
        alpha     = VertColor.a;
    } else {
        // use material base color factor (glTF solid color per mesh)
        baseColor = baseColorFactor.rgb;
        alpha     = baseColorFactor.a;
    }

    // Ambient
    vec3 ambient = 0.15 * baseColor;

    // Attenuation
    float distance    = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.007 * distance + 0.0002 * distance * distance);

    // Diffuse
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * lightColor * baseColor * attenuation;

    // Specular
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular   = 0.08 * spec * lightColor * attenuation;


    // Spot Light (손전등)
    vec3 spotContrib = vec3(0.0);
    if (flashlightOn) {
        vec3  flashDir   = normalize(flashlightPos - FragPos);
        float theta      = dot(flashDir, normalize(-flashlightDir));
        float epsilon    = flashlightCutOff - flashlightOuterCutOff;
        float intensity  = clamp((theta - flashlightOuterCutOff) / epsilon, 0.0, 1.0);

        float flashDist  = length(flashlightPos - FragPos);
        float flashAtt   = 1.0 / (1.0 + 0.09 * flashDist + 0.032 * flashDist * flashDist);

        float flashDiff  = max(dot(norm, flashDir), 0.0);
        vec3  flashHalf  = normalize(flashDir + viewDir);
        float flashSpec  = pow(max(dot(norm, flashHalf), 0.0), 32.0);

        spotContrib = (flashDiff * baseColor + 0.3 * flashSpec)
                      * flashlightColor * flashAtt * intensity;
    }

    vec3 result;
    if (isEmissive) {
        float flicker = 0.8 + 0.2 * sin(time * 1.5);
        result = baseColor * flicker;
    } else {
        result = ambient + diffuse + specular + spotContrib;
    }

    if (gammaEnabled) {
        result = pow(result, vec3(1.0 / 1.2));
    }

    FragColor = vec4(result, alpha * meshAlpha);
}
