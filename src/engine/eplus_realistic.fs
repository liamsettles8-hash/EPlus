#version 330

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 fragColor;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform float uTime;

out vec4 finalColor;

float saturate(float x) { return clamp(x, 0.0, 1.0); }

vec3 aces(vec3 x) {
    x = max(x, vec3(0.0));
    return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}

void main() {
    vec3 n = normalize(fragNormal);
    vec3 l = normalize(-uSunDir);
    vec3 v = normalize(uCameraPos - fragPosition);
    vec3 h = normalize(l + v);

    float sun = saturate(dot(n, l));
    float sky = saturate(n.y * 0.5 + 0.5);

    // Soft Minecraft-like daylight with more realistic falloff.
    vec3 base = fragColor.rgb;
    vec3 ambient = base * mix(0.10, 0.28, sky);
    vec3 diffuse = base * sun * 0.78;
    float spec = pow(saturate(dot(n, h)), 48.0) * 0.12;

    // Gentle distance fog for depth.
    float distanceToCamera = length(uCameraPos - fragPosition);
    float fog = smoothstep(45.0, 180.0, distanceToCamera);
    vec3 fogColor = vec3(0.43, 0.56, 0.68);

    vec3 color = ambient + diffuse + vec3(spec);
    color = mix(color, fogColor, fog);
    color = aces(color * 1.15);
    color = pow(color, vec3(1.0 / 2.2));

    finalColor = vec4(color, fragColor.a);
}
