@vs vs
@glsl_options fixup_clipspace
layout(binding=0) uniform vs_params {
    float angle;
    float aspect;
};
in vec3 position;
in vec3 color;
out vec3 face_color;

void main() {
    float cy = cos(angle), sy = sin(angle);
    float cx = cos(angle * 0.6), sx = sin(angle * 0.6);
    vec3 p = position;
    p = vec3(cy*p.x + sy*p.z, p.y, -sy*p.x + cy*p.z);
    p = vec3(p.x, cx*p.y - sx*p.z, sx*p.y + cx*p.z);
    p.z -= 5.0;
    // Right-handed projection with depth [0, 1]. The GLSL option above
    // converts to [-1, 1] for OpenGL; Metal, D3D11 and WebGPU use [0, 1].
    gl_Position = vec4(1.7320508*p.x/aspect, 1.7320508*p.y,
                       (-100.0/99.9)*p.z - 10.0/99.9, -p.z);
    face_color = color;
}
@end

@fs fs
in vec3 face_color;
out vec4 frag_color;
void main() {
    frag_color = vec4(face_color, 1.0);
}
@end

@program cube vs fs
