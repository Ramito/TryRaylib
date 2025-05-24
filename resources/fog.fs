#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables

#define     MAX_LIGHTS              4
#define     LIGHT_DIRECTIONAL       0
#define     LIGHT_POINT             1

struct MaterialProperty {
    vec3 color;
    int useSampler;
    sampler2D sampler;
};

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};

// Input lighting values
uniform vec4 ambient;
uniform vec3 viewPos;
uniform float fogDensity;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);


    vec3 tanX = dFdx(fragPosition);
    vec3 tanY = dFdy(fragPosition);
    vec3 normal = normalize(cross(tanX, tanY));
    //vec3 normal = normalize(fragNormal);

    const vec3 light = normalize(vec3(0.2, 1.0, 0.2));
    vec3 lightDot = vec3(clamp(dot(normal, light), 0.0, 1.0));

    finalColor = (texelColor*((colDiffuse + vec4(specular,1))*vec4(lightDot, 1.0)));
    finalColor += texelColor*(ambient/10.0);

    // Gamma correction
    finalColor = pow(finalColor, vec4(1.0/2.2));

    // Fog calculation
    float dist = length(viewPos - fragPosition);

    // these could be parameters...
    const vec4 fogColor = vec4(0.15, 0.15, 0.2, 1.0);

    // Exponential fog
    float fogFactor = 1.0/exp((dist*fogDensity)*(dist*fogDensity));

    fogFactor = clamp(fogFactor, 0.0, 1.0);

    finalColor = mix(fogColor, finalColor, fogFactor);
}
