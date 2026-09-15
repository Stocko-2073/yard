@block camera
// Camera at yard eye height. North is -Z, east is +X.
mat3 camera_basis(vec2 look) {
    float cy = cos(look.x), sy = sin(look.x);
    float cp = cos(look.y), sp = sin(look.y);
    return mat3(vec3(cy, 0, sy), vec3(-sy*sp, cp, cy*sp),
                vec3(sy*cp, sp, -cy*cp));
}
@end

@block lighting
const float PI = 3.14159265;
// Kilometres, sea-level scattering coefficients, exponential scale heights.
const float EARTH = 6360.0;
const float TOP = 6460.0;
const vec3 BETA_R = vec3(0.0058, 0.0135, 0.0331);
const vec3 BETA_M = vec3(0.004);
float atmosphere_exit(vec3 p, vec3 d) {
    float b = dot(p, d);
    return -b + sqrt(max(0.0, b*b - dot(p,p) + TOP*TOP));
}
vec2 density(vec3 p) {
    float h = max(0.0, length(p) - EARTH);
    return exp(-h / vec2(8.0, 1.2));
}
vec3 extinction(vec2 depth) {
    return exp(-BETA_R * depth.x - BETA_M * 1.1 * depth.y);
}
vec3 sun_transmittance(vec3 p, vec3 sun) {
    float b = dot(p, sun);
    if (b < 0.0 && b*b > dot(p,p) - EARTH*EARTH) return vec3(0);
    float step_len = atmosphere_exit(p, sun) / 8.0;
    vec2 depth = vec2(0);
    for (int j = 0; j < 8; ++j) {
        depth += density(p + sun * (float(j) + 0.5) * step_len) * step_len;
    }
    return extinction(depth);
}
vec3 atmosphere(vec3 ray, vec3 sun, vec3 night) {
    vec3 origin = vec3(0, EARTH + 0.0025, 0);
    // The ground pass hides downward rays; clamp the distant haze to the horizon.
    ray = normalize(vec3(ray.x, max(ray.y, 0.001), ray.z));
    float step_len = atmosphere_exit(origin, ray) / 16.0;
    vec2 depth = vec2(0);
    vec3 sum_r = vec3(0), sum_m = vec3(0);
    for (int i = 0; i < 16; ++i) {
        vec3 p = origin + ray * (float(i) + 0.5) * step_len;
        vec2 local_depth = density(p) * step_len;
        vec3 transmission = extinction(depth + local_depth * 0.5) * sun_transmittance(p, sun);
        sum_r += transmission * local_depth.x;
        sum_m += transmission * local_depth.y;
        depth += local_depth;
    }
    float mu = dot(ray, sun);
    float rayleigh = 3.0 / (16.0 * PI) * (1.0 + mu*mu);
    float g = 0.8;
    float mie = (1.0-g*g) / (4.0*PI*pow(max(0.001, 1.0+g*g-2.0*g*mu), 1.5));
    vec3 sky = 20.0 * (BETA_R * sum_r * rayleigh + BETA_M * sum_m * mie);
    // Solar angular radius ~0.266 degrees, antialiased at the pixel footprint.
    float distance_to_sun = length(ray - sun);
    float edge = max(fwidth(distance_to_sun), 0.0001);
    float disk = 1.0 - smoothstep(0.00465-edge, 0.00465+edge, distance_to_sun);
    sky += 40.0 * disk * extinction(depth) * smoothstep(-0.009, 0.0, sun.y);
    return sky + night;
}
vec3 display_color(vec3 linear_color, float exposure) {
    // Integrate shutter and gain in linear space before tone mapping and 8-bit output.
    vec3 x = max(linear_color * exposure, vec3(0));
    vec3 mapped = clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14), 0.0, 1.0);
    return mix(12.92*mapped, 1.055*pow(mapped, vec3(1.0/2.4))-0.055,
               step(vec3(0.0031308), mapped));
}
vec3 surface_light(vec3 albedo, vec3 normal, vec3 sun, vec3 sunlight, float visibility, vec3 night) {
    float day = smoothstep(-0.12, 0.18, sun.y);
    // Hemispherical ambient approximation; direct irradiance uses atmospheric extinction.
    // Uniform night hemisphere: irradiance = PI * radiance, in render units.
    vec3 ambient = vec3(0.10, 0.17, 0.28)*day + PI*night;
    ambient *= mix(0.3, 1.0, normal.y*0.5+0.5);
    return albedo * (ambient + sunlight * max(dot(normal, sun), 0.0) * visibility);
}
@end

