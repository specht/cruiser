#include <Gamebuino.h>
Gamebuino gb;

const int32_t PI2 = 411775;
const int32_t PI1 = 205887;

#define MAX_POLYGON_VERTICES 8
#define MAX_JOB_COUNT 3
#define MAX_SHARED_FRUSTUM_PLANES 22
#define MAX_RENDER_ADJACENT_SEGMENTS 8
// #define DEBUG
#define MONITOR_RAM
#define SHOW_FRAME_TIME
#define COLLISION_DETECTION
// #define SHOW_TITLE_SCREEN
// #define ENABLE_STRAFE
//#define ENABLE_MAP
#define ENABLE_SHOOTING
#define MAX_SHOTS 12

#define PREMULTIPLIED_WIDTH 1328
#define PREMULTIPLIED_HEIGHT 752

#ifndef PORT_ENABLED
#define SUB_PIXEL_ACCURACY
#endif

#define FIXED_POINT_SCALE 4

#ifndef LINE_COORDINATE_TYPE
#define LINE_COORDINATE_TYPE int
#endif

#ifndef LOG_ALREADY_DEFINED
#define LOG
#define draw_pixel(x, y)
#endif

#define ROLL_SHIP
#define WOBBLE_SHIP
#define CLIP_TO_FRUSTUM
#ifdef ENABLE_MAP
    #define MIN_MAP_SCALE (1L << 16)
    #define MAX_MAP_SCALE (12L << 16)
#endif

int32_t current_frame_start_millis = 0;
int32_t last_shot_frame_millis = -10000;
uint32_t last_micros = 0;
uint32_t micros_per_frame = 0;
#ifdef ENABLE_MAP
    bool map_mode;
    int32_t map_scale;
    int32_t map_dx, map_dy;
#endif
#ifdef DEBUG
int faces_touched = 0;
int faces_drawn = 0;
int max_frustum_planes = 0;
int max_polygon_vertices = 0;
int segments_drawn = 0;
#endif

int32_t lsin(int32_t a);
// save 32 bytes of code by not defining a separate cosine function... heh!
#define lcos(x) lsin((x) + 102943)
int32_t lsqrt(int32_t a);

byte SCREEN_RESOLUTION[2] = {LCDWIDTH, LCDHEIGHT};

struct vec3d_16
{
    union {
        // minimize code size by looping through coordinates
        int16_t v[3];
        struct {
            int16_t x, y, z;
        };
    };
    
    vec3d_16()
        : x(0), y(0), z(0)
    {}
    
    vec3d_16(int16_t _x, int16_t _y, int16_t _z)
        : x(_x), y(_y), z(_z)
    {}
};

struct vec3d
{
    union {
        // minimize code size by looping through coordinates
        int32_t v[3];
        struct {
            int32_t x, y, z;
        };
    };

    vec3d()
        : x(0), y(0), z(0)
    {}

    vec3d(int32_t _x, int32_t _y, int32_t _z)
        : x(_x), y(_y), z(_z)
    {}

    vec3d_16 divby256()
    {
        return vec3d_16(x >> 8, y >> 8, z >> 8);
    }

    vec3d operator +(const vec3d& other)
    {
        return vec3d(x + other.x, y + other.y, z + other.z);
    }

    void operator +=(const vec3d& other)
    {
        for (byte i = 0; i < 3; ++i)
            v[i] += other.v[i];
    }

    vec3d operator -(const vec3d& other)
    {
        return vec3d(x - other.x, y - other.y, z - other.z);
    }

    void operator -=(const vec3d& other)
    {
        for (byte i = 0; i < 3; ++i)
            v[i] -= other.v[i];
    }

    vec3d operator *(int32_t d)
    {
        d >>= 8;
        return vec3d((x >> 8) * d, (y >> 8) * d, (z >> 8) * d);
    }

    vec3d operator >>(int8_t d)
    {
        return vec3d(x >> d, y >> d, z >> d);
    }

    void operator >>=(int8_t d)
    {
        for (byte i = 0; i < 3; ++i)
            v[i] >>= d;
    }

    void operator <<=(int8_t d)
    {
        for (byte i = 0; i < 3; ++i)
            v[i] <<= d;
    }

    int32_t dot(const vec3d& other)
    {
        int32_t result = 0;
        for (byte i = 0; i < 3; ++i)
            result += (v[i] >> 8) * (other.v[i] >> 8);
        return result;
    }

    int32_t dot(const vec3d_16& other)
    {
        int32_t result = 0;
        for (byte i = 0; i < 3; ++i)
            result += (v[i] >> 8) * (other.v[i] >> 8);
        return result;
    }

    vec3d cross(const vec3d& other)
    {
        vec3d result;
        for (byte i = 0; i < 3; ++i)
        {
            byte i1 = (i + 1) % 3;
            byte i2 = (i + 2) % 3;
            result.v[i] = (v[i1] >> 8) * (other.v[i2] >> 8) - (v[i2] >> 8) * (other.v[i1] >> 8);
        }
        return result;
    }

    vec3d cross(vec3d* result, const vec3d& other)
    {
        for (byte i = 0; i < 3; ++i)
        {
            byte i1 = (i + 1) % 3;
            byte i2 = (i + 2) % 3;
            result->v[i] = (v[i1] >> 8) * (other.v[i2] >> 8) - (v[i2] >> 8) * (other.v[i1] >> 8);
        }
    }

    int32_t length()
    {
        return lsqrt(dot(*this));
    }

    void normalize()
    {
        int32_t l = (1L << 24) / length();
        for (byte i = 0; i < 3; ++i)
            v[i] = (v[i] >> 8) * l;
    }

    bool maximize_length_16()
    {
        byte max_index = 0;
        for (byte k = 1; k < 3; k++)
            if (abs(v[k]) > abs(v[max_index]))
                max_index = k;
        // f is 15.16
        if (v[max_index])
        {
            int32_t f = (0x7fffL << 16) / abs(v[max_index]);
            for (byte k = 0; k < 3; k++)
                v[k] = ((int32_t)v[k] * f) >> 16;
            return true;
        }
        else
            return false;
    }
    
    void rotate(uint8_t axis, int32_t s, int32_t c)
    {
        uint8_t i, k;
        int32_t temp;
        switch(axis) 
        {
            case 0:
                // X: +c -s / +s +c
                temp = v[1];
                v[1] = (v[1] >> 8) * c + (v[2] >> 8) * -s;
                v[2] = (temp >> 8) * s + (v[2] >> 8) *  c;
                break;
            case 1:
                // Y: +c +s / -s +c
                temp = v[0];
                v[0] = (v[0] >> 8) *  c + (v[2] >> 8) * s;
                v[2] = (temp >> 8) * -s + (v[2] >> 8) * c;
                break;
            case 2:
                // Z: +c -s / +s +c
                temp = v[0];
                v[0] = (v[0] >> 8) * c + (v[1] >> 8) * -s;
                v[1] = (temp >> 8) * s + (v[1] >> 8) *  c;
                break;
        }
    }
    
    void rotate(uint8_t axis, int32_t phi)
    {
        int32_t s = lsin(phi) >> 8;
        int32_t c = lcos(phi) >> 8;
        rotate(axis, s, c);
    }
    
    void translate7(struct polygon* line, const vec3d& dx, const vec3d& dy, byte sx0, byte sy0, byte sx1, byte sy1);
};

struct frustum_plane
{
    // this structure uses 6 bytes
    int16_t x0: 12;
    int16_t y0: 11;
    int16_t x1: 12;
    int16_t y1: 11;

    frustum_plane()
        : x0(0), y0(0), x1(0), y1(0)
    {
    }

    frustum_plane(uint16_t _x0, uint16_t _y0, uint16_t _x1, uint16_t _y1)
        : x0(_x0 - 672 + 8) , y0(384 - _y0 - 8) , x1(_x1 - 672 + 8) , y1(384 - _y1 - 8)
    {
    }
    
    void to_vec3d(vec3d* target)
    {
        vec3d a((int32_t)x0 << 8, (int32_t)y0 << 8, -172032);
        *target = vec3d((int32_t)x1 << 8, (int32_t)y1 << 8, -172032);
        *target = target->cross(a);
    }
};

struct polygon
{
    byte num_vertices;
    // keep track of which edges are to be drawn
    // (if we clip a polygon, we don't want to draw a clipped edge)
    byte draw_edges;
    vec3d vertices[MAX_POLYGON_VERTICES];

    polygon()
        : num_vertices(0)
        , draw_edges(0)
    {}

    polygon(const polygon& other)
        : num_vertices(other.num_vertices)
        , draw_edges(other.draw_edges)
    {
        memcpy(vertices, other.vertices, sizeof(vertices));
    }

    void add_vertex(vec3d v, bool draw_edge)
    {
        set_vertex(num_vertices++, v, draw_edge);
    }

