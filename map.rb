#!/usr/bin/env ruby

require 'json'
require 'yaml'

$ax = 0
$ay = 0
$segments = []
$doors = []

def v(dx, dy)
    $segments.last[:vertices] << [dx, dy]
end

def height(floor, ceiling)
    $segments.last[:floor_height] = floor
    $segments.last[:ceiling_height] = ceiling
end

def door(dx, dy)
    $segments.last[:doors][$segments.last[:vertices].size] = $doors.size
    $doors << {:segment_index => $segments.size - 1, :vertex_index => $segments.last[:vertices].size}
    $segments.last[:vertices] << [dx, dy]
end

def segment(x0, y0, &block)
    $segments << {
        :offset => [0, 0],
        :p0 => [x0, y0],
        :vertices => [],
        :portals => {},
        :doors => {},
        :floor_height => 0,
        :ceiling_height => 16,
        :line => Kernel.caller_locations(1, 1).first.to_s.split(':')[1]
    }
    $ax = 0
    $ay = 0
    
    yield
end

def find_portals()
    tags = {}
    # auto-close paths if necessary
    $segments.each.with_index do |segment, segment_index|
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        segment[:vertices].each.with_index do |p, vertex_index|
            ax += p[0]
            ay += p[1]
        end
        # auto-close path if necessary
        unless ax == segment[:p0][0] && ay == segment[:p0][1]
            segment[:vertices] << [segment[:p0][0] - ax, segment[:p0][1] - ay]
        end
    end
    $segments.each.with_index do |segment, segment_index|
        minx = 255
        miny = 255
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        segment[:vertices].each.with_index do |p, vertex_index|
            minx = ax if ax < minx
            miny = ay if ay < miny
            oax = ax
            oay = ay
            ax += p[0]
            ay += p[1]
            minx = ax if ax < minx
            miny = ay if ay < miny
            tag = [[oax, oay, ax, ay].join(','), [ax, ay, oax, oay].join(',')].sort
            tags[tag] ||= []
            tags[tag] << {:segment_index => segment_index, :vertex_index => ((vertex_index + segment[:vertices].size - 1) % segment[:vertices].size)}
        end
        segment[:offset] = [minx, miny]
        segment[:p0][0] -= segment[:offset][0]
        segment[:p0][1] -= segment[:offset][1]
    end
    tags.select! do |tag, entries|
        entries.size > 1
    end
    STDERR.puts "Found #{tags.size} portals."
    tags.each_pair do |tag, entries|
        if entries.size != 2
            raise 'The number of shared edges is TOO DAMN HIGH!'
        end
        $segments[entries[0][:segment_index]][:portals][entries[0][:vertex_index]] = entries[1][:segment_index]
        $segments[entries[1][:segment_index]][:portals][entries[1][:vertex_index]] = entries[0][:segment_index]
    end
end

def dump(io = STDOUT)
    vertex_bytes_for_segment = []
    portal_words_for_segment = []
    normal_scale_for_dxdy = {}
    $segments.each.with_index do |segment, _|
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        vertices = []
        vertex_bytes = []
        segment[:vertices].each do |p|
            ax += p[0]
            ay += p[1]
            if ax < 0 || ax > 15 || ay < 0 || ay > 15
                raise 'Coordinates out of range.'
            end
            # TODO: 
            # - test if segment is convex
            # - test if vertices are defined counter-clockwise
            # - test if no more than 16 vertices
            # - test whether it's a closed path
            vertices << [ax, ay]
            vertex_bytes << sprintf('0x%x', (ax << 4 | ay))
        end
        segment[:vertices].each.with_index do |p, _|
            p0 = vertices[_]
            p1 = vertices[(_ + 1) % segment[:vertices].size]
            dx = p0[1] - p1[1]
            dy = p1[0] - p0[0]
            n = [dx, dy]
            len = 1.0 / ((n[0] * n[0] + n[1] * n[1]) ** 0.5)
            tag = [dx.abs, dy.abs].sort
            normal_scale_for_dxdy[(tag[0] << 4) | tag[1]] = len;
        end
        portal_words = []
        segment[:portals].keys.sort.each do |vertex_index|
            portal_words << sprintf('0x%x', ((vertex_index << 12) | (segment[:portals][vertex_index] & 0xfff)))
        end
        vertex_bytes_for_segment << vertex_bytes.join(',')
        portal_words_for_segment << portal_words.join(',')
    end