@block scene_view
layout(binding=0) uniform vs_params {
    vec4 view; // yaw, pitch, aspect, reserved
    vec4 lens; // projection scale, terrain column count, grass instance stride
    vec4 camera_position;
    vec4 grass_lod; // enabled, fade start/end metres (horizontal), density scale
};
@end

@vs vs
@glsl_options fixup_clipspace
@include_block scene_view
@include_block camera
in vec3 position;
in vec3 normal;
out vec3 world_normal;
out vec3 world_position;
out float surface_depth;
void main() {
    world_position = position;
    vec3 p = transpose(camera_basis(view.xy)) * (world_position - camera_position.xyz);
    // Forward-positive camera coordinates; depth [0, 1].
    gl_Position = vec4(lens.x*p.x/view.z, lens.x*p.y,
                      (1000.0/999.9)*p.z - 100.0/999.9, p.z);
    world_normal = normal;
    surface_depth = p.z;
}
@end

@block scene_light
layout(binding=1) uniform light_params {
    vec4 sun_direction;
    vec4 sun_color;
    vec4 night_radiance;
    vec4 camera_exposure; // shutter * gain / reference exposure
};
@end

@fs fs
@include_block scene_light
@include_block lighting
in vec3 world_normal;
in vec3 world_position;
in float surface_depth;
out vec4 frag_color;
void main() {
    vec3 n = normalize(world_normal);
    // #56341B is an sRGB albedo; convert to linear before lighting.
    vec3 albedo = pow((vec3(86,52,27)/255.0+0.055)/1.055, vec3(2.4));
    frag_color = vec4(surface_light(albedo,n,sun_direction.xyz,
                                   sun_color.xyz,1.0,night_radiance.xyz),surface_depth);
}
@end

@fs object_fs
@include_block scene_light
@include_block lighting
in vec3 world_normal;
in vec3 world_position;
in float surface_depth;
out vec4 frag_color;
void main() {
    frag_color=vec4(surface_light(vec3(0.55),normalize(world_normal),
        sun_direction.xyz,sun_color.xyz,1.0,night_radiance.xyz),surface_depth);
}
@end
@program object vs object_fs

@vs sky_vs
@glsl_options fixup_clipspace
in vec2 position;
out vec2 screen_position;
void main() {
    screen_position = position;
    gl_Position = vec4(position, 0, 1);
}
@end