    void set_vertex(byte index, vec3d v, bool draw_edge)
    {
#ifdef DEBUG
        if (max_polygon_vertices < index + 1)
            max_polygon_vertices = index + 1;
#endif
        if (index >= MAX_POLYGON_VERTICES)
            return;
        memcpy(&vertices[index], &v, sizeof(vec3d));
        if (draw_edge)
            draw_edges |= (1 << index);
        else
            draw_edges &= ~(1 << index);
    }
};

struct polygon2
{
    byte num_vertices;
    byte draw_edges;
    vec3d vertices[2];
};

struct polygon4
{
    byte num_vertices;
    byte draw_edges;
    vec3d vertices[4];
};

struct textured_polygon4
{
    byte num_vertices;
    byte draw_edges;
    vec3d vertices[4];
    uint16_t uv[4];
};

struct segment
{
    // total size: 8 bytes
    byte floor_height   :  5;
    byte ceiling_height :  5;
    byte x              :  6;
    byte y              :  6;
    byte vertex_count   :  4;
    byte portal_count   :  3;
    byte door_count     :  3;
    word vertices       : 10;
    word normals        : 10;
    byte portals        :  8;
    byte doors          :  4;
};

struct sprite_polygon
{
    byte num_vertices;
    byte draw_edges;
    const byte* vertices;
};

struct sprite
{
    byte polygon_count;
    const sprite_polygon* polygons;
};

struct r_camera
{
    vec3d at;
    vec3d up, forward, right;
    vec3d_16 up8, forward8, right8;
    int32_t yaw, ayaw;
    int32_t pitch, apitch;
    int32_t a;
    #ifdef ENABLE_STRAFE
        int32_t xa, ya;
    #endif
    int width;
    int height;
    uint8_t current_segment_index;
    segment current_segment;
    int32_t wobble;
    int32_t wobble_sin;
    int32_t wobble_shift;

    r_camera()
        : yaw(0)
        , ayaw(0)
        , pitch(0)
        , apitch(0)
        , a(0)
        #ifdef ENABLE_STRAFE
            , xa(0)
            , ya(0)
        #endif
    {}
    
    void set_current_segment(uint8_t segment_index);
};

struct render_job
{
    uint8_t segment;
    uint8_t from_segment;
    uint8_t first_frustum_plane;
    uint8_t frustum_plane_count;

    render_job()
        : segment(0)
        , from_segment(0)
        , first_frustum_plane(0)
        , frustum_plane_count(0)
    {}

    render_job(uint8_t _segment, uint8_t _from_segment, uint8_t _first_frustum_plane, uint8_t _frustum_plane_count)
        : segment(_segment)
        , from_segment(_from_segment)
        , first_frustum_plane(_first_frustum_plane)
        , frustum_plane_count(_frustum_plane_count)
    {}
};

struct render_job_list;
struct wall_loop_info;

frustum_plane shared_frustum_planes[MAX_SHARED_FRUSTUM_PLANES];
render_job_list* next_render_jobs;
render_job_list* current_render_jobs;
segment temp_segment_buffer;
vec3d current_frustum_normals[8];
uint8_t current_frustum_normal_count;

struct render_job_list
{
    uint8_t job_count;
    uint8_t frustum_plane_count;
    uint8_t frustum_plane_offset;
    render_job jobs[MAX_JOB_COUNT];

    render_job_list()
        : job_count(0)
        , frustum_plane_count(0)
        , frustum_plane_offset(0)
    {
    }

    render_job_list(const render_job_list& other)
        : job_count(other.job_count)
        , frustum_plane_count(other.frustum_plane_count)
        , frustum_plane_offset(other.frustum_plane_offset)
    {
        memcpy(jobs, other.jobs, sizeof(jobs));
    }

    // return vec3d offset for requested number of frustum planes or -1 if no more space available
    // (in that case, the segment just won't be rendered)
    int add_job(uint8_t segment, uint8_t from_segment, uint8_t requested_frustum_plane_count)
    {
        if (job_count >= MAX_JOB_COUNT || (next_render_jobs->frustum_plane_count + current_render_jobs->frustum_plane_count + requested_frustum_plane_count > MAX_SHARED_FRUSTUM_PLANES))
            return -1;
        frustum_plane_offset = (frustum_plane_offset + MAX_SHARED_FRUSTUM_PLANES - requested_frustum_plane_count) % MAX_SHARED_FRUSTUM_PLANES;
        jobs[job_count++] = render_job(segment, from_segment, (frustum_plane_offset + 1) % MAX_SHARED_FRUSTUM_PLANES, requested_frustum_plane_count);
        frustum_plane_count += requested_frustum_plane_count;
        return (frustum_plane_offset + 1) % MAX_SHARED_FRUSTUM_PLANES;
    }
};

#ifdef ENABLE_SHOOTING
    // 10 bytes per shot
    // TODO: reduce memory footprint!
    struct shot
    {
        union {
            int16_t p[3];
            struct {
                int16_t x, y, z;
            };
        };
        union {
            int8_t dir[3];
            struct {
                int8_t dx, dy, dz;
            };
        };
        uint8_t current_segment;
    };
#endif

#include "map.h"
#include "sprites.h"
    
void r_camera::set_current_segment(uint8_t segment_index)
{
    current_segment_index = segment_index;
    memcpy_P(&current_segment, &segments[segment_index], sizeof(segment));
}

#ifdef ENABLE_SHOOTING
    byte num_shots;
    shot shots[MAX_SHOTS];
#endif

render_job_list render_jobs_0, render_jobs_1;

bool allow_steering = true;
r_camera camera;
polygon4 _wall;
polygon4 _portal;
polygon2 _line;
polygon clipped_polygon;

#ifdef MONITOR_RAM
extern uint8_t _end;

void stack_paint()
{
    uint8_t *p = &_end;

    while (p < (uint8_t*)&p)
    {
        *p = 0xc5;
        p++;
    }
}

size_t max_ram_usage()
{
    const uint8_t *p = &_end;
    size_t c = 2048;

    while (*p == 0xc5 && p < (uint8_t*)&p)
    {
        p++;
        c--;
    }

    return c;
}
#endif

// inlining lsin and lsqrt saves us 6 bytes of code
inline int32_t lsin(int32_t a)
{
    return (int32_t)(sin((float)a / 0x10000) * 0x10000);
}

inline int32_t lsqrt(int32_t a)
{
    return (int32_t)(sqrt((float)a / 0x10000) * 0x10000);
}

#ifdef SUB_PIXEL_ACCURACY

void draw_line_fixed_point(int *p0, int *p1) {
    int x0 = p0[0];
    int y0 = p0[1];
    int x1 = p1[0];
    int y1 = p1[1];
    bool steep = abs(y1 - y0) > abs(x1 - x0);

    if (steep) {
        int t;
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }

    if (x0 > x1) {
        int t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }

    int32_t nx = x1 - x0;
    int32_t ny = y0 - y1;
    int32_t mx = ((x0 & ~0xf) << 1) + 0x0f;
    int32_t my = ((y0 & ~0xf) << 1);
    if (ny > 0)
        my -= 1;
    else
        my += 0x1f;
    int32_t error = (mx - (x0 << 1)) * ny + (my - (y0 << 1)) * nx;
    int32_t ddx = ny << 5;
    int32_t ddy = nx << 5;

    int sy = 1;
    if (ny > 0)
    {
        error = -error;
        ddx = -ddx;
        sy = -1;
    }
    
    x0 >>= 4;
    y0 >>= 4;
    x1 >>= 4;
    for (; x0 <= x1; ++x0)
    {
        if (steep)
            gb.display.drawPixel(y0, x0);
        else
            gb.display.drawPixel(x0, y0);
        error += ddx;
        if (error < 0)
        {
            error += ddy;
            y0 += sy;
        }
    }
}

#else

#ifdef PORT_ENABLED
    #define draw_line_fixed_point(v0, v1) gb.display.drawLine((float)v0[0], (float)v0[1], (float)v1[0], (float)v1[1])
#else
    #define draw_line_fixed_point(v0, v1) gb.display.drawLine(v0[0] >> 4, v0[1] >> 4, v1[0] >> 4, v1[1] >> 4)
#endif

#endif

byte log2(int32_t v)
{
    if (v < 0)
        v = -v;
    byte r = 0;

    while (v >>= 1)
        r++;
    
    return r;
}

void title_screen()
{
#ifdef SHOW_TITLE_SCREEN
    gb.titleScreen(F("CRUISER"));
    allow_steering = false;
#endif
    gb.battery.show = false;
    camera = r_camera();
    camera.at = vec3d((int32_t)(1.5 * 65536), (int32_t)(4.5 * 65536), (int32_t)(9.75 * 65536));
    camera.set_current_segment(0);
//     camera.at = vec3d((int32_t)(16.0 * 65536), (int32_t)(4.5 * 65536), (int32_t)(13.0 * 65536));
//     camera.set_current_segment(11);
//     camera.yaw = -80000;
    #ifdef ENABLE_SHOOTING
        num_shots = 0;
    #endif
    camera.wobble = 0;
    #ifdef ENABLE_MAP    
        memset(segments_seen, 0, SEGMENTS_TOUCHED_SIZE);
        map_mode = false;
    #endif
    for (byte i = 0; i < DOOR_COUNT; i++)
        door_state[i] = -1;
}

