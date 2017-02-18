#include <Gamebuino.h>
Gamebuino gb;

const long PI2 = 411775;
const long PI1 = 205887;
const int PI180 = 1143;
const int PI128 = 1608;
const long sqrt22 = 46340;

#define MONITOR_RAM

//#define DEBUG_REALLY_HARD_FIXED_POINT_LINE_DRAWING_PROBLEM

#define MAX_POLYGON_VERTICES 8
#define MAX_JOB_COUNT 3
#define MAX_SHARED_FRUSTUM_PLANES 22
#define MAX_RENDER_ADJACENT_SEGMENTS 8
#define MAX_SHOTS 12
#define DEBUG
#define SHOW_FRAME_TIME
#define COLLISION_DETECTION
#define SHOW_TITLE_SCREEN

#define SUB_PIXEL_ACCURACY

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
//#define VARIABLE_ROOM_HEIGHT
#define MIN_MAP_SCALE (1L << 16)
#define MAX_MAP_SCALE (8L << 16)

unsigned long last_micros = 0;
unsigned long micros_per_frame = 0;
bool map_mode;
long map_scale;
long map_dx, map_dy;
#ifdef DEBUG
int faces_touched = 0;
int faces_drawn = 0;
int max_polygon_vertices = 0;
int max_frustum_planes = 0;
int segments_drawn = 0;
#endif

long lsin(long a);
long lcos(long a);
long lsqrt(long a);

struct vec3d
{
    long x, y, z;

    vec3d()
        : x(0), y(0), z(0)
    {}