@fs sky_fs
layout(binding=2) uniform sky_params {
    vec4 sky_view;
    vec4 sky_sun;
    vec4 sky_sun_color;
    vec4 sky_night_radiance;
    vec4 sky_camera_exposure;
    vec4 sky_lens;
    vec4 sky_camera_position;
    vec4 sky_moon; // direction and angular radius
    vec4 moon_sun; // Moon-to-Sun direction, in the same world frame
    vec4 moon_north;
};
@include_block camera
@include_block lighting
in vec2 screen_position;
out vec4 frag_color;
float lunar_noise(vec2 p) {
    vec2 cell = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    vec4 ids = vec4(dot(cell,vec2(127.1,311.7)), dot(cell+vec2(1,0),vec2(127.1,311.7)),
                    dot(cell+vec2(0,1),vec2(127.1,311.7)), dot(cell+vec2(1,1),vec2(127.1,311.7)));
    vec4 values = fract(sin(ids)*43758.5453);
    return mix(mix(values.x,values.y,f.x), mix(values.z,values.w,f.x), f.y);
}
vec3 lunar_disk(vec3 ray) {
    vec3 center = sky_moon.xyz;
    float alignment = dot(ray, center);
    vec3 tangent = (ray-center*alignment)/sin(sky_moon.w);
    float r = length(tangent);
    float edge = max(fwidth(r), 0.001);
    float coverage = (1.0-smoothstep(1.0-edge, 1.0+edge, r))*step(0.0, alignment);
    if (coverage <= 0.0 || ray.y <= 0.0) return vec3(0);
    // Visible hemisphere normal; illumination from the ephemeris gives both
    // the terminator shape and its orientation relative to the local horizon.
    tangent /= max(1.0, r);
    float facing = sqrt(max(0.0, 1.0-dot(tangent,tangent)));
    vec3 normal = tangent-center*facing;
    float mu = max(dot(normal, moon_sun.xyz), 0.0);
    // Lommel-Seeliger-like response avoids a Lambert sphere's excessive limb darkening.
    float lit = 2.0*mu/max(mu+facing, 0.0001);
    vec3 up = normalize(moon_north.xyz-center*dot(moon_north.xyz,center));
    vec3 right = normalize(cross(center,up));
    vec2 uv = vec2(dot(normal,right),dot(normal,up));
    // Restrained procedural albedo variation, not a surveyed lunar surface map.
    float coarse = lunar_noise(uv*3.7+vec2(4.3,2.1));
    float detail = lunar_noise(uv*17.0)*0.6+lunar_noise(uv*43.0)*0.4;
    float maria = smoothstep(0.44, 0.64, coarse+0.12*(detail-0.5));
    float albedo = 0.78-0.28*maria+0.07*(detail-0.5);
    vec3 transmission = sun_transmittance(vec3(0,EARTH+0.0025,0), ray);
    return vec3(0.72,0.70,0.66)*albedo*lit*coverage*transmission;
}
void main() {
    vec3 ray = normalize(camera_basis(sky_view.xy) *
                        vec3(screen_position.x*sky_view.z, screen_position.y, sky_lens.x));
    vec3 sky = atmosphere(ray, sky_sun.xyz, sky_night_radiance.xyz);
    if (ray.y > 0.0) sky += lunar_disk(ray);
    vec3 result = sky;
    float scene_depth=1000.0;
    if (ray.y < -0.0001) {
        float distance_to_ground = -sky_camera_position.y/ray.y;
        vec3 p = sky_camera_position.xyz + ray*distance_to_ground;
        scene_depth=min(1000.0,distance_to_ground*dot(ray,camera_basis(sky_view.xy)[2]));
        // Subtle metre grid makes changing shadow length easy to judge.
        vec2 grid_dist = abs(fract(p.xz-0.5)-0.5);
        vec2 footprint = max(fwidth(p.xz), vec2(0.001));
        float grid = 1.0-min(smoothstep(0.015, 0.015+footprint.x, grid_dist.x),
                             smoothstep(0.015, 0.015+footprint.y, grid_dist.y));
        grid *= 1.0-smoothstep(20.0, 60.0, distance_to_ground);
        vec3 ground = surface_light(vec3(0.16,0.20,0.10)*(1.0-0.15*grid),
                                   vec3(0,1,0), sky_sun.xyz, sky_sun_color.xyz,
                                   1.0, sky_night_radiance.xyz);
        result = mix(ground, sky, 1.0-exp(-distance_to_ground*0.0015));
    }
    frag_color = vec4(result,scene_depth);
}
@end

@program cube vs fs
@program sky sky_vs sky_fs

@vs preview_vs
@glsl_options fixup_clipspace
in vec2 position;
out vec2 preview_uv;
void main() {
    gl_Position = vec4(position, 0, 1);
    // Render-target texture origin differs between GL and the other backends.
    preview_uv = position*0.5+0.5;
#ifndef SOKOL_GLSL
    preview_uv.y = 1.0-preview_uv.y;
#endif
}
@end

@fs preview_fs
layout(binding=3) uniform preview_params {
    vec4 preview_settings; // digital inspection zoom
};
layout(binding=0) uniform texture2D camera_image;
layout(binding=0) uniform sampler camera_sampler;
in vec2 preview_uv;
out vec4 frag_color;
void main() {
    vec2 uv = (preview_uv-0.5)/preview_settings.x+0.5;
    frag_color = texture(sampler2D(camera_image, camera_sampler), uv);
}
@end

@program preview preview_vs preview_fs

@vs grass_vs
@glsl_options fixup_clipspace
@include_block scene_view
@include_block camera
layout(binding=5) uniform grass_region_params {
    vec4 grass_region; // voxel origin X/Z, region width, reserved
};
in vec2 blade;
in float root_height;
out vec3 grass_normal;
out float grass_depth;
uint grass_hash(uint seed) {
    uint h = seed*747796405u+2891336453u;
    h = ((h >> ((h >> 28u)+4u)) ^ h)*277803737u;
    return (h >> 22u) ^ h;
}
void main() {
    int side = int(lens.y);
    uint local_id = uint(gl_InstanceIndex)*uint(lens.z);
    int width = int(grass_region.z);
    ivec2 grid = ivec2(grass_region.xy)+ivec2(int(local_id)%width,int(local_id)/width);
    uint id = uint(grid.y*side+grid.x);
    vec2 cell = vec2(grid);
    // Separate hashes keep placement stable and independent of azimuth.
    uint h = grass_hash(id);
    vec2 axis = normalize(vec2(float(h & 65535u),float(h >> 16u))-vec2(32767.5));
    uint placement = grass_hash(id ^ 0x9e3779b9u);
    vec2 offset = (vec2(float(placement & 65535u),float(placement >> 16u))+0.5)/65536.0;
    vec3 root = vec3((cell.x+offset.x-lens.y*0.5)*0.01, root_height*0.01,
                     (cell.y+offset.y-lens.y*0.5)*0.01);
    float lod=grass_lod.x*smoothstep(grass_lod.y,grass_lod.z,length(root.xz-camera_position.xz));
    float blade_width=blade.x*(1.0-lod);
    vec3 world = root+vec3(axis.x*blade_width,blade.y,axis.y*blade_width);
    vec3 p = transpose(camera_basis(view.xy))*(world-camera_position.xyz);
    gl_Position = vec4(lens.x*p.x/view.z,lens.x*p.y,
                       (1000.0/999.9)*p.z-100.0/999.9,p.z);
    grass_normal = vec3(-axis.y,0,axis.x);
    grass_depth=p.z;
    if (lod>=1.0) gl_Position=vec4(2,2,2,1);
}
@end