#     io.puts "const static word num_normal_scale_for_dxdy = #{normal_scale_for_dxdy.size};"
#     io.puts "const static normal_scale_for_dxdy normal_scales[] PROGMEM = {"
#     normal_scale_for_dxdy.keys.sort.each.with_index do |tag, _|
#         io.print "  {#{tag}, #{sprintf('%1.6g', normal_scale_for_dxdy[tag])}}"
#         if _ < normal_scale_for_dxdy.size - 1
#             io.print ','
#         end
#         io.puts
#     end
#     io.puts "};"
    $segments.each.with_index do |segment, _|
        io.puts "const static byte v_#{_}[] PROGMEM = {#{vertex_bytes_for_segment[_]}};"
        io.puts "const static word p_#{_}[] PROGMEM = {#{portal_words_for_segment[_]}};"
    end
    io.puts "const static segment segments[] PROGMEM = {"
    $segments.each.with_index do |segment, _|
        io.print "  {#{segment[:floor_height]},#{segment[:ceiling_height]},#{segment[:offset].join(',')},"
        io.print "#{sprintf('0x%x', (segment[:vertices].size << 4) | segment[:portals].size)},"
        io.print "v_#{_},"
        io.print "p_#{_}"
        io.puts "}#{_ < $segments.size - 1 ? ',' : ''}"
    #     const static segment segments[] = {
    #   {
    #     0, 16,
    #     0, 7,
    #     6, BP {0x03, 0x33, 0x31, 0x20, 0x10, 0x01},
    #     2, BP {1, 3, 3, 1},
    #   },

    end
    io.puts "};"
    io.puts "const static int SEGMENTS_TOUCHED_SIZE = #{(($segments.size - 1) >> 3) + 1};"
    io.puts "byte segments_touched[SEGMENTS_TOUCHED_SIZE];"
end

def dump_svg(io)
#     io.puts "<rect width='256' height='256' fill='#001b4a' />"
    # render polygons
    width = 0 
    height = 0
    $segments.each.with_index do |segment, segment_index|
        sx = segment[:offset][0]
        sy = segment[:offset][1]
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        vertex_bytes = []
        segment[:vertices].each.with_index do |p, vertex_index|
            if vertex_index == 0
                x = sx + ax
                y = sy + ay
                width = x if x > width
                height = y if y > height
            end
            ax += p[0]
            ay += p[1]
            x = sx + ax
            y = sy + ay
            width = x if x > width
            height = y if y > height
        end
    end
    width += 1
    height += 1
    io.puts '<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">'
    io.puts "<svg version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' xml:space='preserve' width='#{width * 4}' height='#{height * 4}' >"
    io.puts "<rect width='#{width * 4}' height='#{height * 4}' fill='#000' />"
    $segments.each.with_index do |segment, segment_index|
        sx = segment[:offset][0]
        sy = segment[:offset][1]
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        vertex_bytes = []
        io.print "<polygon points='"
        segment[:vertices].each.with_index do |p, vertex_index|
            if vertex_index == 0
                io.print "#{(sx + ax) * 4 + 0.5},#{(sy + ay) * 4 + 0.5},"
#                 cx += (sx + ax) * 4 + 0.5
#                 cy += (sy + ay) * 4 + 0.5
            end
            ax += p[0]
            ay += p[1]
            io.print "#{(sx + ax) * 4 + 0.5},#{(sy + ay) * 4 + 0.5}"
            if vertex_index < segment[:vertices].size - 1
                io.print ","
            end
        end
        io.puts "' style='fill: #03182b;' />"
    end
    # render grid
    (0..256).each do |i|
        if i <= height
            io.print "<line x1='0' y1='#{i * 4 + 0.5}' x2='#{width * 4}' y2='#{i * 4 + 0.5}' stroke='#{i % 10 == 0 ? '#777' : '#444'}' stroke-width='0.1' />"
        end
        if i <= width
            io.print "<line x1='#{i * 4 + 0.5}' y1='0' x2='#{i * 4 + 0.5}' y2='#{height * 4}' stroke='#{i % 10 == 0 ? '#777' : '#444'}' stroke-width='0.1' />"
        end
    end
    # render labels
    $segments.each.with_index do |segment, segment_index|
        cx = 0.0
        cy = 0.0
        sx = segment[:offset][0]
        sy = segment[:offset][1]
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        vertex_bytes = []
        segment[:vertices].each.with_index do |p, vertex_index|
            if vertex_index == 0
                io.print "#{(sx + ax) * 4 + 0.5},#{(sy + ay) * 4 + 0.5},"
            end
            ax += p[0]
            ay += p[1]
            cx += (sx + ax) * 4 + 0.5
            cy += (sy + ay) * 4 + 0.5
        end
        cx /= segment[:vertices].size
        cy /= segment[:vertices].size
        io.puts "<text x='#{cx}' y='#{cy + 0.6}' text-anchor='middle' font-family='Arial' font-size='2' fill='#fff'>#{segment_index}</text>"
    end
    # render lines
    $segments.each.with_index do |segment, segment_index|
        sx = segment[:offset][0]
        sy = segment[:offset][1]
        ax = segment[:p0][0]
        ay = segment[:p0][1]
        vertex_bytes = []
        segment[:vertices].each.with_index do |p, vertex_index|
            io.puts "<rect x='#{(sx + ax) * 4 + 0.5 - 0.2}' y='#{(sy + ay) * 4 + 0.5 - 0.2}' width='0.4' height='0.4'  stroke='#fff' stroke-width='0.2' />"
            lx = sx + ax + p[0] * 0.5
            ly = sy + ay + p[1] * 0.5
            nx = -p[1]
            ny = p[0]
            nl = (nx * nx + ny * ny) ** 0.5;
            nx /= nl
            ny /= nl
            lx -= nx * 0.2
            ly -= ny * 0.2
            io.puts "<text x='#{lx * 4 + 0.5}' y='#{ly * 4 + 0.5 + 0.3}' text-anchor='middle' font-family='Arial' font-size='1' fill='#fff'>#{(vertex_index + segment[:vertices].size - 1) % segment[:vertices].size}</text>"
            io.print "<line x1='#{(sx + ax) * 4 + 0.5}' y1='#{(sy + ay) * 4 + 0.5}' "
            ax += p[0]
            ay += p[1]
            color = '#fff'
            stroke_width = 0.2
            if segment[:portals].include?((vertex_index + segment[:vertices].size - 1) % segment[:vertices].size)
                color = '#2f6f91'
            end
            if segment[:doors].include?(vertex_index)
                color = '#f00'
            end
            io.puts "x2='#{(sx + ax) * 4 + 0.5}' y2='#{(sy + ay) * 4 + 0.5}' stroke='#{color}' stroke-width='#{stroke_width}' />"
        end
    end
    io.puts '</svg>'