void setup()
{
#ifdef MONITOR_RAM
    stack_paint();
#endif
    gb.begin();
    title_screen();
    next_render_jobs = &render_jobs_0;
    current_render_jobs = &render_jobs_1;
    camera.width = LCDWIDTH;
    camera.height = LCDHEIGHT;
}

void handle_controls()
{
    bool a_pressed = gb.buttons.repeat(BTN_A, 1);
    bool b_pressed = gb.buttons.repeat(BTN_B, 1);
    if (gb.buttons.pressed(BTN_C))
#ifdef ENABLE_MAP    
    {
        map_mode = !map_mode;
        if (map_mode)
        {
            map_scale = 5L << 16;
            map_dx = 0;
            map_dy = 0;
            // stop all movement when entering map mode
            camera.ayaw = 0;
            camera.apitch = 0;
            camera.a = 0;
        }
    }
    if (gb.buttons.held(BTN_C, 20))
#endif
        title_screen();
    if (b_pressed)
    {
    #ifdef ENABLE_MAP
        if (map_mode)
        {
            map_scale = (int32_t)((((float)map_scale / 65536.0) * 0.95) * 65536.0);
            if (map_scale < MIN_MAP_SCALE)
                map_scale = MIN_MAP_SCALE;
        }
        else
    #endif
        {
            #ifdef ENABLE_SHOOTING
                // fire shots
                if (current_frame_start_millis - last_shot_frame_millis > 250)
                {
                    LOG("firing shots!\n");
                    last_shot_frame_millis = current_frame_start_millis;
                    for (byte i = 0; i < 2; i++)
                    {
                        if (num_shots < MAX_SHOTS)
                        {
                            for (byte k = 0; k < 3; k++)
                            {
                                shots[num_shots].p[k] = camera.at.v[k] >> 8;
                                shots[num_shots].p[k] -= ((camera.up.v[k] * 3277) >> 24);
                                if (i == 0)
                                    shots[num_shots].p[k] += ((camera.right.v[k] * 3277) >> 24);
                                else
                                    shots[num_shots].p[k] -= ((camera.right.v[k] * 3277) >> 24);
                                    
                                shots[num_shots].dir[k] = camera.forward.v[k] >> 9;
                            }
                            shots[num_shots].current_segment = camera.current_segment_index;
                            num_shots++;
                        }
                    }
                }
            #endif
        }
    }

    if (gb.buttons.released(BTN_A))
        allow_steering = true;
    if (allow_steering)
    {
    #ifdef ENABLE_MAP
        if (map_mode)
        {
            int32_t map_move = ((1L << 24) / (map_scale >> 8)) * (micros_per_frame >> 10) >> 5;
            if (gb.buttons.repeat(BTN_LEFT, 1))
            {
                map_dx += map_move;
            }
            if (gb.buttons.repeat(BTN_RIGHT, 1))
            {
                map_dx -= map_move;
            }
            if (gb.buttons.repeat(BTN_DOWN, 1))
            {
                map_dy -= map_move;
            }
            if (gb.buttons.repeat(BTN_UP, 1))
            {
                map_dy += map_move;
            }
            if (a_pressed)
            {
                map_scale = (int32_t)((((float)map_scale / 65536.0) * 1.05) * 65536.0);
                if (map_scale > MAX_MAP_SCALE)
                    map_scale = MAX_MAP_SCALE;
            }
        }
        else
    #endif
        {
            if (gb.buttons.repeat(BTN_LEFT, 1))
            {
            #ifdef ENABLE_STRAFE
                if (b_pressed)
                    camera.xa = -100000;
                else
            #endif
                    camera.ayaw = 80000;
            }
            if (gb.buttons.repeat(BTN_RIGHT, 1))
            {
            #ifdef ENABLE_STRAFE
                if (b_pressed)
                    camera.xa = 100000;
                else
            #endif
                    camera.ayaw = -80000;
            }
            if (gb.buttons.repeat(BTN_DOWN, 1))
            {
            #ifdef ENABLE_STRAFE
                if (b_pressed)
                    camera.ya = -100000;
                else
            #endif
                    camera.apitch = 80000;
            }
            if (gb.buttons.repeat(BTN_UP, 1))
            {
            #ifdef ENABLE_STRAFE
                if (b_pressed)
                    camera.ya = 100000;
                else
            #endif
                    camera.apitch = -80000;
            }
            if (a_pressed)
            {
//                 if (b_pressed)
//                     camera.a = -1;
//                 else
                    camera.a = 200000;
            }
        }
    }
}

struct wall_loop_info {
    uint8_t wall_index;
    uint8_t x0, z0, x1, z1;
    int16_t adjacent_segment_index;
    uint8_t adjacent_floor_height, adjacent_ceiling_height;
    int8_t door_index;
    uint16_t door_time;
    bool door_is_open, also_drew_previous_wall;
};

wall_loop_info wall_info;
    
// TODO: Add option to declare we want wall normals passed to callback function as well
void loop_through_segment_walls(uint8_t segment_index, segment* segment, 
                                bool early_adjacent_segment_culling,
                                bool(*callback)(wall_loop_info*, void*), void* callback_info)
{
//     LOG("Looping through segment walls of segment %d with %d portals and %d doors.\n",
//         segment_index, segment->portal_count, segment->door_count);
    const uint8_t* next_portal_pointer = portals + segment->portals;
    uint8_t next_portal_point = pgm_read_byte(next_portal_pointer);
//     LOG("Next portal byte is 0x%02x...\n", next_portal_point);
    uint8_t remaining_portals = segment->portal_count;
    
    const uint8_t* next_door_pointer = doors + segment->doors;
    uint8_t next_door = pgm_read_byte(next_door_pointer);
    uint8_t remaining_doors = segment->door_count;
    
    int8_t x1_z1_from_point = -1;
    uint8_t temp_byte;
    
    for (uint8_t i = 0; i < segment->vertex_count; i++)
//     for (uint8_t i = 1; i < 2; i++)
    {
        wall_info.wall_index = i;
        wall_info.adjacent_segment_index = -1;
        // test if it's a portal
        if (remaining_portals && (i == (next_portal_point >> 5)))
        {
            --remaining_portals;
            if (next_portal_point & 0x10)
            {
                // adjacent segment is stored in next byte
                wall_info.adjacent_segment_index = pgm_read_byte(next_portal_pointer + 1);
                next_portal_pointer += 2;
            }
            else
            {
                // adjacent segment is differential encoded
                int8_t diff = (next_portal_point & 7) + 1;
                if (next_portal_point & 8)
                    diff = -diff;
                wall_info.adjacent_segment_index = segment_index + diff;
                next_portal_pointer++;
            }
            next_portal_point = pgm_read_byte(next_portal_pointer);
//             LOG("Next portal byte is 0x%02x...\n", next_portal_point);
            
            // early face culling: skip the portal if it's leading to a segment we have already touched in this frame
            if (early_adjacent_segment_culling)
                if ((((segments_touched[wall_info.adjacent_segment_index >> 3] >> (wall_info.adjacent_segment_index & 7)) & 1) == 1))
                    continue;
            
            #ifdef PORT_ENABLED
                struct segment temp_segment;
                memcpy_P(&temp_segment, &segments[wall_info.adjacent_segment_index], sizeof(segment));
                wall_info.adjacent_floor_height = temp_segment.floor_height;
                wall_info.adjacent_ceiling_height = temp_segment.ceiling_height;
            #else
                uint16_t temp = pgm_read_word((uint16_t*)&segments[wall_info.adjacent_segment_index]);
                wall_info.adjacent_floor_height = temp & 0x1f;
                wall_info.adjacent_ceiling_height = (temp >> 5) & 0x1f;
            #endif
        }
        
        wall_info.door_index = -1;
        wall_info.door_is_open = false;
        wall_info.door_time = 0;

        // also test if it's a door
        if (remaining_doors && (i == (next_door >> 4)))
        {
            --remaining_doors;
            wall_info.door_index = next_door & 0xf;
            next_door_pointer++;
            next_door = pgm_read_byte(next_door_pointer);
            if (door_state[wall_info.door_index] >= 0)
            {
                uint32_t temp_door_time = current_frame_start_millis - door_state[wall_info.door_index];
                if (temp_door_time > 4000)
                    temp_door_time = 4000;
                wall_info.door_time = temp_door_time;
                if (wall_info.door_time > 500 && wall_info.door_time < 3500)
                    wall_info.door_is_open = true;
            }
        }
        
        wall_info.also_drew_previous_wall = (x1_z1_from_point == ((i + 1) % segment->vertex_count + segment->vertices));
        if (wall_info.also_drew_previous_wall)
        {
            // re-use the coordinates we already read to save calls to pgm_read_byte
            wall_info.x0 = wall_info.x1;
            wall_info.z0 = wall_info.z1;
        }
        else
        {
            temp_byte = pgm_read_byte(&vertices[i % segment->vertex_count + segment->vertices]);
            wall_info.x0 = segment->x + (temp_byte >> 4);
            wall_info.z0 = segment->y + (temp_byte & 0xf);
        }

        x1_z1_from_point = (i + 1) % segment->vertex_count + segment->vertices;
        temp_byte = pgm_read_byte(&vertices[x1_z1_from_point]);
        wall_info.x1 = segment->x + (temp_byte >> 4);
        wall_info.z1 = segment->y + (temp_byte & 0xf);
        
        // now we have everything togetgher:
        // wall coordinates: x0, z0, x1, z1
        // floor_height, ceiling_height
        // adjacent_segment_index (-1 or index), adjacent_floor_height, adjacent_ceiling_height
        // door_index (-1 or index), door_time, door_is_open
        // ...so now is the time to call the callback function!
        if (!callback(&wall_info, callback_info))
            break;
    }
}