@fs grass_fs
@include_block scene_light
@include_block lighting
in vec3 grass_normal;
in float grass_depth;
out vec4 frag_color;
void main() {
    vec3 n = normalize(grass_normal);
    // Thin, two-sided leaf: either face can receive the directional light.
    if (dot(n,sun_direction.xyz) < 0.0) n = -n;
    vec3 albedo = vec3(0.075,0.16,0.025);
    frag_color = vec4(surface_light(albedo,n,sun_direction.xyz,
                        sun_color.xyz,1.0,night_radiance.xyz),grass_depth);
}
@end

@program grass grass_vs grass_fs

@fs downsample_fs
@include_block lighting
@include_block scene_light
layout(binding=4) uniform downsample_params {
    vec4 output_size; // final sensor width and height
};
layout(binding=1) uniform texture2D source_image;
layout(binding=1) uniform sampler source_sampler;
in vec2 preview_uv;
out vec4 frag_color;
vec3 decode_srgb(vec3 c) {
    return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));
}
vec3 encode_srgb(vec3 c) {
    return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c));
}
void main() {
    ivec2 dims = textureSize(sampler2D(source_image,source_sampler),0);
    ivec2 pixel = clamp(ivec2(preview_uv*output_size.xy),ivec2(0),ivec2(output_size.xy)-1);
    // Exact box overlap in source-pixel units; handles fractional sqrt(8) scale.
    vec2 ratio = vec2(dims)/output_size.xy;
    vec2 lo = vec2(pixel)*ratio, hi = vec2(pixel+1)*ratio;
    ivec2 start = ivec2(floor(lo));
    vec3 sum = vec3(0);
    float weight_sum = 0;
    // A <= sqrt(8)+rounding pixel footprint overlaps at most 4x4 source pixels.
    for (int y=0; y<4; ++y) for (int x=0; x<4; ++x) {
        ivec2 p = start+ivec2(x,y);
        vec2 overlap = max(vec2(0),min(hi,vec2(p+1))-max(lo,vec2(p)));
        float weight = overlap.x*overlap.y;
        if (weight > 0) {
            vec3 c = texelFetch(sampler2D(source_image,source_sampler),clamp(p,ivec2(0),dims-1),0).rgb;
            sum += decode_srgb(display_color(c,camera_exposure.x))*weight;
            weight_sum += weight;
        }
    }
    frag_color = vec4(encode_srgb(sum/weight_sum),1);
}
@end

@program downsample preview_vs downsample_fs


