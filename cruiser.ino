#include <Gamebuino.h>
Gamebuino gb;

const int32_t PI2 = 411775;
const int32_t PI1 = 205887;
const int PI180 = 1143;
const int PI128 = 1608;
const int32_t sqrt22 = 46340;

#define MONITOR_RAM

#define MAX_POLYGON_VERTICES 8
#define MAX_JOB_COUNT 3
#define MAX_SHARED_FRUSTUM_PLANES 22
#define MAX_RENDER_ADJACENT_SEGMENTS 8
//#define DEBUG
#define SHOW_FRAME_TIME
#define COLLISION_DETECTION
#define SHOW_TITLE_SCREEN
//#define ENABLE_STRAFE
//#define ENABLE_MAP
#define ENABLE_SHOOTING
#define MAX_SHOTS 12

#define SUB_PIXEL_ACCURACY

#define FRUSTUM_PLANE_CALCULATION_PREMULTIPLY
#define FIXED_POINT_SCALE 4

#ifndef LINE_COORDINATE_TYPE
#define LINE_COORDINATE_TYPE int
#endif

#ifndef LOG_ALREADY_DEFINED
#define LOG
#endif

//#define ROLL_SHIP
#define WOBBLE_SHIP
#define CLIP_TO_FRUSTUM
#ifdef ENABLE_MAP
    #define MIN_MAP_SCALE (1L << 16)
    #define MAX_MAP_SCALE (12L << 16)
#endif

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
int max_polygon_vertices = 0;
int max_frustum_planes = 0;
int segments_drawn = 0;
#endif

int32_t lsin(int32_t a);
int32_t lcos(int32_t a);
int32_t lsqrt(int32_t a);

byte SCREEN_RESOLUTION[2] = {LCDWIDTH, LCDHEIGHT};

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

    vec3d divby256()
    {
        return vec3d(x >> 8, y >> 8, z >> 8);
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

    void rotate(int32_t yaw)
    {
        int32_t s, c;
        vec3d temp(x, y, z);
        s = lsin(yaw) >> 8;
        c = lcos(yaw) >> 8;
        x = (temp.x >> 8) * c + (temp.z >> 8) * s;
        y = temp.y;
        z = (temp.x >> 8) * s + (temp.z >> 8) * c;
    }

    void rotate(int32_t pitch, int32_t yaw)
    {
        int32_t s, c;
        vec3d temp(x, y, z);
        s = lsin(pitch) >> 8;
        c = lcos(pitch) >> 8;
        x = temp.x;
        y = (-temp.z >> 8) * s + (temp.y >> 8) * c;
        z = (temp.z >> 8) * c + (temp.y >> 8) * s;

        temp = vec3d(x, y, z);
        s = lsin(yaw) >> 8;
        c = lcos(yaw) >> 8;
        x = (temp.x >> 8) * c + (temp.z >> 8) * s;
        y = temp.y;
        z = (temp.x >> 8) * s + (temp.z >> 8) * c;
    }

    void rotate(int32_t pitch, int32_t yaw, int32_t roll)
    {
        int32_t s, c;
        vec3d temp(x, y, z);
        s = lsin(pitch) >> 8;
        c = lcos(pitch) >> 8;
        x = temp.x;
        y = (-temp.z >> 8) * s + (temp.y >> 8) * c;
        z = (temp.z >> 8) * c + (temp.y >> 8) * s;

        temp = vec3d(x, y, z);
        s = lsin(yaw) >> 8;
        c = lcos(yaw) >> 8;
        x = (temp.x >> 8) * c + (temp.z >> 8) * s;
        y = temp.y;
        z = (temp.x >> 8) * s + (temp.z >> 8) * c;

        temp = vec3d(x, y, z);
        s = lsin(roll) >> 8;
        c = lcos(roll) >> 8;
        x = (temp.x >> 8) * c + (temp.y >> 8) * s;
        y = (-temp.x >> 8) * s + (temp.y >> 8) * c;
        z = temp.z;
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
#ifdef DEBUG
        if (max_polygon_vertices < num_vertices + 1)
            max_polygon_vertices = num_vertices + 1;
#endif
        if (num_vertices == MAX_POLYGON_VERTICES)
            return;
        memcpy(&vertices[num_vertices], &v, sizeof(vec3d));
        if (draw_edge)
            draw_edges |= (1 << num_vertices);
        num_vertices++;
    };

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
    };
};

