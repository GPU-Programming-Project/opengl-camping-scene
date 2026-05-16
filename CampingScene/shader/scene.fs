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
uniform bool isEmissive;

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
    vec3 ambient = 0.55 * baseColor;

    // Diffuse
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * lightColor * baseColor;

    // Specular
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    vec3 specular   = 0.08 * spec * lightColor;

    if (isEmissive) {
        FragColor = vec4(baseColor, alpha);
    } else {
        FragColor = vec4(ambient + diffuse + specular, alpha);
    }
}