struct collision_detection_callback_info {
    vec3d* from;
    vec3d* to;
    vec3d dir;
    uint16_t bump_distance;
    uint8_t collided;
    uint8_t* new_segment_index;
    int8_t* hit_door_index;
};

bool collision_detection_callback(wall_loop_info* wall_info, void* _callback_info)
{
    collision_detection_callback_info* callback_info = (collision_detection_callback_info*)(_callback_info);
    vec3d n(((int32_t)wall_info->z0 - wall_info->z1) << 16, 0, ((int32_t)wall_info->x1 - wall_info->x0) << 16);
    // perform collision detection if we're facing this wall
    // TODO: speed this up with binary search
    if (n.dot(callback_info->dir) > 0)
    {
        vec3d wall_p((int32_t)wall_info->x0 << 16, 0.0, (int32_t)wall_info->z0 << 16);
        if ((wall_info->adjacent_segment_index == -1) || 
            (wall_info->adjacent_segment_index != -1 && wall_info->door_index >= 0 && !wall_info->door_is_open))
        {
            // it's a wall
            // we actually have to normalize n here, because we want a fixed distance to the walls (bump_distance)
            // TODO: try normal_scale lookup, it might help a bit because we get rid of the square root
            n.normalize();
            wall_p -= n * callback_info->bump_distance;
            wall_p -= *callback_info->to;
            int32_t f = wall_p.dot(n);
            if (f < 0)
            {
                if (wall_info->door_index >= 0)
                    *callback_info->hit_door_index = wall_info->door_index;
                // project trajectory onto wall and continue
                vec3d temp = *callback_info->to + n * f;
                if ((temp - vec3d((int32_t)wall_info->x0 << 16, 0, (int32_t)wall_info->z0 << 16)).dot(vec3d(((int32_t)wall_info->x1 - wall_info->x0) << 16, 0, ((int32_t)wall_info->z1 - wall_info->z0) << 16)) > 0 &&
                    (temp - vec3d((int32_t)wall_info->x1 << 16, 0, (int32_t)wall_info->z1 << 16)).dot(vec3d(((int32_t)wall_info->x0 - wall_info->x1) << 16, 0, ((int32_t)wall_info->z0 - wall_info->z1) << 16)) > 0)
                {
                    // we're actually hitting this wall, not another wall that shares the same plane
                    *callback_info->to = temp;
                    callback_info->dir = *callback_info->to - *callback_info->from;
                    callback_info->collided = wall_info->wall_index;
                }
            }
        }
        else
        {
            // it's a portal
            wall_p -= *callback_info->to;
            int32_t f = wall_p.dot(n);
            if (f < 0)
            {
                // yup, we've crossed the portal, move camera to adjacent segment
                //LOG("crossed a portal: from %d to %d\n", *segment, adjacent_segment);
                if (wall_info->adjacent_segment_index != -1)
                {
                    *callback_info->new_segment_index = wall_info->adjacent_segment_index;
                    // If we don't return here, we'll get some portal jumping bugs, yay!
                    // return false so that the loop will be aborted here
                    return false;
                }
            }
        }
    }
    return true;
}

// returns wall index of collision or:
// - 0xfd for floor collision
// - 0xfe for ceiling collision
// - 0xff if no collision
byte collision_detection(uint8_t current_segment_index, segment* current_segment, 
                         uint8_t* new_segment_index, 
                         int8_t* hit_door_index,
                         vec3d* from, vec3d* to, 
                         uint16_t bump_distance)
{
    collision_detection_callback_info callback_info;
    
    callback_info.from = from;
    callback_info.to = to;
    callback_info.dir = *to - *from;
    callback_info.bump_distance = bump_distance;
    callback_info.collided = 0xff;
    callback_info.new_segment_index = new_segment_index;
    callback_info.hit_door_index = hit_door_index;
    *callback_info.new_segment_index = current_segment_index;
    *callback_info.hit_door_index = -1;

    // perform collision detection for floor and ceiling
    // default: heading for the floor
    vec3d n(0, -65536, 0);
    int32_t y = (int32_t)current_segment->floor_height << 14;
    byte collision_code = 0xfd;
    if (callback_info.dir.y > 0)
    {
        // heading for the ceiling
        n.y = 65536;
        y = (int32_t)current_segment->ceiling_height << 14;
        collision_code = 0xfe;
    }
    vec3d p(0, y - ((n.y * 26) >> 8), 0);
    p -= *to;
    int32_t f = p.dot(n);
    if (f < 0)
    {
        // project trajectory onto floor/ceiling and continue
        *to += n * f;
        callback_info.dir = *to - *from;
        callback_info.collided = collision_code;
    }
    
    loop_through_segment_walls(current_segment_index, current_segment, false, &collision_detection_callback, &callback_info);
    return callback_info.collided;
}