struct old_segment
{
    byte floor_height;
    byte ceiling_height;
    byte x, y;
    byte vertex_and_portal_count;
    byte door_count;
    const byte* vertices;
    const word* portals;
    const word* doors;
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
    vec3d up8, forward8, right8;
    int32_t yaw, ayaw;
    int32_t pitch, apitch;
    int32_t a;
    #ifdef ENABLE_STRAFE
        int32_t xa, ya;
    #endif
    int width;
    int height;
    word current_segment;
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
};

struct render_job
{
    byte segment;
    byte from_segment;
    byte first_frustum_plane;
    byte frustum_plane_count;

    render_job()
        : segment(0)
        , from_segment(0)
        , first_frustum_plane(0)
        , frustum_plane_count(0)
    {}

    render_job(byte _segment, byte _from_segment, byte _first_frustum_plane, byte _frustum_plane_count)
        : segment(_segment)
        , from_segment(_from_segment)
        , first_frustum_plane(_first_frustum_plane)
        , frustum_plane_count(_frustum_plane_count)
    {}
};

struct render_job_list;

vec3d shared_frustum_planes[MAX_SHARED_FRUSTUM_PLANES];
render_job_list* next_render_jobs;
render_job_list* current_render_jobs;

struct render_job_list
{
    byte job_count;
    byte frustum_plane_count;
    byte frustum_plane_offset;
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
    int add_job(byte segment, byte from_segment, byte requested_frustum_plane_count)
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
    struct shot
    {
        uint16_t x, y, z;
        int8_t dx, dy, dz;
        uint16_t current_segment;
    };
#endif

#include "map.h"
#include "sprites.h"

#ifdef ENABLE_SHOOTING
    byte num_shots;
    shot shots[MAX_SHOTS];
#endif

render_job_list render_jobs_0, render_jobs_1;

bool allow_steering = true;
r_camera camera;

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

int32_t lsin(int32_t a)
{
    return (int32_t)(sin((float)a / 65536.0) * 65536.0);
}

int32_t lcos(int32_t a)
{
    return (int32_t)(cos((float)a / 65536.0) * 65536.0);
}

int32_t lsqrt(int32_t a)
{
    return (int32_t)(sqrt((float)a / 65536.0) * 65356.0);
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
    
    int x = x0 >> 4;
    int y = y0 >> 4;
    int x_end = x1 >> 4;
    for (; x <= x_end; ++x)
    {
        if (steep)
            gb.display.drawPixel(y, x);
        else
            gb.display.drawPixel(x, y);
        error += ddx;
        if (error < 0)
        {
            error += ddy;
            y += sy;
        }
    }
}

#else

#define draw_line_fixed_point(x0, y0, x1, y1) gb.display.drawLine(x0 >> 4, y0 >> 4, x1 >> 4, y1 >> 4)

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
    camera.at = vec3d((int32_t)(1.5 * 65536), (int32_t)(0.5 * 65536), (int32_t)(9.75 * 65536));
    //camera.at = vec3d((int32_t)(1.5 * 65536), (int32_t)(0.5 * 65536), (int32_t)(7.05 * 65536));
    camera.current_segment = 0;
    #ifdef ENABLE_SHOOTING
        num_shots = 0;
    #endif
    camera.wobble = 0.0;
    memset(segments_seen, 0, SEGMENTS_TOUCHED_SIZE);
    #ifdef ENABLE_MAP    
        map_mode = false;
    #endif
}