end

segment(0, 8) do
    v 0, 2
    v 4, 0
    door 0, -1
    v 0, -1
    v -1, -1
    v -1, 0
    v -1, 0
    v -1, 1
end

segment(1, 7) do
    height 4, 12
    v 1, 0
    v 0, -4
    v -1, 0
end

segment(0, 2) do
    v 1, 1
    v 1, 0
    v 8, -1
    v 0, -2
    v -9, 0
    v -1, 1
end

segment(10, 0) do
    height 0, 8
    v 0, 2
    v 1, 0
    v 1, -2
end

segment(12, 0) do
    height 2, 10
    v -1, 2
    v 2, 1
    v 1, -2
end

segment(14, 1) do
    height 4, 12
    v -1, 2
    v 1, 1
    v 2, -1
end

segment(14, 4) do
    height 6, 14
    v 1, 2
    v 2, -1
    v -1, -2
end

segment(15, 6) do
    height 8, 16
    v 0, 1
    v 2, 0
    v 0, -2
end

segment(15, 7) do
    v -2, 1
    v -1, 1
    v 0, 1
    v 1, 1
    v 2, 0
    v 4, 0
    v 1, 0
    v 0, -1
    v 0, -2
    v -3, -1
end

segment(4, 9) do
    v 0, 1
    v 8, 0
    v 0, -1
end

segment(15, 11) do
    v 0, 4
    v 4, 0
    v 0, -4
end

segment(20, 11) do
    v 2, 0
    v 0, -1
    v -2, 0
end

segment(22, 11) do
    v 3, -1
    v -1, -1
    v -2, 1
end

segment(25, 10) do
    v 1, -1
    v -1, -1
    v -1, 1
end

segment(26, 9) do
    v 1, -3
    v -1, 0
    v -1, 2
end

segment(27, 6) do
    v 0, -1
    v -1, 0
    v 0, 1
end

segment(27, 5) do
    v 3, 0
    v 0, -3
    v -7, 0
    v 0, 3
    v 3, 0
end

segment(15, 15) do
    v -2, 2
    v 0, 1
    v 3, 0
    v 2, 0
    v 3, 0
    v 0, -1
    v -2, -2
end

# segment(13, 18) do
#     v 0, 2
#     v 3, 0
#     v 0, -2
# end
# 
# segment(16, 18) do
#     v 0, 2
#     v 2, 0
#     v 0, -2
# end
# 
# segment(18, 18) do
#     v 0, 2
#     v 3, 0
#     v 0, -2
# end
# 
# segment(13, 20) do
#     v 0, 1
#     v 2, 2
#     v 4, 0
#     v 2, -2
#     v 0, -1
#     v -3, 0
#     v -2, 0
# end


find_portals()
dump()

File::open('level.svg', 'w') do |f|
    dump_svg(f)
end
system("inkscape -z -w 2048 -e level.png level.svg > /dev/null")