void move_player()
{
    if (camera.ayaw < 0)
    {
        camera.yaw += PI2;
        camera.yaw -= (-camera.ayaw) * (micros_per_frame >> 10) >> 10;
        camera.ayaw = -((-camera.ayaw * 205) >> 8); // * 0.8
    }
    else
    {
        camera.yaw += camera.ayaw * (micros_per_frame >> 10) >> 10;
        camera.ayaw = (camera.ayaw * 205) >> 8; // * 0.8
    }
    camera.yaw %= PI2;

    if (camera.apitch < 0)
    {
        camera.pitch += PI2;
        camera.pitch -= (-camera.apitch) * (micros_per_frame >> 10) >> 10;
        camera.apitch = -((-camera.apitch * 205) >> 8); // * 0.8
    }
    else
    {
        camera.pitch += camera.apitch * (micros_per_frame >> 10) >> 10;
        camera.apitch = (camera.apitch * 205) >> 8; // * 0.8
    }
    camera.pitch %= PI2;

    // auto-leveling
    // TODO: disable auto-leveling in vertical corridors!
    if (camera.pitch < PI1)
        camera.pitch = (camera.pitch * 243) >> 8; // * 0.95
    else
        camera.pitch = PI2 - (((PI2 - camera.pitch) * 243) >> 8); // * 0.95

    camera.up = vec3d(0, 65536, 0);
    camera.forward = vec3d(0, 0, -65536);

    // TODO: re-use sine and cosine values for both following rotations!
#ifdef ENABLE_MAP
    if (map_mode)
    {
//         camera.up.rotate(camera.yaw);
//         camera.forward.rotate(camera.yaw);
    }
    else
#endif
    {
        int32_t s, c;
#ifdef ROLL_SHIP
        // roll (rotate around z axis - tilt left and right)
        s = lsin((int32_t)camera.ayaw >> 4) >> 8;
        c = lcos((int32_t)camera.ayaw >> 4) >> 8;
        camera.up.rotate(2, s, c);
#endif
        // pitch (rotate around x axis - up and down)
        s = lsin(camera.pitch) >> 8;
        c = lcos(camera.pitch) >> 8;
        camera.up.rotate(0, s, c);
        camera.forward.rotate(0, s, c);
        // yaw (rotate around y axis - turn left and right)
        s = lsin(camera.yaw) >> 8;
        c = lcos(camera.yaw) >> 8;
        camera.up.rotate(1, s, c);
        camera.forward.rotate(1, s, c);
    }

    camera.right = camera.forward.cross(camera.up);
    
    camera.up8 = camera.up.divby256();
    camera.forward8 = camera.forward.divby256();
    camera.right8 = camera.right.divby256();

    //    vec3d new_at = camera.at + camera.forward * camera.a + camera.up * camera.ya + camera.right * camera.xa;
    vec3d new_at = camera.at + camera.forward * (camera.a * (micros_per_frame >> 10) >> 10);
    
    #ifdef ENABLE_STRAFE
    if (camera.xa < 0)
        new_at -= camera.right * (-camera.xa * (micros_per_frame >> 10) >> 10);
    else
        new_at += camera.right * (camera.xa * (micros_per_frame >> 10) >> 10);

    if (camera.ya < 0)
        new_at -= camera.up * (-camera.ya * (micros_per_frame >> 10) >> 10);
    else
        new_at += camera.up * (camera.ya * (micros_per_frame >> 10) >> 10);
        
        if (camera.ya < 0)
            camera.ya = -((-camera.ya * 205) >> 8); // * 0.8
        else
            camera.ya = (camera.ya * 205) >> 8; // * 0.8
    
        if (camera.xa < 0)
            camera.xa = -((-camera.xa * 205) >> 8); // * 0.8
        else
            camera.xa = (camera.xa * 205) >> 8; // * 0.8
    #endif

#ifdef COLLISION_DETECTION
    if (camera.a != 0
    #ifdef ENABLE_STRAFE
        || camera.xa != 0 || camera.ya != 0
        #endif
    )
    {
        uint8_t new_segment_index;
        int8_t hit_door_index;
        collision_detection(camera.current_segment_index, 
                            &camera.current_segment,
                            &new_segment_index,
                            &hit_door_index,
                            &camera.at, &new_at, 16384);
        if (new_segment_index != camera.current_segment_index)
            camera.set_current_segment(new_segment_index);
        if (hit_door_index >= 0)
        {
            int32_t door_time = current_frame_start_millis - door_state[hit_door_index];
            if (door_time > 3500 || door_state[hit_door_index] < 0)
                door_state[hit_door_index] = current_frame_start_millis;
        }
    }
#endif
    camera.at = new_at;

    camera.a = (camera.a * 230) >> 8;

    camera.wobble = (camera.wobble + (micros_per_frame >> 5)) % 65536;
    camera.wobble_sin = lsin((camera.wobble >> 8) * (PI2 >> 8));
    
#ifdef ENABLE_SHOOTING
    // move shots
    for (int i = 0; i < num_shots; i++)
    {
        vec3d p;
        for (byte k = 0; k < 3; k++)
        {
            p.v[k] = (int32_t)shots[i].p[k] << 8;
            new_at.v[k] = p.v[k] + ((((int32_t)shots[i].dir[k]) * (int32_t)micros_per_frame) << 7) / 50000;
        }
        uint8_t new_segment_index;
        segment* segment = &camera.current_segment;
        if (shots[i].current_segment != camera.current_segment_index)
        {
            memcpy_P(&temp_segment_buffer, &segments[shots[i].current_segment], sizeof(segment));
            segment = &temp_segment_buffer;
        }
        int8_t hit_door_index;
        byte wall_collided = collision_detection(shots[i].current_segment, 
                                                 segment,
                                                 &new_segment_index,
                                                 &hit_door_index,
                                                 &p, &new_at, 16384);
        bool shot_stopped_by_door = false;
        if (hit_door_index >= 0)
        {
            int32_t door_time = current_frame_start_millis - door_state[hit_door_index];
            shot_stopped_by_door = true;
            if (door_time > 500 && door_time < 3500)
                shot_stopped_by_door = false;
            if (door_time > 3500 || door_state[hit_door_index] < 0)
                door_state[hit_door_index] = current_frame_start_millis;
        }
            
        if (shot_stopped_by_door || wall_collided != 0xff)
        {
            // flare hit a wall, remove the shot
            if (num_shots > 1)
                memcpy(&shots[i], &shots[num_shots - 1], sizeof(shot));
            num_shots--;
            i--;
        }
        else
        {
            shots[i].current_segment = new_segment_index;
            for (byte k = 0; k < 3; k++)
                shots[i].p[k] = new_at.v[k] >> 8;
        }
    }
#endif
}

// without in place clipping: 1778 bytes RAM used
// with in place clipping: 1636 bytes RAM used (saving 142 bytes!)

// void clip_polygon_against_plane(polygon* result, const vec3d_16& clip_plane_normal, polygon* source)
void clip_polygon_against_plane(polygon* source, polygon* target, const vec3d& clip_plane_normal)
{
//     LOG("Clipping polygon against %d %d %d\n", clip_plane_normal.x, clip_plane_normal.y, clip_plane_normal.z);
//     LOG("Got %d vertices before clipping\n", p->num_vertices);
    // TODO: In order to save a lot of RAM, we should investigate whether clipping can be done in place
    // so that we don't have to allocate RAM for the destination polyon
    // IN PLACE CLIPPING:
    // When we clip a convex polygon against a plane, n vertices become at most n+1 vertices, so we can totally
    // do this in place and we also save time to copy any vertices that remain untouched. 
    // If we have to insert a vertex, just copy the remaining source vertices one unit down the line... YES!
    uint8_t source_vertex_index = 0;
    uint8_t source_vertex_count = source->num_vertices;
    target->num_vertices = 0;
    uint8_t target_vertex_index = 0;
    uint8_t target_draw_edges = 0;
    vec3d *v0 = NULL;
    vec3d *v1 = NULL;
    int32_t d0, d1;
    bool flag0, flag1;
    vec3d intersection;
    vec3d first_vertex(source->vertices[0]);
    uint8_t source_draw_edge_offset = 0;
    while (source_vertex_index < source_vertex_count)
    {
        if (v1)
        {
            v0 = v1;
            d0 = d1;
            flag0 = flag1;
        }
        else
        {
            v0 = &source->vertices[source_vertex_index];
            // default: 16.16 * 16.16 => 16.8 * 16.8 => 16.16
            // optimum: 8.16 * 8.16 => 8.12 * 8.12 => 16.16
            d0 = v0->dot(clip_plane_normal);
            // CAVEAT: d >= 0 would result in degenerate faces which we would spend time on!
            flag0 = d0 > 0;
        }
        // TODO: If we wrap around here, the original vertex may already
        // have been clipped away - unsure whether it's a problem
        if (source_vertex_index == source_vertex_count - 1)
            v1 = &first_vertex;
        else
            v1 = &source->vertices[source_vertex_index + 1];
        d1 = v1->dot(clip_plane_normal);
        flag1 = d1 > 0;

        if (flag0 ^ flag1)
        {
            // this line segment goes from inside to outside or vice verse,
            // let's calculate the intersection point
            // d0 and d1 are 8.16
            int32_t f = (-d1 << 8) / (d0 - d1);
            int32_t f1 = 256 - f;
            for (byte j = 0; j < 3; ++j)
                intersection.v[j] = (v0->v[j] >> 8) * f + (v1->v[j] >> 8) * f1;
        }
        if (flag0)
        {
            target->vertices[target_vertex_index] = *v0;
            if ((source->draw_edges >> (source_vertex_index - source_draw_edge_offset)) & 1)
                target_draw_edges |= (1 << target_vertex_index);
            ++target_vertex_index;
            if (target_vertex_index == MAX_POLYGON_VERTICES)
                break;
            if (!flag1)
            {
                if (source == target)
                {
                    // NOTICE: If we're doing in place clipping, we have to be very careful here.
                    // Before we can add the output vertex, we have to move the input vertices 
                    // to the right.
                    uint8_t shift_vertex_count = source_vertex_count - source_vertex_index - 1;
    //                 LOG("%d\n", shift_vertex_count);
                    if (shift_vertex_count)
                    {
                        if (source_vertex_count == MAX_POLYGON_VERTICES)
                            break;
                        for (uint8_t i = shift_vertex_count; i > 0; --i)
                        {
    //                         memcpy(&p->vertices[source_vertex_index + i + 1], &p->vertices[source_vertex_index + i], sizeof(vec3d));
                            source->vertices[source_vertex_index + i + 1] = source->vertices[source_vertex_index + i];
//                             LOG("Shifting: @%d = @%d\n", source_vertex_index + i + 1, source_vertex_index + i);
                        }
                        ++source_vertex_count;
                        ++source_vertex_index;
                        ++v1;
                        source_draw_edge_offset += 1;
                    }
                }
                target->vertices[target_vertex_index] = intersection;

                ++target_vertex_index;
                if (target_vertex_index == MAX_POLYGON_VERTICES)
                    break;
            }
        }
        else if (flag1)
        {
            target->vertices[target_vertex_index] = intersection;
            if ((source->draw_edges >> (source_vertex_index - source_draw_edge_offset)) & 1)
                target_draw_edges |= (1 << target_vertex_index);
            ++target_vertex_index;
            if (target_vertex_index == MAX_POLYGON_VERTICES)
                break;
        }
        ++source_vertex_index;
    }
    target->num_vertices = target_vertex_index;
    target->draw_edges = target_draw_edges;
//     target->draw_edges = 0xff;
//     LOG("Got %d vertices after clipping\n", target->num_vertices);
}