@fs volume_fs
@include_block camera
layout(binding=7) uniform volume_camera_params {
    vec4 volume_view;
    vec4 volume_lens;
    vec4 volume_camera_position;
    vec4 volume_grass_lod;
};
@include_block scene_light
@include_block lighting
layout(binding=6) uniform volume_params {
    vec4 volume_field; // first sample X/Z, sample spacing, size, global slope bound
    vec4 volume_bounds; // min root height, max tip height, yard half-width, reserved
};
layout(binding=2) uniform texture2D scene_image;
layout(binding=2) uniform sampler scene_sampler;
layout(binding=3) uniform texture2D height_image;
layout(binding=3) uniform sampler height_sampler;
@image_sample_type height_image unfilterable_float
@sampler_type height_sampler nonfiltering
in vec2 preview_uv;
out vec4 frag_color;
float root_height_at(vec2 xz) {
    vec2 grid=clamp((xz-volume_field.x)/volume_field.y,vec2(0),vec2(volume_field.z-1.0));
    ivec2 cell=ivec2(floor(grid));
    ivec2 last=ivec2(int(volume_field.z)-1);
    vec2 f=fract(grid);
    float a=texelFetch(sampler2D(height_image,height_sampler),cell,0).r;
    float b=texelFetch(sampler2D(height_image,height_sampler),min(cell+ivec2(1,0),last),0).r;
    float c=texelFetch(sampler2D(height_image,height_sampler),min(cell+ivec2(0,1),last),0).r;
    float d=texelFetch(sampler2D(height_image,height_sampler),min(cell+ivec2(1,1),last),0).r;
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
bool clip_axis(float origin,float direction,float lo,float hi,inout float enter,inout float leave) {
    if (abs(direction)<1e-7) return origin>=lo && origin<=hi;
    float a=(lo-origin)/direction,b=(hi-origin)/direction;
    enter=max(enter,min(a,b)); leave=min(leave,max(a,b));
    return leave>enter;
}
void main() {
    ivec2 dims=textureSize(sampler2D(scene_image,scene_sampler),0);
    ivec2 pixel=clamp(ivec2(preview_uv*vec2(dims)),ivec2(0),dims-1);
    vec4 scene=texelFetch(sampler2D(scene_image,scene_sampler),pixel,0);
    frag_color=scene;
    vec2 ndc=preview_uv*2.0-1.0;
#ifndef SOKOL_GLSL
    ndc.y=-ndc.y;
#endif
    vec3 camera_ray=normalize(vec3(ndc.x*volume_view.z,ndc.y,volume_lens.x));
    vec3 ray=camera_basis(volume_view.xy)*camera_ray;
    float horizontal=length(ray.xz);
    if (horizontal<1e-6) return; // Upright zero-thickness blades have no top-down area.
    float enter=max(0.1/camera_ray.z,volume_grass_lod.y/horizontal);
    float leave=min(1000.0,scene.a)/camera_ray.z;
    if (!clip_axis(volume_camera_position.x,ray.x,-volume_bounds.z,volume_bounds.z,enter,leave) ||
        !clip_axis(volume_camera_position.z,ray.z,-volume_bounds.z,volume_bounds.z,enter,leave) ||
        !clip_axis(volume_camera_position.y,ray.y,volume_bounds.x,volume_bounds.y,enter,leave)) return;
    // Average the original two-sided upright leaf lighting, weighted by projected area.
    vec3 leaf=vec3(0); float weights=0;
    for (int i=0; i<16; ++i) {
        float angle=(float(i)+0.5)*PI/16.0;
        vec3 n=vec3(cos(angle),0,sin(angle));
        float w=abs(dot(n,ray));
        if (dot(n,sun_direction.xyz)<0) n=-n;
        leaf+=w*surface_light(vec3(0.075,0.16,0.025),n,sun_direction.xyz,sun_color.xyz,1.0,night_radiance.xyz);
        weights+=w;
    }
    leaf/=max(weights,1e-6);
    float t=enter, transmission=1.0;
    float rate=abs(ray.y)+volume_field.w*horizontal;
    // Conservative empty-space steps from the bilinear field's global slope bound.
    // Inside the layer, midpoint integration at <=1 cm steps evaluates Beer-Lambert extinction.
    for (int i=0; i<1024; ++i) {
        if (t>=leave || transmission<0.002) break;
        vec3 p=volume_camera_position.xyz+ray*t;
        float h=p.y-root_height_at(p.xz);
        float gap=max(-h,h-0.05);
        if (gap>0.001) {
            t+=min(leave-t,max(0.001,gap/max(rate,1e-6)*0.9));
            continue;
        }
        float step_length=min(0.01,leave-t);
        vec3 midpoint=volume_camera_position.xyz+ray*(t+0.5*step_length);
        float height=midpoint.y-root_height_at(midpoint.xz);
        if (height>=0.0 && height<0.05) {
            float lod=smoothstep(volume_grass_lod.y,volume_grass_lod.z,length(midpoint.xz-volume_camera_position.xz));
            // 10,000 roots/m² * 5 mm base width * triangular width profile * mean projected azimuth.
            float sigma=50.0*(1.0-height/0.05)*(2.0/PI)*horizontal*lod*volume_grass_lod.w;
            transmission*=exp(-sigma*step_length);
        }
        t+=step_length;
    }
    frag_color=vec4(leaf*(1.0-transmission)+scene.rgb*transmission,scene.a);
}
@end
@program volume preview_vs volume_fs