void setup()
{
#ifdef MONITOR_RAM
    stack_paint();
#endif
    gb.begin();
    //gb.setFrameRate(40);
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
                for (byte i = 0; i < 2; i++)
                {
                    if (num_shots < MAX_SHOTS)
                    {
                        vec3d p(camera.at);
                        p -= camera.up * 3277;
                        if (i == 0)
                            p += camera.right * 3277;
                        else
                            p -= camera.right * 3277;
                        shots[num_shots].x = p.x >> 8;
                        shots[num_shots].y = p.y >> 8;
                        shots[num_shots].z = p.z >> 8;
                        shots[num_shots].dx = camera.forward.x >> 8;
                        shots[num_shots].dy = camera.forward.y >> 8;
                        shots[num_shots].dz = camera.forward.z >> 8;
                        shots[num_shots].current_segment = camera.current_segment;
                        num_shots++;
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
                camera.a = 200000;
        }
    }
}

// returns wall index of collision or:
// - 0xfd for floor collision
// - 0xfe for ceiling collision
// - 0xff if no collision
byte collision_detection(word* segment, vec3d* from, vec3d* to, int32_t bump_distance)
{
    return 0xff;
    /*
    byte collided = 0xff;
    vec3d dir(*to - *from);

    // perform collision detection for floor and ceiling
    // default: heading for the floor
    vec3d n(0, -65536, 0);
    int32_t y = (int32_t)pgm_read_byte(&segments[*segment].floor_height) << 12;
    byte collision_code = 0xfd;
    if (dir.y > 0)
    {
        // heading for the ceiling
        n.y = 65536;
        y = (int32_t)pgm_read_byte(&segments[*segment].ceiling_height) << 12;
        collision_code = 0xfe;
    }
    vec3d p(0, y - ((n.y * 26) >> 8), 0);
    p -= *to;
    int32_t f = p.dot(n);
    if (f < 0)
    {
        // project trajectory onto floor/ceiling and continue
        *to += n * f;
        dir = *to - *from;
        collided = collision_code;
    }

    // perform collision detection on walls of current segment
    // - if we hit a wall, bump against it
    // - if we hit a portal, pass through it and update the camera's current segment
    byte num_points = pgm_read_byte(&segments[*segment].vertex_and_portal_count) >> 4;
    byte *vertices = (byte*)pgm_read_ptr(&segments[*segment].vertices);
    word *portals = (word*)pgm_read_ptr(&segments[*segment].portals);
    byte num_portals = pgm_read_byte(&segments[*segment].vertex_and_portal_count) & 0xf;
    byte next_portal_index = 0;

    word adjacent_segment;
    for (int i = 0; i < num_points; i++)
    {
        adjacent_segment = 0xffff;
        // test if it's a portal
        if (next_portal_index < num_portals)
        {
            // TODO: possible speedup here, don't pgm_read again and again
            if (i == pgm_read_word(&portals[next_portal_index]) >> 12)
            {
                adjacent_segment = pgm_read_word(&portals[next_portal_index]) & 0xfff;
                next_portal_index++;
            }
        }
        int x0 = pgm_read_byte(&segments[*segment].x) + (pgm_read_byte(&vertices[i % num_points]) >> 4);
        int z0 = pgm_read_byte(&segments[*segment].y) + (pgm_read_byte(&vertices[i % num_points]) & 0xf);
        int x1 = pgm_read_byte(&segments[*segment].x) + (pgm_read_byte(&vertices[(i + 1) % num_points]) >> 4);
        int z1 = pgm_read_byte(&segments[*segment].y) + (pgm_read_byte(&vertices[(i + 1) % num_points]) & 0xf);
        vec3d n(((int32_t)z0 - z1) << 16, 0, ((int32_t)x1 - x0) << 16);
        // perform collision detection if we're facing this wall
        // TODO: speed this up with binary search
        if (n.dot(dir) > 0)
        {
            vec3d wall_p((int32_t)x0 << 16, 0.0, (int32_t)z0 << 16);
            if (adjacent_segment == 0xffff)
            {
                // it's a wall
                // we actually have to normalize n here, because we want a fixed distance to the walls (bump_distance)
                // TODO: try normal_scale lookup, it might help a bit
                n.normalize();
                wall_p -= n * bump_distance;
                wall_p -= *to;
                int32_t f = wall_p.dot(n);
                if (f < 0)
                {
                    // project trajectory onto wall and continue
                    vec3d temp = *to + n * f;
                    if ((temp - vec3d((int32_t)x0 << 16, 0, (int32_t)z0 << 16)).dot(vec3d(((int32_t)x1 - x0) << 16, 0, ((int32_t)z1 - z0) << 16)) > 0 &&
                        (temp - vec3d((int32_t)x1 << 16, 0, (int32_t)z1 << 16)).dot(vec3d(((int32_t)x0 - x1) << 16, 0, ((int32_t)z0 - z1) << 16)) > 0)
                    {
                        // we're actually hitting this wall, not another wall that shares the same plane
                        *to = temp;
                        dir = *to - *from;
                        collided = i;
                    }
                }
            }
            else
            {
                // it's a portal
                wall_p -= *to;
                int32_t f = wall_p.dot(n);
                if (f < 0)
                {
                    // yup, we've crossed the portal, move camera to adjacent segment
                    //LOG("crossed a portal: from %d to %d\n", *segment, adjacent_segment);
                    *segment = adjacent_segment;
                    // If we don't return here, we'll get some portal jumping bugs, yay!
                    return collided;
                }
            }
        }
    }
    return collided;
    */
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
    /*
    if (camera.pitch < PI1)
        camera.pitch = (camera.pitch * 243) >> 8; // * 0.95
    else
        camera.pitch = PI2 - (((PI2 - camera.pitch) * 243) >> 8); // * 0.95
        */

    camera.up = vec3d(0, 65536, 0);
    camera.forward = vec3d(0, 0, -65536);

    // TODO: re-use sine and cosine values for both following rotations!
#ifdef ENABLE_MAP
    if (map_mode)
    {
        camera.up.rotate(camera.yaw);
        camera.forward.rotate(camera.yaw);
    }
    else
#endif
    {
#ifdef ROLL_SHIP
        camera.up.rotate(camera.pitch, camera.yaw, (-camera.ayaw * 256) >> 8);
        camera.forward.rotate(camera.pitch, camera.yaw, (-camera.ayaw * 256) >> 8);
#else
        camera.up.rotate(camera.pitch, camera.yaw);
        camera.forward.rotate(camera.pitch, camera.yaw);
#endif
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
        collision_detection(&camera.current_segment, &camera.at, &new_at, 16384);
#endif
    camera.at = new_at;

    camera.a = (camera.a * 230) >> 8;

    camera.wobble = (camera.wobble + (micros_per_frame >> 5)) % 65536;
    camera.wobble_sin = lsin((camera.wobble >> 8) * (PI2 >> 8));
}

byte count_zero_bits_from_msb(int32_t l)
{
    // shift out sign bit
    byte test_bit = (l >> 31) & 1;
    l <<= 1;
    byte count = 0;
    while (count < 15)
    {
        if (((l >> 31) & 1) != test_bit)
            break;
        ++count;
        l <<= 1;
    }
    return 15 - count;
}

byte count_zero_bits_from_lsb(int32_t l)
{
    byte count = 0;
    while (count < 16)
    {
        if (l & 1)
            break;
        ++count;
        l >>= 1;
    }
    return 16 - count;
}

void clip_polygon_against_plane(polygon* result, const vec3d& clip_plane_normal, polygon* source)
{
    result->num_vertices = 0;
    result->draw_edges = 0;
    vec3d *v0 = NULL;
    vec3d *v1 = NULL;
    int32_t d0, d1;
    bool flag0, flag1;
    for (int i = 0; i < source->num_vertices; i++)
    {
        if (v1)
        {
            v0 = v1;
            d0 = d1;
            flag0 = flag1;
        }
        else
        {
            v0 = &source->vertices[i];
            // default: 16.16 * 16.16 => 16.8 * 16.8 => 16.16
            // optimum: 8.16 * 8.16 => 8.12 * 8.12 => 16.16
            d0 = v0->dot(clip_plane_normal);
            // CAVEAT: d >= 0 would result in degenerate faces which we would spend time on!
            flag0 = (d0 > 0);
        }
        v1 = &source->vertices[(i + 1) % source->num_vertices];
        d1 = v1->dot(clip_plane_normal);
        flag1 = (d1 > 0);

        vec3d intersection;
        if (flag0 ^ flag1)
        {
            // d0 and d1 are 8.16
            int32_t f = (-d1 << 8) / (d0 - d1);
            int32_t f1 = 256 - f;
            intersection = vec3d();
            for (byte j = 0; j < 3; ++j)
                intersection.v[j] = (v0->v[j] >> 8) * f + (v1->v[j] >> 8) * f1;
        }
        if (flag0)
        {
            result->add_vertex(*v0, (source->draw_edges >> i) & 1);
            if (!flag1)
                result->add_vertex(intersection, false);
        }
        else if (flag1)
            result->add_vertex(intersection, (source->draw_edges >> i) & 1);
    }
}

void clip_line_against_plane(polygon* result, const vec3d& clip_plane_normal, polygon* source)
{
    result->num_vertices = 0;
    float d0 = source->vertices[0].dot(clip_plane_normal);
    float d1 = source->vertices[1].dot(clip_plane_normal);
    bool flag0 = (d0 > 0.0);
    bool flag1 = (d1 > 0.0);
    // TODO: this is kind of quick and dirty, the line will be discarded unless it is completely
    // visible... maybe add a calculation of the intersection here, but it works alright for flares
    if (flag0 && flag1)
    {
        result->num_vertices = 2;
        memcpy(result->vertices, source->vertices, sizeof(vec3d) * 2);
    }
}

void transform_world_space_to_view_space(vec3d* v, byte count = 1)
{
    for (byte i = 0; i < count; ++i)
    {
        vec3d* r =&v[i];
        vec3d s(*r);
        for (byte j = 0; j < 3; ++j)
            s.v[j] = (s.v[j] - camera.at.v[j]) >> 8;
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

void flash_memcpy(void* destination, void* source, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        *((uint8_t*)destination++) = pgm_read_byte(source++);
}

void render_segment(byte segment_index, byte frustum_count, byte frustum_offset, byte from_segment = 255)
{
    #ifdef ENABLE_MAP
        if (map_mode)
        {
            if ((((segments_seen[segment_index >> 3] >> (segment_index & 7)) & 1) == 0))
                return;
        }
    #endif
    segments_touched[segment_index >> 3] |= 1 << (segment_index & 7);
    segments_seen[segment_index >> 3] |= 1 << (segment_index & 7);
    #ifdef DEBUG
        segments_drawn++;
    #endif
    segment current_segment;
    flash_memcpy(&current_segment, &segments[segment_index], sizeof(segment));
    // iterate through every pair of adjacent vertices:
    // - construct polygon
    // - if it's a portal to the segment we're coming from: skip it
    // - clip against frustum (skip if invisible)
    // - if it's a wall: render outline
    // - if it's a portal: recursively render adjacent segment with updated frustum
    byte num_points = current_segment.vertex_count;
    byte vertices = current_segment.vertices;
    word portals = current_segment.portals;
    byte num_portals = current_segment.portal_count;
    byte next_portal_index = 0;
    word next_portal_point = pgm_read_word(&portals[next_portal_index]);
    word adjacent_segment;
    int x0, z0, x1, z1;
    int x1_z1_from_point = -1;
    byte floor_height = current_segment.floor_height;
    byte ceiling_height = pgm_read_byte(&segments[segment_index].ceiling_height);
    // wall has the geometry for the current wall of the current segment
    polygon wall;
    for (int i = 0; i < num_points; i++)
    {
        // portal_polygon has the geometry for the portal, if it's different from the wall
        polygon portal_polygon;
        // portal points to wall if the wall's geometry is equal to the portal's geometry
        // (as in most cases) OR it points to portal_polygon if we need two drawing passes
        polygon* portal = &wall;
        
        adjacent_segment = 0xffff;
        segment adjancent_segment_;
        // test if it's a portal
        if (next_portal_index < num_portals && i == next_portal_point >> 12)
        {
            adjacent_segment = next_portal_point & 0xfff;
            flash_memcpy(&adjacent_segment, &segments[adjacent_segment], sizeof(segment));
            next_portal_index++;
            next_portal_point = pgm_read_word(&portals[next_portal_index]);
            // early face culling: skip the portal if it's leading to a segment we have already touched in this frame
            if ((((segments_touched[adjacent_segment >> 3] >> (adjacent_segment & 7)) & 1) == 1))
                continue;
        }

        bool also_drew_previous_wall = (x1_z1_from_point == ((i + 1) % num_points));
        if (also_drew_previous_wall)
        {
            // re-use the coordinates we already read to save calls to pgm_read_byte
            x0 = x1;
            z0 = z1;
            // if we have constructed a wall before, let's re-use the previously transformed vertices
            wall.vertices[1] = wall.vertices[0];
            wall.vertices[2] = wall.vertices[3];
        }
        else
        {
            x0 = pgm_read_byte(&segments[segment_index].x) + (pgm_read_byte(&vertices[i % num_points]) >> 4);
            z0 = pgm_read_byte(&segments[segment_index].y) + (pgm_read_byte(&vertices[i % num_points]) & 0xf);
        }

        x1_z1_from_point = (i + 1) % num_points;
        x1 = pgm_read_byte(&segments[segment_index].x) + (pgm_read_byte(&vertices[x1_z1_from_point]) >> 4);
        z1 = pgm_read_byte(&segments[segment_index].y) + (pgm_read_byte(&vertices[x1_z1_from_point]) & 0xf);

        //vec3d n(z0 - z1, 0.0, x1 - x0);
        //n.normalize();
        // backface culling: skip this wall if it's facing away from the camera
        // Update: joke's on us, all faces are facing the viewer in a convex segment
        //if (camera.forward.x * n.x + camera.forward.z * n.z < -0.3)
        //  continue;

        #ifdef DEBUG
            faces_touched += 1;
        #endif

        vec3d p0((int32_t)x0 << 16, (int32_t)floor_height << 12, (int32_t)z0 << 16);
        vec3d p1((int32_t)x1 << 16, (int32_t)floor_height << 12, (int32_t)z1 << 16);

        // construct wall polygon
        wall.num_vertices = 4;
        if (also_drew_previous_wall)
        {
            wall.set_vertex(0, p1, adjacent_segment == 0xffff);
            wall.set_vertex(3, p1, false);
        }
        else
        {
            wall.set_vertex(0, p1, adjacent_segment == 0xffff);
            #ifdef ENABLE_MAP
                wall.set_vertex(1, p0, !map_mode);
                wall.set_vertex(2, p0, (!map_mode) && (adjacent_segment == 0xffff));
            #else
                wall.set_vertex(1, p0, true);
                wall.set_vertex(2, p0, adjacent_segment == 0xffff);
            #endif
            // don't draw second vertical edge, it will be drawn by the adjacent wall
            wall.set_vertex(3, p1, false);
        }
        wall.vertices[2].y = (int32_t)ceiling_height << 12;
        wall.vertices[3].y = (int32_t)ceiling_height << 12;

        byte adjacent_floor_height = 0;
        byte adjacent_ceiling_height = 16;

        if (adjacent_segment != 0xffff)
        {
            // this wall is a portal to another segment...
            // if floor and ceiling heights are the same for this segment and the adjacent segment,
            // (or the adjacent segment's floor is lower and it's ceiling is higher than this segment)
            // everything's just fine and dandy.
            // HOWEVER if there's a difference:
            // - (adjacent_floor_height > floor_height) || (adjacent_ceiling_height < ceiling_height)
            // we'll have to render this wall in two passes (portal == &portal_polygon):
            // - the bigger polygon (wall)
            //   - with default lines
            //   - NOT defining a new portal, it's just cosmetic ;-)
            // - the smaller polygon (portal_polygon)
            //   - with only top and bottom lines, if required (different from bigger polygon)
            //   - defining the portal and thus the frustum used for the adjacent segment
            adjacent_floor_height = adjacent_segment_.floor_height;
            adjacent_ceiling_height = pgm_read_byte(&segments[adjacent_segment].ceiling_height);
            if (adjacent_floor_height != floor_height)
                wall.draw_edges |= 1;
            if (adjacent_ceiling_height != ceiling_height)
                wall.draw_edges |= 4;
            if (adjacent_floor_height > floor_height || adjacent_ceiling_height < ceiling_height)
            {
                portal = &portal_polygon;
                portal_polygon.num_vertices = 4;
                portal_polygon.set_vertex(0, p1, false);
                portal_polygon.set_vertex(1, p0, false);
                portal_polygon.set_vertex(2, p0, false);
                portal_polygon.set_vertex(3, p1, false);
                if (adjacent_floor_height > floor_height)
                {
                    portal_polygon.vertices[0].y = (int32_t)adjacent_floor_height << 12;
                    portal_polygon.vertices[1].y = (int32_t)adjacent_floor_height << 12;
                    portal_polygon.draw_edges |= 1;
                }
                else
                {
                    portal_polygon.vertices[0].y = (int32_t)floor_height << 12;
                    portal_polygon.vertices[1].y = (int32_t)floor_height << 12;
                }
                if (adjacent_ceiling_height < ceiling_height)
                {
                    portal_polygon.vertices[2].y = (int32_t)adjacent_ceiling_height << 12;
                    portal_polygon.vertices[3].y = (int32_t)adjacent_ceiling_height << 12;
                    portal_polygon.draw_edges |= 4;
                }
                else
                {
                    portal_polygon.vertices[2].y = (int32_t)ceiling_height << 12;
                    portal_polygon.vertices[3].y = (int32_t)ceiling_height << 12;
                }
                transform_world_space_to_view_space(portal_polygon.vertices, 4);
            }
        }
        
        // transform wall to view space
        // TODO: This is done twice as often as required because we don't
        // re-use transformed vertices from the last wall
        if (also_drew_previous_wall)
        {
            transform_world_space_to_view_space(&wall.vertices[0]);
            transform_world_space_to_view_space(&wall.vertices[3]);
        }
        else
            transform_world_space_to_view_space(wall.vertices, 4);

        byte passes_required = 1;
        if (portal == &portal_polygon)
            passes_required = 2;
        polygon clipped_wall;
        // clip wall polygon against frustum
        polygon* p_wall = &wall;
        for (byte pass = 0; pass < passes_required; pass++)
        {
            p_wall = &wall;
            if (pass == 1)
                p_wall = &portal_polygon;
                
            #ifdef CLIP_TO_FRUSTUM
                #ifdef ENABLE_MAP
                    if (!map_mode)
                #endif
                {
                    polygon* p_clipped_wall = &clipped_wall;
                    polygon* temp;
                    for (int k = 0; k < frustum_count; k++)
                    {
                        clip_polygon_against_plane(p_clipped_wall, shared_frustum_planes[(frustum_offset + k) % MAX_SHARED_FRUSTUM_PLANES], p_wall);
                        // break from loop if we have a degenerate polygon
                        temp = p_wall; p_wall = p_clipped_wall; p_clipped_wall = temp;
                        if (p_wall->num_vertices < 3)
                            break;
                    }
                }
            #endif

            // skip this polygon if too many vertices have been clipped away
            if (p_wall->num_vertices < 3)
                continue;

            #ifdef DEBUG
                faces_drawn += 1;
            #endif

            int first[2];
            int last[2];

            for (int k = 0; k < p_wall->num_vertices; k++)
            {
//                 if (!(segment_index == 1 && i == 3))
//                     continue;
                LINE_COORDINATE_TYPE tv[2];
                #ifdef ENABLE_MAP
                    if (map_mode)
                    {
                        tv[0] = ((((map_scale >> 8) * ((p_wall->vertices[k].x + map_dx) >> 8)) >> 16) + 42) << FIXED_POINT_SCALE;
                        tv[1] = ((((map_scale >> 8) * ((p_wall->vertices[k].z + map_dy) >> 8)) >> 16) + 34) << FIXED_POINT_SCALE;
                    }
                    else
                #endif
                {
                    int32_t z1 = (((-1L) << 24) / p_wall->vertices[k].z);
                    tv[0] = (((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE) + ((((p_wall->vertices[k].v[0] * ((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
                    tv[1] = (((SCREEN_RESOLUTION[1] - 1) << FIXED_POINT_SCALE) - ((((p_wall->vertices[k].v[1] * ((SCREEN_RESOLUTION[0] - 1) << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
                }

                if (k == 0)
                    memcpy(first, tv, 4);
                else
                {
                    if ((p_wall->draw_edges >> (k - 1)) & 1)
                        draw_line_fixed_point(last, tv);
                }
                memcpy(last, tv, 4);
            }
            if ((p_wall->draw_edges >> (p_wall->num_vertices - 1)) & 1)
                draw_line_fixed_point(last, first);

            // enqueue next render job if it's a portal
            if ((pass == passes_required - 1) && (adjacent_segment != 0xffff))
            {
                // enqueue render job:
                // - render segment [adjacent_segment] with frustum defined by required number of normal vectors
                //   (or give up is not enough space is available - p will be NULL in that case!)
                if ((((segments_touched[adjacent_segment >> 3] >> (adjacent_segment & 7)) & 1) == 0))
                {
                    #ifdef DEBUG
                        int temp = current_render_jobs->frustum_plane_count + next_render_jobs->frustum_plane_count + portal->num_vertices;
                        if (max_frustum_planes < temp)
                            max_frustum_planes = temp;
                    #endif
                    int p = next_render_jobs->add_job(adjacent_segment, segment_index, portal->num_vertices);
                    if (p > -1)
                    {
                        // fill in frustum planes...
                        for (int k = 0; k < portal->num_vertices; k++)
                        {
                            byte next_k = (k + 1) % portal->num_vertices;
                            vec3d v[2] = {portal->vertices[next_k], portal->vertices[k]};
                            #ifdef FRUSTUM_PLANE_CALCULATION_PREMULTIPLY
                                // determine if we need to premultiply vectors so that we don't get mixups with 
                                // vertices close to the viewer (close to the apex of the viewing frustum)
                                byte max_log2 = 0;
                                for (byte vi = 0; vi < 2; ++vi)
                                {
                                    for (byte vj = 0; vj < 3; ++vj)
                                    {
                                        byte test_log2 = log2(v[vi].v[vj]);
                                        if (test_log2 > max_log2)
                                            max_log2 = test_log2;
                                    }
                                }
                                
                                if (max_log2 < 16)
                                    for (byte vi = 0; vi < 2; ++vi)
                                        v[vi] <<= 16 - max_log2;
                            #endif
                            
                            shared_frustum_planes[p] = v[0].cross(v[1]);
                            p = (p + 1) % MAX_SHARED_FRUSTUM_PLANES;
                        }
                    }
                }
            }

        }

    }

}

void update_scene()
{
    LOG("-----------------------\nupdating scene...\n");
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
    shared_frustum_planes[0] = vec3d(  75366, 0, -76677);
    shared_frustum_planes[1] = vec3d( -75366, 0, -76677);
    shared_frustum_planes[2] = vec3d(0,  132383, -76677);
    shared_frustum_planes[3] = vec3d(0, -132383, -76677);
    current_render_jobs->frustum_plane_offset = 0;
    next_render_jobs->frustum_plane_offset = MAX_SHARED_FRUSTUM_PLANES - 1;
    // render current segment first
    render_segment(camera.current_segment, 4, 0);
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
        #ifdef SHOW_FRAME_TIME
            gb.display.print((micros() - start_micros) / 1000.0);
            gb.display.print(F(" ms / "));
            gb.display.print(1e6 / micros_per_frame);
            gb.display.println(F(" fps"));
        #endif
        #ifdef MONITOR_RAM
            int32_t ram_usage = max_ram_usage();
            gb.display.print(F("RAM: "));
            gb.display.print(ram_usage);
            gb.display.print(F(" ("));
            gb.display.print(ram_usage * 100 / 2048);
            gb.display.print(F("%)"));
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


