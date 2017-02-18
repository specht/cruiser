#!/usr/bin/env ruby

require 'json'
require 'yaml'

$ax = 0
$ay = 0
$segments = []
$doors = []

class Array
    def each_with_next_and_indices(&block)
        (0...self.size).each do |a|
            b = (a + 1) % self.size
            yield(self[a], self[b], a, b)
        end
    end

    def each_with_prev_and_next_and_indices(&block)
        (0...self.size).each do |b|
            a = (b + self.size - 1) % self.size
            c = (b + 1) % self.size
            yield(self[a], self[b], self[c], a, b, c)
        end
    end
end

def v(dx, dy)
    $segments.last[:p][0] += dx
    $segments.last[:p][1] += dy
    $segments.last[:vertices] << [$segments.last[:p][0], $segments.last[:p][1]]
end

def height(floor, ceiling)
    $segments.last[:floor_height] = floor
    $segments.last[:ceiling_height] = ceiling
end

def door(dx, dy)
    $segments.last[:doors][$segments.last[:vertices].size - 1] = $doors.size
    $doors << {:segment_index => $segments.size - 1, :vertex_index => $segments.last[:vertices].size}
    $segments.last[:p][0] += dx
    $segments.last[:p][1] += dy
    $segments.last[:vertices] << [$segments.last[:p][0], $segments.last[:p][1]]
end