void transform_world_space_to_view_space(vec3d* v, byte count = 1)
{
    for (byte i = 0; i < count; ++i)
    {
        vec3d* r =&v[i];
        vec3d s(*r);
        for (byte j = 0; j < 3; ++j)
            s.v[j] = (s.v[j] - camera.at.v[j]) >> 8;
        // TODO: put this into a loop to save code space
        r->x =  s.x * (camera.right8.x  ) + s.y * (camera.right8.y  ) + s.z * (camera.right8.z  );
        r->y =  s.x * (camera.up8.x     ) + s.y * (camera.up8.y     ) + s.z * (camera.up8.z     );
        r->z = -s.x * (camera.forward8.x) - s.y * (camera.forward8.y) - s.z * (camera.forward8.z);
        #ifdef ENABLE_MAP
        if (!map_mode)
        #endif
        {
    #ifdef WOBBLE_SHIP
            // add wobble
            r->y += (camera.wobble_sin * 13) >> 8;
    #endif
        }
    }
}

void project_vertex(const vec3d& p, LINE_COORDINATE_TYPE* tv)
{
    int32_t z1 = (((-1L) << 24) / p.z);
    tv[0] = (((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE) + ((((p.x * ((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
    tv[1] = (((SCREEN_RESOLUTION[1] - 1) << FIXED_POINT_SCALE) - ((((p.y * ((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
}

polygon* render_polygon(polygon* p, byte min_vertex_count = 3)
{
    #ifdef CLIP_TO_FRUSTUM
        #ifdef ENABLE_MAP
            if (!map_mode)
        #endif
        {
            for (int k = 0; k < current_frustum_normal_count; k++)
            {
                clip_polygon_against_plane(k == 0 ? p : &clipped_polygon, &clipped_polygon, current_frustum_normals[k]);
                if (clipped_polygon.num_vertices < min_vertex_count)
                    break;
            }
        }
        p = &clipped_polygon;
    #endif

    // skip this polygon if too many vertices have been clipped away
    if (p->num_vertices < min_vertex_count)
        return 0;

    #ifdef DEBUG
        faces_drawn += 1;
    #endif

    LINE_COORDINATE_TYPE first[2];
    LINE_COORDINATE_TYPE last[2];
    
    LOG("Drawing polygon with %d vertices.\n", p->num_vertices);

    for (int k = 0; k < p->num_vertices; k++)
    {
        LINE_COORDINATE_TYPE tv[2];
        #ifdef ENABLE_MAP
            if (map_mode)
            {
                tv[0] = ((((map_scale >> 8) * ((p->vertices[k].x + map_dx) >> 8)) >> 16) + 42) << FIXED_POINT_SCALE;
                tv[1] = ((((map_scale >> 8) * ((p->vertices[k].z + map_dy) >> 8)) >> 16) + 34) << FIXED_POINT_SCALE;
            }
            else
        #endif
        {
            project_vertex(p->vertices[k], tv);
//             int32_t z1 = (((-1L) << 24) / p->vertices[k].z);
//             tv[0] = (((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE) + ((((p->vertices[k].v[0] * ((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
//             tv[1] = (((SCREEN_RESOLUTION[1] - 1) << FIXED_POINT_SCALE) - ((((p->vertices[k].v[1] * ((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
//             LOG("PROJ [%d] %d %d\n", k, tv[0] >> FIXED_POINT_SCALE, tv[1] >> FIXED_POINT_SCALE);
            draw_pixel(tv[0], tv[1]);
        }

        if (k == 0)
        {
            first[0] = tv[0];
            first[1] = tv[1];
        }
        // TODO: Find out why memcpy doesn't cut it here and further down as well
//             memcpy(first, tv, 4);
        else
        {
            if ((p->draw_edges >> (k - 1)) & 1)
                draw_line_fixed_point(last, tv);
        }
        last[0] = tv[0];
        last[1] = tv[1];
//         memcpy(last, tv, 4);
    }
    if ((p->draw_edges >> (p->num_vertices - 1)) & 1)
        draw_line_fixed_point(last, first);
    
    return p;
}

void vec3d::translate7(polygon* line, const vec3d& dx, const vec3d& dy, byte sx0, byte sy0, byte sx1, byte sy1)
{
    line->vertices[0].x = this->x + ((dx.x * sx0) >> 7) + ((dy.x * sy0) >> 7);
    line->vertices[0].y = this->y + ((dx.y * sx0) >> 7) + ((dy.y * sy0) >> 7);
    line->vertices[0].z = this->z + ((dx.z * sx0) >> 7) + ((dy.z * sy0) >> 7);
    line->vertices[1].x = this->x + ((dx.x * sx1) >> 7) + ((dy.x * sy1) >> 7);
    line->vertices[1].y = this->y + ((dx.y * sx1) >> 7) + ((dy.y * sy1) >> 7);
    line->vertices[1].z = this->z + ((dx.z * sx1) >> 7) + ((dy.z * sy1) >> 7);
    render_polygon(line, 2);
}

void render_sprite(int32_t x, int32_t y, int32_t z)
{
    LOG("Rendering sprite...\n");
    _wall.num_vertices = 4;
    _wall.draw_edges = 0xf;
    vec3d p(x, y, z);
    vec3d dx = camera.right * 8192;
//    dx.rotate((camera.wobble >> 8) * (PI2 >> 8));
    vec3d dy = camera.up * 8192;
    for (int32_t px = 0; px < 1; px++)
    {
        for (int32_t py = 0; py < 1; py++)
        {
            _wall.vertices[0] = p + dx * (px << 16) + dy * (py << 16);
            _wall.vertices[1] = p + dx * ((px + 4) << 16) + dy * (py << 16);
            _wall.vertices[2] = p + dx * ((px + 4) << 16) + dy * ((py + 4) << 16);
            _wall.vertices[3] = p + dx * (px << 16) + dy * ((py + 4) << 16);
            LOG("%d %d %d\n", _wall.vertices[0].x, _wall.vertices[0].y, _wall.vertices[0].z);
            transform_world_space_to_view_space(_wall.vertices, 4);
            render_polygon((polygon*)&_wall, 3);
        }
    }
    /*
     * fill the polygon:
     * - rasterize lines, interpolate u, v coordinates
     *   - maybe Bresenham is not the best choice here, because we need spans in the end
     * - keep track of ymin, ymax
     * - for every line, keep track of xmin, xmax
     * - render every span
     *   - 1/z interpolates linearly (maybe ignore perspective, heh...)
     */
    
}

struct render_segment_callback_info {
    // TODO: PRO TIP: the wall always has 4 vertices, but polygon struct reserves space for 8 vertices,
    // so we're just declaring a polygon with 4 vertices and force-cast it to polygon... hue hue hue
    // THIS SAVES US 48 BYTES.... PRECIOUS PRECIOUS BYTES!
    polygon* wall;
    polygon* line;
    uint8_t segment_index;
    segment* current_segment;
    vec3d p0, p1;
};

bool render_segment_callback(wall_loop_info* wall_info, void* _callback_info)
{
    #ifdef DEBUG
        faces_touched += 1;
    #endif
        
    render_segment_callback_info* callback_info = (render_segment_callback_info*)_callback_info;
//     LOG("Handling wall %d in segment %d, and portal to adjacent segment %d...\n", 
//         wall_info->wall_index, callback_info->segment_index, wall_info->adjacent_segment_index);

    callback_info->p0 = vec3d((int32_t)wall_info->x0 << 16, (int32_t)callback_info->current_segment->floor_height << 14, (int32_t)wall_info->z0 << 16);
    callback_info->p1 = vec3d((int32_t)wall_info->x1 << 16, (int32_t)callback_info->current_segment->floor_height << 14, (int32_t)wall_info->z1 << 16);

    // construct wall polygon
    callback_info->wall->num_vertices = 4;
    bool wall_or_door = (wall_info->adjacent_segment_index < 0) || (wall_info->door_index >= 0);
    if (wall_info->also_drew_previous_wall)
    {
        callback_info->wall->set_vertex(0, callback_info->p1, wall_or_door);
        callback_info->wall->set_vertex(3, callback_info->p1, false);
    }
    else
    {
        callback_info->wall->set_vertex(0, callback_info->p1, wall_or_door);
        #ifdef ENABLE_MAP
            callback_info->wall->set_vertex(1, callback_info->p0, !map_mode);
            callback_info->wall->set_vertex(2, callback_info->p0, (!map_mode) && wall_or_door);
        #else
            callback_info->wall->set_vertex(1, callback_info->p0, true);
            callback_info->wall->set_vertex(2, callback_info->p0, wall_or_door);
        #endif
        // don't draw second vertical edge, it will be drawn by the adjacent wall
        callback_info->wall->set_vertex(3, callback_info->p1, false);
    }
    callback_info->wall->vertices[2].y = (int32_t)callback_info->current_segment->ceiling_height << 14;
    callback_info->wall->vertices[3].y = (int32_t)callback_info->current_segment->ceiling_height << 14;
    
    if (wall_info->adjacent_segment_index >= 0)
    {
        if (wall_info->adjacent_floor_height != callback_info->current_segment->floor_height)
            callback_info->wall->draw_edges |= 1;
        if (wall_info->adjacent_ceiling_height != callback_info->current_segment->ceiling_height)
            callback_info->wall->draw_edges |= 4;
    }

    // transform wall to view space
    // re-use transformed vertices from the last wall if possible
    if (wall_info->also_drew_previous_wall)
    {
        transform_world_space_to_view_space(&callback_info->wall->vertices[0]);
        transform_world_space_to_view_space(&callback_info->wall->vertices[3]);
    }
    else
        transform_world_space_to_view_space(callback_info->wall->vertices, 4);
//     for (int i = 0; i < 4; i++)
//         LOG("[%d] (%d %d %d) / (%1.1f %1.1f %1.1f)\n", i, 
//             callback_info->wall->vertices[i].x,
//             callback_info->wall->vertices[i].y,
//             callback_info->wall->vertices[i].z,
//             (float)callback_info->wall->vertices[i].x / 65536.0,
//             (float)callback_info->wall->vertices[i].y / 65536.0,
//             (float)callback_info->wall->vertices[i].z / 65536.0);

    // render the wall
    polygon* clipped_portal = render_polygon(callback_info->wall);
    if (!clipped_portal)
        return true;
    
    // render the door if there's one
    if (wall_info->door_index >= 0)
    {
        LOG("There's a door with door time %d.\n", wall_info->door_time);
        vec3d dx = callback_info->wall->vertices[1] - callback_info->wall->vertices[0];
        vec3d dy = callback_info->wall->vertices[3] - callback_info->wall->vertices[0];
        callback_info->line->num_vertices = 2;
        callback_info->line->draw_edges = 1;
        // 0 - 500: open door
        // 500 - 3500: stay open
        // 3500 - 4000: close door
        vec3d* w = &callback_info->wall->vertices[0];
        if (wall_info->door_time > 0 && wall_info->door_time < 4000)
        {
            byte dt = 128;
            if (wall_info->door_time < 500)
                dt = wall_info->door_time * 128 / 500;
            else if (wall_info->door_time >= 3500)
                dt = (4000 - wall_info->door_time) * 128 / 500;
            byte t39 = 39 * dt >> 7;
            byte t40 = 40 * dt >> 7;
            byte t51 = 51 * dt >> 7;
            byte t52 = 52 * dt >> 7;
            byte t59 = 59 * dt >> 7;
            byte t60 = 60 * dt >> 7;
            clipped_portal = (polygon*)&_portal;
            clipped_portal->num_vertices = 4;
            clipped_portal->draw_edges = 0;
            // 3 to 14
            w->translate7(callback_info->line, dx, dy, 0, 51 - t51, 128, 76 - t51);
            // 24 to 26
            w->translate7(callback_info->line, dx, dy, 64 + t59, 64 - t40, 64 + t39, 64 + t59);
            memcpy(&clipped_portal->vertices[0], &callback_info->line->vertices[0], sizeof(vec3d) * 2);
            // 15 to 4
            w->translate7(callback_info->line, dx, dy, 128, 76 + t52, 0, 51 + t51);
            // 25 to 23
            w->translate7(callback_info->line, dx, dy, 64 - t60, 64 + t39, 64 - t40, 64 - t60);
            memcpy(&clipped_portal->vertices[2], &callback_info->line->vertices[0], sizeof(vec3d) * 2);
            // TODO: just clip, don't render
            clipped_portal = render_polygon(clipped_portal);
        }
        else
        {
            LOG("Door is closed.\n");
            w->translate7(callback_info->line, dx, dy, 0, 51, 128, 76);
            clipped_portal = 0;
        }
    }
    
    // enqueue next render job if it's a portal
    if (wall_info->adjacent_segment_index >= 0)
    {
//         LOG("clipped portal is %p.\n", clipped_portal);
//         if (clipped_portal)
//             LOG("   ...and has %d vertices.\n", clipped_portal->num_vertices);
        
        // test whether the portal needs to be smaller
        if (wall_info->adjacent_floor_height > callback_info->current_segment->floor_height || 
            wall_info->adjacent_ceiling_height < callback_info->current_segment->ceiling_height)
        {
            #ifdef PORT_ENABLED
                LOG("hiya!\n");
            #endif
            LOG("adjusting clipped portal because of different segment heights\n");
            LOG("this segment: %d %d %d\n", callback_info->segment_index, 
                callback_info->current_segment->floor_height,
                callback_info->current_segment->ceiling_height);
            LOG("adjacent segment: %d %d %d\n", wall_info->adjacent_segment_index,
                wall_info->adjacent_floor_height,
                wall_info->adjacent_ceiling_height);
            // we need to render some more lines and update the clipped portal, because floor
            // and ceiling heights are so that it's required
            clipped_portal = (polygon*)&_portal;
            clipped_portal->num_vertices = 4;
            clipped_portal->set_vertex(0, callback_info->p1, false);
            clipped_portal->set_vertex(1, callback_info->p0, false);
            clipped_portal->set_vertex(2, callback_info->p0, false);
            clipped_portal->set_vertex(3, callback_info->p1, false);
            clipped_portal->draw_edges = 0;
            if (wall_info->adjacent_floor_height > callback_info->current_segment->floor_height)
            {
                clipped_portal->vertices[0].y = (int32_t)wall_info->adjacent_floor_height << 14;
                clipped_portal->vertices[1].y = (int32_t)wall_info->adjacent_floor_height << 14;
                clipped_portal->draw_edges |= 1;
            }
            else
            {
                clipped_portal->vertices[0].y = (int32_t)callback_info->current_segment->floor_height << 14;
                clipped_portal->vertices[1].y = (int32_t)callback_info->current_segment->floor_height << 14;
            }
            if (wall_info->adjacent_ceiling_height < callback_info->current_segment->ceiling_height)
            {
                clipped_portal->vertices[2].y = (int32_t)wall_info->adjacent_ceiling_height << 14;
                clipped_portal->vertices[3].y = (int32_t)wall_info->adjacent_ceiling_height << 14;
                clipped_portal->draw_edges |= 4;
            }
            else
            {
                clipped_portal->vertices[2].y = (int32_t)callback_info->current_segment->ceiling_height << 14;
                clipped_portal->vertices[3].y = (int32_t)callback_info->current_segment->ceiling_height << 14;
            }
            transform_world_space_to_view_space(clipped_portal->vertices, 4);
            clipped_portal = render_polygon(clipped_portal);
        }
        
        // enqueue render job:
        // - render segment [adjacent_segment_index] with frustum defined by required number of normal vectors
        //   (or give up is not enough space is available - p will be NULL in that case!)
        if (clipped_portal)
        {
            if ((((segments_touched[wall_info->adjacent_segment_index >> 3] >> (wall_info->adjacent_segment_index & 7)) & 1) == 0))
            {
                #ifdef DEBUG
                    int temp = current_render_jobs->frustum_plane_count + next_render_jobs->frustum_plane_count + clipped_portal->num_vertices;
                    if (max_frustum_planes < temp)
                        max_frustum_planes = temp;
                #endif
                int p = next_render_jobs->add_job(wall_info->adjacent_segment_index, callback_info->segment_index, clipped_portal->num_vertices);
                if (p > -1)
                {
                    // fill in frustum planes...
                    for (int k = 0; k < clipped_portal->num_vertices; k++)
                    {
                        byte next_k = (k + 1) % clipped_portal->num_vertices;
                        LINE_COORDINATE_TYPE tv0[2], tv1[2];
                        project_vertex(clipped_portal->vertices[k], tv0);
                        project_vertex(clipped_portal->vertices[next_k], tv1);
                        shared_frustum_planes[p] = frustum_plane(tv0[0], tv0[1], tv1[0], tv1[1]);
                        p = (p + 1) % MAX_SHARED_FRUSTUM_PLANES;
                        
//                         vec3d v[2] = {clipped_portal->vertices[next_k], clipped_portal->vertices[k]};
//                         #ifdef FRUSTUM_PLANE_CALCULATION_PREMULTIPLY
//                             // determine if we need to premultiply vectors so that we don't get hiccups with 
//                             // vertices close to the viewer (close to the apex of the viewing frustum)
//                             byte max_log2 = 0;
//                             for (byte vi = 0; vi < 2; ++vi)
//                             {
//                                 for (byte vj = 0; vj < 3; ++vj)
//                                 {
//                                     byte test_log2 = log2(v[vi].v[vj]);
//                                     if (test_log2 > max_log2)
//                                         max_log2 = test_log2;
//                                 }
//                             }
//                             
//                             if (max_log2 < 16)
//                                 for (byte vi = 0; vi < 2; ++vi)
//                                     v[vi] <<= 16 - max_log2;
//                         #endif
//                         
//                         vec3d n = v[0].cross(v[1]);
//                         if (n.maximize_length_16())
//                         {
//                             // only add the frustum plane if it's not degenerate
// //                             shared_frustum_planes[p] = vec3d_16(n.x, n.y, n.z);
// //                             p = (p + 1) % MAX_SHARED_FRUSTUM_PLANES;
//                         }
                    }
                }
            }
        }
    }
    
    return true;
}

void render_segment(uint8_t segment_index, uint8_t frustum_count, uint8_t frustum_offset, uint8_t from_segment = 255)
{
    LOG("Rendering segment: %d (from: %d)\n", segment_index, from_segment);
    #ifdef ENABLE_MAP
        if (map_mode)
        {
            if ((((segments_seen[segment_index >> 3] >> (segment_index & 7)) & 1) == 0))
                return;
        }
        segments_seen[segment_index >> 3] |= 1 << (segment_index & 7);
    #endif
    segments_touched[segment_index >> 3] |= 1 << (segment_index & 7);
    #ifdef DEBUG
        segments_drawn++;
    #endif
    segment* current_segment = &temp_segment_buffer;
    // the camera already has a copy of the current segment, no need to fetch it again from flash memory
    if (segment_index == camera.current_segment_index)
        current_segment = &camera.current_segment;
    else
        memcpy_P(current_segment, &segments[segment_index], sizeof(segment));
    // iterate through every pair of adjacent vertices:
    // - construct polygon
    // - if it's a portal to the segment we're coming from: skip it
    // - clip against frustum (skip if invisible)
    // - if it's a wall: render outline
    // - if it's a portal: recursively render adjacent segment with updated frustum
    
    render_segment_callback_info callback_info;
    callback_info.wall = (polygon*)&_wall;
    callback_info.line = (polygon*)&_line;
    callback_info.segment_index = segment_index;
    callback_info.current_segment = current_segment;

    current_frustum_normal_count = frustum_count;
    for (uint8_t i = 0; i < frustum_count; ++i)
        shared_frustum_planes[(frustum_offset + i) % MAX_SHARED_FRUSTUM_PLANES].to_vec3d(&current_frustum_normals[i]);
    
    loop_through_segment_walls(segment_index, current_segment, true, &render_segment_callback, &callback_info);
    
    // render object at (1, 8, 0.5)
    if (segment_index == 0)
        render_sprite((int32_t)(1.0 * 65536), (int32_t)(4.5 * 65536), (int32_t)(8.0 * 65536));
    
    #ifdef ENABLE_SHOOTING
        // render shots in this segment
        for (byte i = 0; i < num_shots; i++)
        {
            if (shots[i].current_segment != segment_index)
                continue;
            polygon2 flare_polygon;
            flare_polygon.draw_edges = 1;
            flare_polygon.num_vertices = 2;
            for (byte k = 0; k < 3; k++)
            {
                flare_polygon.vertices[0].v[k] = (int32_t)shots[i].p[k] << 8;
                flare_polygon.vertices[1].v[k] = flare_polygon.vertices[0].v[k] + ((int32_t)shots[i].dir[k] << 5);
            }

            // transform flare to view space
            transform_world_space_to_view_space(flare_polygon.vertices, 2);
            
            render_polygon((polygon*)&flare_polygon, 2);
        }
    #endif
}

void update_scene()
{
    LOG("-----------------------\nupdating scene...\n");
    current_frame_start_millis = millis();
#ifdef SHOW_FRAME_TIME
    uint32_t start_micros = micros();
#endif
    memset(segments_touched, 0, SEGMENTS_TOUCHED_SIZE);
#ifdef DEBUG
    faces_touched = 0;
    faces_drawn = 0;
    segments_drawn = 0;
#endif
    *next_render_jobs = render_job_list();
    // prepare camera frustum
    // PRO TIP: put the planes first which discard the most faces (left and right)
//     shared_frustum_planes[0] = vec3d_16(  22969, 0, -23369);
//     shared_frustum_planes[1] = vec3d_16( -22969, 0, -23369);
//     shared_frustum_planes[2] = vec3d_16(0,  32268, -18689);
//     shared_frustum_planes[3] = vec3d_16(0, -32268, -18689);
    shared_frustum_planes[0] = frustum_plane(0, 0, 0, 767);       // left
    shared_frustum_planes[1] = frustum_plane(1343, 767, 1343, 0); // right
    shared_frustum_planes[2] = frustum_plane(0, 767, 1343, 767);  // bottom
    shared_frustum_planes[3] = frustum_plane(1343, 0, 0, 0);  // top
    current_render_jobs->frustum_plane_offset = 0;
    next_render_jobs->frustum_plane_offset = MAX_SHARED_FRUSTUM_PLANES - 1;
    // render current segment first
    render_segment(camera.current_segment_index, 4, 0);
    // now render all segments visible through portals until we're done
    byte max_loop_count = MAX_RENDER_ADJACENT_SEGMENTS;
    while (next_render_jobs->job_count > 0 && max_loop_count > 0)
    {
        max_loop_count--;
        render_job_list *temp = current_render_jobs;
        current_render_jobs = next_render_jobs;
        next_render_jobs = temp;
        *next_render_jobs = render_job_list();
        current_render_jobs->frustum_plane_offset = (current_render_jobs->frustum_plane_offset + 1) % MAX_SHARED_FRUSTUM_PLANES;
        next_render_jobs->frustum_plane_offset = (current_render_jobs->frustum_plane_offset + MAX_SHARED_FRUSTUM_PLANES - 1) % MAX_SHARED_FRUSTUM_PLANES;
        for (byte i = 0; i < current_render_jobs->job_count; i++)
        {
            const render_job& job = current_render_jobs->jobs[i];
            render_segment(job.segment, job.frustum_plane_count,
                           job.first_frustum_plane,
                           job.from_segment);
        }
    }
    
    uint32_t current_micros = micros();
    micros_per_frame = current_micros - last_micros;
    last_micros = current_micros;
    #ifdef ENABLE_MAP
        if (map_mode)
            gb.display.print(F("hold C to quit"));
        else
    #endif
    {
//         gb.display.print(" wi:");
//         gb.display.print(sizeof(wall_info));
//         gb.display.print(" rj01:");
//         gb.display.print(sizeof(render_jobs_0) + sizeof(render_jobs_1));
//         gb.display.print(" sfp:");
//         gb.display.print(sizeof(shared_frustum_planes));
//         gb.display.print(" tsb:");
//         gb.display.print(sizeof(temp_segment_buffer));
//         gb.display.print(" c:");
//         gb.display.print(sizeof(camera));
//         gb.display.print(" w:");
//         gb.display.print(sizeof(_wall));
//         gb.display.print(" p:");
//         gb.display.print(sizeof(_portal));
//         gb.display.print(" l:");
//         gb.display.print(sizeof(_line));
//         gb.display.print(" cp:");
//         gb.display.print(sizeof(clipped_polygon));
//         gb.display.print(" s:");
//         gb.display.print(sizeof(shots));
//         gb.display.print(" cdcbi:");
//         gb.display.print(sizeof(collision_detection_callback_info));
//         gb.display.print(" rscbi:");
//         gb.display.print(sizeof(render_segment_callback_info));
//         gb.display.print(" ");
        
//         wall_loop_info wall_info;
//         render_job_list render_jobs_0, render_jobs_1;
//         frustum_plane shared_frustum_planes[MAX_SHARED_FRUSTUM_PLANES];
//         render_job_list* next_render_jobs;
//         render_job_list* current_render_jobs;
//         segment temp_segment_buffer;
//         r_camera camera;
//         polygon4 _wall;
//         polygon4 _portal;
//         polygon2 _line;
//         polygon clipped_polygon;
        #ifdef SHOW_FRAME_TIME
            gb.display.print((micros() - start_micros) / 1000);
            gb.display.print(F(" ms "));
        // don't run the following line. it adds 864 bytes of code... 
// //             gb.display.print(1e6 / micros_per_frame);
// //             gb.display.println(F(" fps"));
        #endif
        #ifdef MONITOR_RAM
            int32_t ram_usage = max_ram_usage();
            gb.display.print(F("RAM: "));
            gb.display.print(ram_usage);
            gb.display.print(F(" ("));
            gb.display.print(ram_usage * 100 / 2048);
            gb.display.print(F("%)"));
        #endif
        #ifdef DEBUG
            gb.display.println();
            gb.display.println();
            gb.display.println();
            gb.display.println();
            gb.display.println();
            gb.display.println();
//             gb.display.print((float)camera.forward.x / 65536.0);
//             gb.display.print(" ");
//             gb.display.print((float)camera.forward.y / 65536.0);
//             gb.display.print(" ");
//             gb.display.print((float)camera.forward.z / 65536.0);
            gb.display.println();
            gb.display.print("CS:");
            gb.display.print(camera.current_segment_index);
            gb.display.print("/FD:");
            gb.display.print(faces_drawn);
//             gb.display.print("/MFP:");
//             gb.display.print(max_frustum_planes);
            
            #ifdef ENABLE_SHOOTING
                gb.display.print("/S:");
                gb.display.print(num_shots);
            #endif
        #endif
    }
}

void loop()
{
    if (gb.update())
    {
        handle_controls();
        move_player();
        update_scene();
    }
}