    vec3d(long _x, long _y, long _z)
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
        x += other.x;
        y += other.y;
        z += other.z;
    }

    vec3d operator -(const vec3d& other)
    {
        return vec3d(x - other.x, y - other.y, z - other.z);
    }

    void operator -=(const vec3d& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
    }

    vec3d operator *(long d)
    {
        d >>= 8;
        return vec3d((x >> 8) * d, (y >> 8) * d, (z >> 8) * d);
    }

    void operator *=(long d)
    {
        d >>= 8;
        x = (x >> 8) * d;
        y = (y >> 8) * d;
        z = (z >> 8) * d;
    }

    long dot(const vec3d& other)
    {
        return (x >> 8) * (other.x >> 8) +
               (y >> 8) * (other.y >> 8) +
               (z >> 8) * (other.z >> 8);
    }

    vec3d cross(const vec3d& other)
    {
        return vec3d(((y >> 8) * (other.z >> 8)) - ((z >> 8) * (other.y >> 8)),
                     ((z >> 8) * (other.x >> 8)) - ((x >> 8) * (other.z >> 8)),
                     ((x >> 8) * (other.y >> 8)) - ((y >> 8) * (other.x >> 8)));
    }

    long length()
    {
        return lsqrt(dot(*this));
    }

    void normalize()
    {
        long l = (1L << 24) / length();
        x = (x >> 8) * l;
        y = (y >> 8) * l;
        z = (z >> 8) * l;
    }

    void rotate(long yaw)
    {
        long s, c;
        vec3d temp(x, y, z);
        s = lsin(yaw) >> 8;
        c = lcos(yaw) >> 8;
        x = (temp.x >> 8) * c + (temp.z >> 8) * s;
        y = temp.y;
        z = (temp.x >> 8) * s + (temp.z >> 8) * c;
    }

    void rotate(long pitch, long yaw)
    {
        long s, c;
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

    void rotate(long pitch, long yaw, long roll)
    {
        long s, c;
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

struct segment
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
    long yaw, ayaw;
    long pitch, apitch;
    long a;
    long xa, ya;
    int width;
    int height;
    word current_segment;
    long wobble;
    long wobble_sin;
    long wobble_shift;

    r_camera()
        : yaw(0)
        , ayaw(0)
        , pitch(0)
        , apitch(0)
        , a(0)
        , xa(0)
        , ya(0)
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

struct shot
{
    word x, y, z;
    byte dx, dy, dz;
    word current_segment;
};

#include "map.h"
#include "sprites.h"

byte num_shots;
shot shots[MAX_SHOTS];

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

long lsin(long a)
{
    return (long)(sin((float)a / 65536.0) * 65536.0);
}

long lcos(long a)
{
    return (long)(cos((float)a / 65536.0) * 65536.0);
}

long lsqrt(long a)
{
    return (long)(sqrt((float)a / 65536.0) * 65356.0);
}

#ifdef SUB_PIXEL_ACCURACY

void draw_line_fixed_point(int x0, int y0, int x1, int y1) {
    LOG("draw line from %d, %d to %d, %d...\n", x0, y0, x1, y1);
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

    int dx = x1 - x0;
    int dy = y1 - y0;
    long dx2 = (long)dx << 5;
    long dy2 = (long)dy << 5;
    long error = (dx * (y0 & 0xf) + dy * (8 - (x0 & 0xf))) << 1;
    int x = x0 >> 4;
    int y = y0 >> 4;

    if (steep)
    {
        for (int count = ((x1 - x0) >> 4) + 1; count; --count)
        {
            for (; error > dx2; ++y, error -= dx2);
            for (; error < 0; --y, error += dx2);
            gb.display.drawPixel(y, x);
            ++x, error += dy2;
        }
    }
    else
    {
        for (int count = ((x1 - x0) >> 4) + 1; count; --count)
        {
            for (; error > dx2; ++y, error -= dx2);
            for (; error < 0; --y, error += dx2);
            gb.display.drawPixel(x, y);
            ++x, error += dy2;
        }
    }
}

#else

#define draw_line_fixed_point(x0, y0, x1, y1) gb.display.drawLine(x0, y0, x1, y1)

#endif

void title_screen()
{
#ifdef SHOW_TITLE_SCREEN
    gb.titleScreen(F("CRUISER"));
    allow_steering = false;
#endif
    gb.battery.show = false;
    camera = r_camera();
    camera.at = vec3d((long)(1.5 * 65536), (long)(0.5 * 65536), (long)(9.75 * 65536));
    camera.current_segment = 0;
    num_shots = 0;
    camera.wobble = 0.0;
    memset(segments_seen, 0, SEGMENTS_TOUCHED_SIZE);
    map_mode = false;
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
        title_screen();
    if (b_pressed)
    {
        if (map_mode)
        {
            map_scale -= 8192;
            if (map_scale < MIN_MAP_SCALE)
                map_scale = MIN_MAP_SCALE;
        }
        else
        {
            /*
            for (byte i = 0; i < 2; i++)
            {
                if (num_shots < MAX_SHOTS)
                {
                    vec3d p(camera.at);
                    p -= camera.up * 0.05;
                    if (i == 0)
                        p += camera.right * 0.05;
                    else
                        p -= camera.right * 0.05;
                    shots[num_shots].x = (word)(p.x * 256.0);
                    shots[num_shots].y = (word)(p.y * 256.0);
                    shots[num_shots].z = (word)(p.z * 256.0);
                    shots[num_shots].dx = (word)(camera.forward.x * 127.0 + 127.0);
                    shots[num_shots].dy = (word)(camera.forward.y * 127.0 + 127.0);
                    shots[num_shots].dz = (word)(camera.forward.z * 127.0 + 127.0);
                    shots[num_shots].current_segment = camera.current_segment;
                    num_shots++;
                }
            }
            */
        }
    }

    if (gb.buttons.released(BTN_A))
        allow_steering = true;
    if (allow_steering)
    {
        if (map_mode)
        {
            long map_move = ((1L << 24) / (map_scale >> 8)) * (micros_per_frame >> 10) >> 5;
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
                map_scale += 8192;
                if (map_scale > MAX_MAP_SCALE)
                    map_scale = MAX_MAP_SCALE;
            }
        }
        else
        {
            if (gb.buttons.repeat(BTN_LEFT, 1))
            {
                if (b_pressed)
                    camera.xa = -200000;
                else
                    camera.ayaw = 80000;
            }
            if (gb.buttons.repeat(BTN_RIGHT, 1))
            {
                if (b_pressed)
                    camera.xa = 200000;
                else
                    camera.ayaw = -80000;
            }
            if (gb.buttons.repeat(BTN_DOWN, 1))
            {
                if (b_pressed)
                    camera.ya = -200000;
                else
                    camera.apitch = 80000;
            }
            if (gb.buttons.repeat(BTN_UP, 1))
            {
                if (b_pressed)
                    camera.ya = 200000;
                else
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
byte collision_detection(word* segment, vec3d* from, vec3d* to, long bump_distance)
{
    byte collided = 0xff;
    vec3d dir(*to - *from);

    // perform collision detection for floor and ceiling
    // default: heading for the floor
    vec3d n(0, -65536, 0);
#ifdef VARIABLE_ROOM_HEIGHT
    float y = (float)pgm_read_byte(&segments[*segment].floor_height) / 16.0;
#else
    long y = 0;
#endif
    byte collision_code = 0xfd;
    if (dir.y > 0)
    {
        // heading for the ceiling
        n.y = 65536;
#ifdef VARIABLE_ROOM_HEIGHT
        y = (float)pgm_read_byte(&segments[*segment].ceiling_height) / 16.0;
#else
        y = 65536;
#endif
        collision_code = 0xfe;
    }
    vec3d p(0, y - ((n.y * 26) >> 8), 0);
    p -= *to;
    long f = p.dot(n);
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
        vec3d n(((long)z0 - z1) << 16, 0, ((long)x1 - x0) << 16);
        // perform collision detection if we're facing this wall
        // TODO: speed this up with binary search
        if (n.dot(dir) > 0)
        {
            if (adjacent_segment == 0xffff)
            {
                // it's a wall
                vec3d wall_p((long)x0 << 16, 0, (long)z0 << 16);
                // we actually have to normalize n here, because we want a fixed distance to the walls (bump_distance)
                // TODO: try normal_scale lookup, it might help a bit
                n.normalize();
                wall_p -= n * bump_distance;
                wall_p -= *to;
                long f = wall_p.dot(n);
                if (f < 0)
                {
                    // project trajectory onto wall and continue
                    vec3d temp = *to + n * f;
                    if ((temp - vec3d((long)x0 << 16, 0, (long)z0 << 16)).dot(vec3d(((long)x1 - x0) << 16, 0, ((long)z1 - z0) << 16)) > 0 &&
                        (temp - vec3d((long)x1 << 16, 0, (long)z1 << 16)).dot(vec3d(((long)x0 - x1) << 16, 0, ((long)z0 - z1) << 16)) > 0)
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
                vec3d wall_p((long)x0 << 16, 0.0, (long)z0 << 16);
                wall_p -= *to;
                long f = wall_p.dot(n);
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
    if (camera.pitch < PI1)
        camera.pitch = (camera.pitch * 243) >> 8; // * 0.95
    else
        camera.pitch = PI2 - (((PI2 - camera.pitch) * 243) >> 8); // * 0.95

    camera.up = vec3d(0, 65536, 0);
    camera.forward = vec3d(0, 0, -65536);

    // TODO: re-use sine and cosine values for both following rotations!
#ifdef ROLL_SHIP
    camera.up.rotate(camera.pitch, camera.yaw, (-camera.ayaw * 256) >> 8);
    camera.forward.rotate(camera.pitch, camera.yaw, (-camera.ayaw * 256) >> 8);
#else
    camera.up.rotate(camera.pitch, camera.yaw);
    camera.forward.rotate(camera.pitch, camera.yaw);
#endif

    camera.right = camera.forward.cross(camera.up);

    camera.up8 = camera.up.divby256();
    camera.forward8 = camera.forward.divby256();
    camera.right8 = camera.right.divby256();

    //    vec3d new_at = camera.at + camera.forward * camera.a + camera.up * camera.ya + camera.right * camera.xa;
    vec3d new_at = camera.at + camera.forward * (camera.a * (micros_per_frame >> 10) >> 10);
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

#ifdef COLLISION_DETECTION
    if (camera.a != 0 || camera.xa != 0 || camera.ya != 0)
        collision_detection(&camera.current_segment, &camera.at, &new_at, 16384);
#endif
    camera.at = new_at;

    camera.a = (camera.a * 230) >> 8;

    camera.wobble = (camera.wobble + (micros_per_frame >> 5)) % 65536;
    camera.wobble_sin = lsin((camera.wobble >> 8) * (PI2 >> 8));

    // move shots
    for (int i = 0; i < num_shots; i++)
    {
        vec3d p((float)shots[i].x / 256.0, (float)shots[i].y / 256.0, (float)shots[i].z / 256.0);
        vec3d dir(
            ((float)(shots[i].dx) - 127.0) / 127.0,
            ((float)(shots[i].dy) - 127.0) / 127.0,
            ((float)(shots[i].dz) - 127.0) / 127.0);
        vec3d new_p(p + dir * 4.0 * micros_per_frame * 1e-6);
        byte wall_collided = collision_detection(&shots[i].current_segment, &p, &new_p, 0.1);
        if (wall_collided != 0xff)
        {
            // flare hit a wall, remove the shot
            if (num_shots > 1)
                memcpy(&shots[i], &shots[num_shots - 1], sizeof(shot));
            num_shots--;
            i--;
        }
        else
        {
            shots[i].x = (word)(new_p.x * 256);
            shots[i].y = (word)(new_p.y * 256);
            shots[i].z = (word)(new_p.z * 256);
        }
    }
}

void clip_polygon_against_plane(polygon* result, const vec3d& clip_plane_normal, polygon* source)
{
    result->num_vertices = 0;
    result->draw_edges = 0;
    vec3d *v0 = NULL;
    vec3d *v1 = NULL;
    long d0, d1;
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
            long f = (-d1 << 8) / (d0 - d1);
            long f1 = 256 - f;
            intersection = vec3d((v0->x >> 8) * f + (v1->x >> 8) * f1,
                                 (v0->y >> 8) * f + (v1->y >> 8) * f1,
                                 (v0->z >> 8) * f + (v1->z >> 8) * f1);
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

void transform_world_space_to_view_space(vec3d* v)
{
    vec3d s(*v);
    s.x -= camera.at.x;
    s.y -= camera.at.y;
    s.z -= camera.at.z;
    if (!map_mode)
    {
        s.x >>= 8;
        s.y >>= 8;
        s.z >>= 8;
        v->x =  s.x * (camera.right8.x  ) + s.y * (camera.right8.y  ) + s.z * (camera.right8.z  );
        v->y =  s.x * (camera.up8.x     ) + s.y * (camera.up8.y     ) + s.z * (camera.up8.z     );
        v->z = -s.x * (camera.forward8.x) - s.y * (camera.forward8.y) - s.z * (camera.forward8.z);
        #ifdef WOBBLE_SHIP
            // add wobble
            v->y += (camera.wobble_sin * 13) >> 8;
        #endif
    }
    else
    {
        *v = s;
    }
}

void render_sprite(byte sprite_index, vec3d p, byte frustum_count, byte frustum_offset)
{   /*
        const sprite* s = &sprites[sprite_index];
        for (int i = 0; i < s->polygon_count; i++)
        {
           // construct polygon
           polygon pl;
           polygon clipped_polygon;

           vec3d x(0.02, 0.0, 0.0);
           vec3d y(0.0, 0.02, 0.0);
           x.rotate(camera.wobble * PI2);
           const sprite_polygon* sp = &s->polygons[i];
           for (int k = 0; k < sp->num_vertices; k++)
               pl.add_vertex(p + x * ((float)(sp->vertices[k] >> 4) - 7) + y * ((float)(sp->vertices[k] & 0xf) - 7), (sp->draw_edges >> k) & 1);

           // transform wall to view space
           for (int k = 0; k < pl.num_vertices; k++)
               transform_world_space_to_view_space(&pl.vertices[k]);

           // clip wall polygon against frustum
           polygon* p_polygon = &pl;
        #ifdef CLIP_TO_FRUSTUM
           polygon* p_clipped_polygon = &clipped_polygon;
           polygon* temp;
           for (int k = 0; k < frustum_count; k++)
           {
               clip_polygon_against_plane(p_clipped_polygon, shared_frustum_planes[(frustum_offset + k) % MAX_SHARED_FRUSTUM_PLANES], p_polygon);
               // break from loop if we have a degenerate polygon
               temp = p_polygon; p_polygon = p_clipped_polygon; p_clipped_polygon = temp;
               if (p_polygon->num_vertices < 3)
                   break;
           }
        #endif

           // skip this polygon if too many vertices have been clipped away
           if (p_polygon->num_vertices < 3)
               continue;

        #ifdef DEBUG
           faces_drawn += 1;
        #endif

           LINE_COORDINATE_TYPE first_x, first_y, last_x, last_y;

           for (int k = 0; k < p_polygon->num_vertices; k++)
           {
        #ifdef TOP_VIEW
               LINE_COORDINATE_TYPE tx = ((TOP_VIEW_SCALE * p_polygon->vertices[k].x) + 42) << FIXED_POINT_SCALE;
               LINE_COORDINATE_TYPE ty = ((TOP_VIEW_SCALE * p_polygon->vertices[k].z) + 34) << FIXED_POINT_SCALE;
        #else
               LINE_COORDINATE_TYPE tx = (41.5 + 41.5 * p_polygon->vertices[k].x / -p_polygon->vertices[k].z) << FIXED_POINT_SCALE;
               LINE_COORDINATE_TYPE ty = (23.5 - 41.5 * p_polygon->vertices[k].y / -p_polygon->vertices[k].z) << FIXED_POINT_SCALE;
        #endif
               if (k == 0)
               {
                   first_x = tx;
                   first_y = ty;
               }
               else
               {
                   if ((p_polygon->draw_edges >> (k - 1)) & 1)
                       draw_line_fixed_point(last_x, last_y, tx, ty);
               }
               last_x = tx;
               last_y = ty;
           }
           if ((p_polygon->draw_edges >> (p_polygon->num_vertices - 1)) & 1)
               draw_line_fixed_point(last_x, last_y, first_x, first_y);
        }
    */
}

void render_segment(byte segment_index, byte frustum_count, byte frustum_offset, byte from_segment = 255)
{
    if (map_mode)
    {
        if ((((segments_seen[segment_index >> 3] >> (segment_index & 7)) & 1) == 0))
            return;
    }
    segments_touched[segment_index >> 3] |= 1 << (segment_index & 7);
    segments_seen[segment_index >> 3] |= 1 << (segment_index & 7);
#ifdef DEBUG
    segments_drawn++;
#endif
    // iterate through every pair of adjacent vertices:
    // - construct polygon
    // - if it's a portal to the segment we're coming from: skip it
    // - clip against frustum (skip if invisible)
    // - if it's a wall: render outline
    // - if it's a portal: recursively render adjacent segment with updated frustum
    byte num_points = pgm_read_byte(&segments[segment_index].vertex_and_portal_count) >> 4;
    byte *vertices = (byte*)pgm_read_ptr(&segments[segment_index].vertices);
    word *portals = (word*)pgm_read_ptr(&segments[segment_index].portals);
    byte num_portals = pgm_read_byte(&segments[segment_index].vertex_and_portal_count) & 0xf;
    byte next_portal_index = 0;
    word next_portal_point = pgm_read_word(&portals[next_portal_index]);
    word adjacent_segment;
    int x0, z0, x1, z1;
    int x1_z1_from_point = -1;
    polygon wall;
    for (int i = 0; i < num_points; i++)
    {
        adjacent_segment = 0xffff;
        // test if it's a portal
        if (next_portal_index < num_portals && i == next_portal_point >> 12)
        {
            adjacent_segment = next_portal_point & 0xfff;
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

        long floor_height = 0;
#ifdef VARIABLE_ROOM_HEIGHT
        floor_height = (float)pgm_read_byte(&segments[segment_index].floor_height) / 16.0;
#endif
        vec3d p0((long)x0 << 16, floor_height, (long)z0 << 16);
        vec3d p1((long)x1 << 16, floor_height, (long)z1 << 16);

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
            wall.set_vertex(1, p0, !map_mode);
            wall.set_vertex(2, p0, (!map_mode) && (adjacent_segment == 0xffff));
            // don't draw second vertical edge, it will be drawn by the adjacent wall
            wall.set_vertex(3, p1, false);
        }
        long ceiling_height = 65536;
#ifdef VARIABLE_ROOM_HEIGHT
        ceiling_height = (float)pgm_read_byte(&segments[segment_index].ceiling_height) / 16.0;
#endif
        wall.vertices[2].y = ceiling_height;
        wall.vertices[3].y = ceiling_height;

#if VARIABLE_ROOM_HEIGHT
        if (adjacent_segment != 0xffff)
        {
            if (floor_height !=
                    pgm_read_byte(&segments[adjacent_segment].floor_height))
                wall.draw_edges |= 0x1;
            if (ceiling_height !=
                    pgm_read_byte(&segments[adjacent_segment].ceiling_height))
                wall.draw_edges |= 0x4;
            if (adjacent_segment == from_segment)
                wall.draw_edges &= ~0xa;
        }
#endif

        // transform wall to view space
        // TODO: This is done twice as often as required because we don't
        // re-use transformed vertices from the last wall
        if (also_drew_previous_wall)
        {
            transform_world_space_to_view_space(&wall.vertices[0]);
            transform_world_space_to_view_space(&wall.vertices[3]);
        }
        else
        {
            for (int k = 0; k < wall.num_vertices; k++)
                transform_world_space_to_view_space(&wall.vertices[k]);
        }

        polygon clipped_wall;
        // clip wall polygon against frustum
        polygon* p_wall = &wall;
#ifdef CLIP_TO_FRUSTUM
        if (!map_mode)
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

        // test if it's a portal
        if (adjacent_segment != 0xffff)
        {
            // enqueue render job:
            // - render segment [adjacent_segment] with frustum defined by required number of normal vectors
            //   (or give up is not enough space is available - p will be NULL in that case!)
            if ((((segments_touched[adjacent_segment >> 3] >> (adjacent_segment & 7)) & 1) == 0))
            {
#ifdef DEBUG
                int temp = current_render_jobs->frustum_plane_count + next_render_jobs->frustum_plane_count + p_wall->num_vertices;
                if (max_frustum_planes < temp)
                    max_frustum_planes = temp;
#endif
                int p = next_render_jobs->add_job(adjacent_segment, segment_index, p_wall->num_vertices);
                if (p > -1)
                {
                    // fill in frustum planes...
                    for (int k = 0; k < p_wall->num_vertices; k++)
                    {
                        // TODO: adjust frustum planes if we have differing floor / ceiling heights
                        byte next_k = (k + 1) % p_wall->num_vertices;
                        shared_frustum_planes[p] = p_wall->vertices[next_k].cross(p_wall->vertices[k]);
                        p = (p + 1) % MAX_SHARED_FRUSTUM_PLANES;
                    }
                }
            }
        }

#ifdef DEBUG
        faces_drawn += 1;
#endif

        int first_x, first_y, last_x, last_y;

        for (int k = 0; k < p_wall->num_vertices; k++)
        {
            LINE_COORDINATE_TYPE tx, ty;
            if (map_mode)
            {
                tx = ((((map_scale >> 8) * ((p_wall->vertices[k].x + map_dx) >> 8)) >> 16) + 42) << FIXED_POINT_SCALE;
                ty = ((((map_scale >> 8) * ((p_wall->vertices[k].z + map_dy) >> 8)) >> 16) + 34) << FIXED_POINT_SCALE;
            }
            else
            {
                // LINE_COORDINATE_TYPE tx = (41.5 + 41.5 * p_wall->vertices[k].x / -p_wall->vertices[k].z) << FIXED_POINT_SCALE;
                // LINE_COORDINATE_TYPE ty = (23.5 - 41.5 * p_wall->vertices[k].y / -p_wall->vertices[k].z) << FIXED_POINT_SCALE;
                // instead of dividing by z twice, calculate 1/z and multiply with that (it's actually faster)
                long z1 = (((-1L) << 24) / p_wall->vertices[k].z);
                tx = ((83 << FIXED_POINT_SCALE) + ((((p_wall->vertices[k].x * (83 << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
                ty = ((47 << FIXED_POINT_SCALE) - ((((p_wall->vertices[k].y * (83 << FIXED_POINT_SCALE)) >> 8) * z1) >> 16)) >> 1;
            }
            
            if (k == 0)
            {
                first_x = tx;
                first_y = ty;
            }
            else
            {
                if ((p_wall->draw_edges >> (k - 1)) & 1)
                    draw_line_fixed_point(last_x, last_y, tx, ty);
            }
            last_x = tx;
            last_y = ty;
        }
        if ((p_wall->draw_edges >> (p_wall->num_vertices - 1)) & 1)
            draw_line_fixed_point(last_x, last_y, first_x, first_y);
    }

    /*
        // render sprites
        if (segment_index == 0)
        {
        render_sprite(0, vec3d(1.0, 0.5, 8.0), frustum_count, frustum_offset);
        }
    */

    // render shots in this segment
    /*
        for (byte i = 0; i < num_shots; i++)
        {
        if (shots[i].current_segment != segment_index)
            continue;
        vec3d p((float)shots[i].x / 256.0, (float)shots[i].y / 256.0, (float)shots[i].z / 256.0);
        vec3d dir(
            ((float)(shots[i].dx) - 127.0) / 127.0,
            ((float)(shots[i].dy) - 127.0) / 127.0,
            ((float)(shots[i].dz) - 127.0) / 127.0);
        vec3d p2(p - dir * 0.1);
        polygon flare_polygon;
        flare_polygon.add_vertex(p, true);
        flare_polygon.add_vertex(p2, false);

        // transform flare to view space
        transform_world_space_to_view_space(&flare_polygon.vertices[0]);
        transform_world_space_to_view_space(&flare_polygon.vertices[1]);

        polygon clipped_flare_polygon;
        polygon* p_flare_polygon = &flare_polygon;
        #ifdef CLIP_TO_FRUSTUM
        polygon* p_clipped_flare_polygon = &clipped_flare_polygon;
        polygon* temp;
        for (int k = 0; k < frustum_count; k++)
        {
            clip_line_against_plane(p_clipped_flare_polygon, shared_frustum_planes[(frustum_offset + k) % MAX_SHARED_FRUSTUM_PLANES], p_flare_polygon);
            temp = p_flare_polygon; p_flare_polygon = p_clipped_flare_polygon; p_clipped_flare_polygon = temp;
            // break from loop if we have a degenerate polygon
            if (p_flare_polygon->num_vertices < 2)
                break;
        }

        #endif
        if (p_flare_polygon->num_vertices == 2)
        {
        #ifdef TOP_VIEW
            LINE_COORDINATE_TYPE tx = ((TOP_VIEW_SCALE * p_flare_polygon->vertices[0].x / 65536) + 42) << FIXED_POINT_SCALE;
            LINE_COORDINATE_TYPE ty = ((TOP_VIEW_SCALE * p_flare_polygon->vertices[0].z / 65536) + 34) << FIXED_POINT_SCALE;
            LINE_COORDINATE_TYPE tx2 = ((TOP_VIEW_SCALE * p_flare_polygon->vertices[1].x / 65536) + 42) << FIXED_POINT_SCALE;
            LINE_COORDINATE_TYPE ty2 = ((TOP_VIEW_SCALE * p_flare_polygon->vertices[1].z / 65536) + 34) << FIXED_POINT_SCALE;
        #else
            LINE_COORDINATE_TYPE tx = ((41.5 + 41.5 * p_flare_polygon->vertices[0].x / -p_flare_polygon->vertices[0].z)) << FIXED_POINT_SCALE;
            LINE_COORDINATE_TYPE ty = ((23.5 - 41.5 * p_flare_polygon->vertices[0].y / -p_flare_polygon->vertices[0].z)) << FIXED_POINT_SCALE;
            LINE_COORDINATE_TYPE tx2 = ((41.5 + 41.5 * p_flare_polygon->vertices[1].x / -p_flare_polygon->vertices[1].z)) << FIXED_POINT_SCALE;
            LINE_COORDINATE_TYPE ty2 = ((23.5 - 41.5 * p_flare_polygon->vertices[1].y / -p_flare_polygon->vertices[1].z)) << FIXED_POINT_SCALE;
        #endif
            draw_line_fixed_point(tx, ty, tx2, ty2);
        }
        }
    */
}
void print_vec3d(const vec3d& v)
{
    gb.display.print((float)(v.x / 65536.0));
    gb.display.print(F(" "));
    gb.display.print((float)(v.y / 65536.0));
    gb.display.print(F(" "));
    gb.display.print((float)(v.z / 65536.0));
}

void update_scene()
{
    LOG("-----------------------\nupdating scene...\n");
    /*
        for (int i = 0; i < 12; i++)
        {
        //camera.wobble = 0;
        float dx = cos(i * 2.0 * PI / 12.0 + (camera.wobble / 256.0));
        float dy = sin(i * 2.0 * PI / 12.0 + (camera.wobble / 256.0));
        LINE_COORDINATE_TYPE x0 = (41.5 + 5.0 * dx) * FIXED_POINT_SCALE;
        LINE_COORDINATE_TYPE y0 = (23.5 + 5.0 * dy) * FIXED_POINT_SCALE;
        LINE_COORDINATE_TYPE x1 = (41.5 + 22.5 * dx) * FIXED_POINT_SCALE;
        LINE_COORDINATE_TYPE y1 = (23.5 + 22.5 * dy) * FIXED_POINT_SCALE;
        draw_line_fixed_point(x0, y0, x1, y1);
        }
        return;
        for (int i = 0; i < 3; i++)
        {
        //camera.wobble = 0;
        float dx = cos(camera.wobble * PI / 128.0);
        LINE_COORDINATE_TYPE x0 = 10.0 * FIXED_POINT_SCALE;
        LINE_COORDINATE_TYPE y0 = (i * 10.0 + 10.0 + dx * 1.0) * FIXED_POINT_SCALE;
        LINE_COORDINATE_TYPE x1 = 70.0 * FIXED_POINT_SCALE;
        LINE_COORDINATE_TYPE y1 = (i * 12.0 + 15.0 + dx * 3.0) * FIXED_POINT_SCALE;
        draw_line_fixed_point(x0, y1, x1, y0);
        }
        return;
    */
#ifdef DEBUG_SERIAL
    Serial.println("updating scene");
#endif

#ifdef SHOW_FRAME_TIME
    unsigned long start_micros = micros();
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
    shared_frustum_planes[0] = vec3d((long)(65536.0 * 1.15), 0, (long)(65536.0 * -1.17));
    shared_frustum_planes[1] = vec3d((long)(65536.0 * -1.15), 0, (long)(65536.0 * -1.17));
    shared_frustum_planes[2] = vec3d(0, (long)(65536.0 * 2.02), (long)(65536.0 * -1.17));
    shared_frustum_planes[3] = vec3d(0, (long)(65536.0 * -2.02), (long)(65536.0 * -1.17));
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
    if (map_mode)
    {
        LINE_COORDINATE_TYPE tx, ty;
        tx = ((((map_scale >> 8) * ((map_dx) >> 8)) >> 16) + 42) << FIXED_POINT_SCALE;
        ty = ((((map_scale >> 8) * ((map_dy) >> 8)) >> 16) + 34) << FIXED_POINT_SCALE;
        draw_line_fixed_point(tx - 16, ty, tx + 16, ty);
        draw_line_fixed_point(tx, ty, tx, ty - 32);
        /*
        gb.display.drawLine(42, 34, 72, 4);
        gb.display.drawLine(42, 34, 12, 4);
        */
    }
    /*
    else
    {
        // draw crosshair
        gb.display.drawPixel(41, 24);
        gb.display.drawPixel(43, 24);
        gb.display.drawPixel(41, 26);
        gb.display.drawPixel(43, 26);

        gb.display.drawPixel(38, 24);
        gb.display.drawPixel(38, 26);
        gb.display.drawPixel(37, 25);
        gb.display.drawPixel(39, 25);

        gb.display.drawPixel(46, 24);
        gb.display.drawPixel(46, 26);
        gb.display.drawPixel(45, 25);
        gb.display.drawPixel(47, 25);

        gb.display.drawPixel(37, 29);
        gb.display.drawPixel(38, 29);
        gb.display.drawPixel(39, 28);

        gb.display.drawPixel(45, 28);
        gb.display.drawPixel(46, 29);
        gb.display.drawPixel(47, 29);
    }
    */
    //gb.display.print(F("LUNAR OUTPOST        "));
#ifdef DEBUG

#endif
#ifdef DEBUG_REALLY_HARD_FIXED_POINT_LINE_DRAWING_PROBLEM
    draw_line_fixed_point(528, 640, 528, 416);
    draw_line_fixed_point(528, 416, 640, 288);
#endif
    unsigned long current_micros = micros();
    micros_per_frame = current_micros - last_micros;
    last_micros = current_micros;
    if (map_mode)
    {
        gb.display.print("AUTOMAP");
        gb.display.println();
        gb.display.println();
        gb.display.println();
        gb.display.println();
        gb.display.println();
        gb.display.println();
        gb.display.println();
        gb.display.print("       hold C to quit");
    }
#ifdef SHOW_FRAME_TIME
    gb.display.print((micros() - start_micros) / 1000.0);
    gb.display.print(" ms / ");
    gb.display.print(1e6 / micros_per_frame);
    gb.display.println(" fps");
#endif
#ifdef MONITOR_RAM
    long ram_usage = max_ram_usage();
    gb.display.println();
    gb.display.println();
    gb.display.println();
    gb.display.println();
    gb.display.println();
    gb.display.println();
    gb.display.print("RAM: ");
    gb.display.print(ram_usage);
    gb.display.print(" (");
    gb.display.print(ram_usage * 100 / 2048);
    gb.display.print("%)");
#endif
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


