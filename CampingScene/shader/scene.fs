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


    vec3 result;
    if (isEmissive) {
        float flicker = 0.8 + 0.2 * sin(time * 1.5);
        result = baseColor * flicker;
    } else {
        result = ambient + diffuse + specular;
    }

    if (gammaEnabled) {
        result = pow(result, vec3(1.0 / 1.2));
    }

    FragColor = vec4(result, alpha);
}