def segment(x0, y0, &block)
    $segments << {
        :offset => [0, 0],
        :p => [x0, y0],
        :vertices => [[x0, y0]],
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
    $segments.each.with_index do |segment, segment_index|
        minx = 255
        miny = 255
        segment[:vertices].each.with_index do |p, vertex_index|
            x0 = segment[:vertices][vertex_index][0]
            y0 = segment[:vertices][vertex_index][1]
            x1 = segment[:vertices][(vertex_index + 1) % segment[:vertices].size][0]
            y1 = segment[:vertices][(vertex_index + 1) % segment[:vertices].size][1]
            minx = x0 if x0 < minx
            miny = y0 if y0 < miny
        end
        segment[:offset] = [minx, miny]
        segment[:vertices].each.with_index do |p, vertex_index|
            segment[:vertices][vertex_index][0] -= segment[:offset][0]
            segment[:vertices][vertex_index][1] -= segment[:offset][1]
        end
        # delete last vertex if segment has been closed in the definition
        if segment[:vertices].last.to_json == segment[:vertices].first.to_json
#             raise "Segment is not closed (line #{segment[:line]})"
            segment[:vertices].delete_at(segment[:vertices].size - 1)
        end
        segment.delete(:p)
    end
    $segments.each.with_index do |segment, segment_index|
        segment[:vertices].each_with_next_and_indices do |p0, p1, vertex_index|
            x0 = p0[0] + segment[:offset][0]
            y0 = p0[1] + segment[:offset][1]
            x1 = p1[0] + segment[:offset][0]
            y1 = p1[1] + segment[:offset][1]
            tag = [[x0, y0, x1, y1].join(','), [x1, y1, x0, y0].join(',')].sort
            tags[tag] ||= []
            tags[tag] << {:segment_index => segment_index, :edge_index => vertex_index}
        end
    end
    tags.select! do |tag, entries|
        entries.size > 1
    end
    STDERR.puts "Found #{tags.size} portals."
    tags.each_pair do |tag, entries|
        if entries.size != 2
            raise 'The number of shared edges is TOO DAMN HIGH!'
        end
        # create two portals for every wall shared between two segments
        $segments[entries[0][:segment_index]][:portals][entries[0][:edge_index]] = entries[1][:segment_index]
        $segments[entries[1][:segment_index]][:portals][entries[1][:edge_index]] = entries[0][:segment_index]
    end
    $segments.each.with_index do |segment, segment_index|
        segment[:doors].each_pair do |edge_index, door_index|
            adjacent_segment_index = segment[:portals][edge_index]
            candidates = $segments[adjacent_segment_index][:portals].keys.select do |adjacent_segment_edge_index|
                $segments[adjacent_segment_index][:portals][adjacent_segment_edge_index] == segment_index
            end
            if candidates.size != 1
                raise 'Expected exactly one candidate.'
            end
            $segments[adjacent_segment_index][:doors][candidates.first] = door_index
        end
    end
    
#     STDERR.puts $segments.to_yaml
end

def dump(io = STDOUT)
    vertex_bytes_for_segment = []
    portal_words_for_segment = []
    door_words_for_segment = []
    $segments.each.with_index do |segment, _|
        sx, sy = *segment[:offset]
        vertices = []
        vertex_bytes = []
        segment[:vertices].each do |p|
            ax, ay = *p
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
#         segment[:vertices].each.with_index do |p, _|
#             p0 = vertices[_]
#             p1 = vertices[(_ + 1) % segment[:vertices].size]
#             dx = p0[1] - p1[1]
#             dy = p1[0] - p0[0]
#             n = [dx, dy]
#             len = 1.0 / ((n[0] * n[0] + n[1] * n[1]) ** 0.5)
#             tag = [dx.abs, dy.abs].sort
#         end
        portal_words = []
        segment[:portals].keys.sort.each do |vertex_index|
            portal_words << sprintf('0x%x', ((vertex_index << 12) | (segment[:portals][vertex_index] & 0xfff)))
        end
        door_words = []
        segment[:doors].keys.sort.each do |vertex_index|
            door_words << sprintf('0x%x', ((vertex_index << 12) | (segment[:doors][vertex_index] & 0xfff)))
        end
        vertex_bytes_for_segment << vertex_bytes.join(',')
        portal_words_for_segment << portal_words.join(',')
        door_words_for_segment << door_words.join(',')
    end
    $segments.each.with_index do |segment, _|
        io.puts "const static byte v_#{_}[] PROGMEM = {#{vertex_bytes_for_segment[_]}};"
        io.puts "const static word p_#{_}[] PROGMEM = {#{portal_words_for_segment[_]}};"
        unless segment[:doors].empty?
            io.puts "const static word d_#{_}[] PROGMEM = {#{door_words_for_segment[_]}};"
        end
    end
    io.puts
    io.puts "const static segment segments[] PROGMEM = {"
    $segments.each.with_index do |segment, _|
        io.print "  {#{segment[:floor_height]},#{segment[:ceiling_height]},#{segment[:offset].join(',')},"
        io.print "#{sprintf('0x%x', (segment[:vertices].size << 4) | segment[:portals].size)},"
        io.print "#{sprintf('0x%x', segment[:doors].size)},"
        io.print "v_#{_},"
        io.print "p_#{_},"
        if segment[:doors].empty?
            io.print "0"
        else
            io.print "d_#{_}"
        end
        io.puts "}#{_ < $segments.size - 1 ? ',' : ''}"
    end
    io.puts "};"
    io.puts
    io.puts "const static int SEGMENTS_TOUCHED_SIZE = #{(($segments.size - 1) >> 3) + 1};"
    io.puts "byte segments_touched[SEGMENTS_TOUCHED_SIZE];"
    io.puts
    io.puts "const static int DOOR_COUNT = #{$doors.size};"
    io.puts "long doors[DOOR_COUNT];"
end

def dump_svg(io)
#     io.puts "<rect width='256' height='256' fill='#001b4a' />"
    # render polygons
    width = 0 
    height = 0
    $segments.each.with_index do |segment, segment_index|
        sx = segment[:offset][0]
        sy = segment[:offset][1]
        segment[:vertices].each.with_index do |p, vertex_index|
            ax, ay = *p
            ax += sx
            ay += sy
            width = ax if ax > width
            height = ay if ay > height
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
        vertex_bytes = []
        io.print "<polygon points='"
        segment[:vertices].each.with_index do |p, vertex_index|
            ax, ay = *p
            if vertex_index == 0
                io.print "#{(sx + ax) * 4 + 0.5},#{(sy + ay) * 4 + 0.5},"
            end
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
        vertex_bytes = []
        count = 0
        segment[:vertices].each_with_prev_and_next_and_indices do |pr, p0, p1, vertex_index|
#             nx = p1[1] - pr[1]
#             ny = pr[0] - p1[0]
#             tx = p0[0] - pr[0]
#             ty = p0[1] - pr[1]
#             if nx * tx + ny * ty < 0.00001
                ax, ay = *p0
                cx += sx + ax
                cy += sy + ay
                count += 1
#             end
        end
        cx /= count
        cy /= count
        cx = cx * 4 + 0.5
        cy = cy * 4 + 0.5
        io.puts "<text x='#{cx}' y='#{cy + 0.3}' text-anchor='middle' font-family='Arial' font-weight='bold' font-size='1.0' fill='#fff'>#{segment_index}</text>"
    end
    # render lines
    $segments.each.with_index do |segment, segment_index|
        sx = segment[:offset][0]
        sy = segment[:offset][1]
        vertex_bytes = []
        segment[:vertices].each_with_next_and_indices do |p0, p1, vertex_index|
            x0 = p0[0] + sx
            y0 = p0[1] + sy
            x1 = p1[0] + sx
            y1 = p1[1] + sy
            
            io.puts "<rect x='#{x0 * 4 + 0.5 - 0.2}' y='#{y0 * 4 + 0.5 - 0.2}' width='0.4' height='0.4'  stroke='#fff' stroke-width='0.2' />"
            
            # render edge index
            lx = (x0 + x1) * 0.5
            ly = (y0 + y1) * 0.5
            nx = y0 - y1
            ny = x1 - x0
            nl = (nx * nx + ny * ny) ** 0.5
            nx /= nl
            ny /= nl
            lx -= nx * 0.2
            ly -= ny * 0.2
            io.puts "<text x='#{lx * 4 + 0.5}' y='#{ly * 4 + 0.5 + 0.3}' text-anchor='middle' font-family='Arial' font-size='1' fill='#aaa'>#{(vertex_index + segment[:vertices].size) % segment[:vertices].size}</text>"

            color = '#fff'
            stroke_width = 0.2
            if segment[:portals].include?(vertex_index)
                color = '#2f6f91'
            end
            if segment[:doors].include?(vertex_index)
                color = '#f00'
            end
            io.print "<line x1='#{x0 * 4 + 0.5}' y1='#{y0 * 4 + 0.5}' "
            io.puts "x2='#{x1 * 4 + 0.5}' y2='#{y1 * 4 + 0.5}' stroke='#{color}' stroke-width='#{stroke_width}' />"
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
    v 0, 4
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

find_portals()

File::open('map.h', 'w') do |f|
    dump(f)
end

File::open('level.svg', 'w') do |f|
    dump_svg(f)
end

system("inkscape -z -w 2048 -e level.png level.svg > /dev/null")
