#!/usr/bin/env ruby

require 'json'
require 'set'
require 'yaml'

$ax = 0
$ay = 0
$segments = []
$doors = []
$wall_normals = []

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
    raise 'Too many vertices' if $segments.last[:vertices].size >= 8
    $segments.last[:p][0] += dx
    $segments.last[:p][1] += dy
    $segments.last[:vertices] << [$segments.last[:p][0], $segments.last[:p][1]]
end

def height(floor, ceiling)
    $segments.last[:floor_height] = floor
    $segments.last[:ceiling_height] = ceiling
end

def door(dx, dy)
    raise "Too many vertices" if $segments.last[:vertices].size >= 8
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
        :wall_normals => [],
        :portals => {},
        :doors => {},
        :floor_height => 16,
        :ceiling_height => 20,
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
            n = [x0 - x1, y1 - y0]
            ln = (n[0] * n[0] + n[1] * n[1]) ** 0.5
            n.map! { |f| f.abs }
            n.sort!
            n.map! { |f| f.to_f / ln }
            n.map! { |f| (f * 255.0).to_i }
            n = "#{n[0]} #{n[1]}"
            unless $wall_normals.include?(n)
                if $wall_normals.size >= 16
                    raise 'No more than 16 unique wall normals allowed!'
                end
                $wall_normals << n
            end
            segment[:wall_normals] << $wall_normals.index(n)
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

def array_index_of_array(haystack, needle)
#     0 1 2 3 4 5 6 7  haystack
#     0 1 2            needle
#       0 1 2          needle
#               0 1 2  needle
    return nil if haystack.size < needle.size
    (0..(haystack.size - needle.size)).each do |i|
        match = true
        (0...needle.size).each do |k|
            if needle[k] != haystack[i + k]
                match = false
                break
            end
        end
        return i if match
    end
    return nil
end

def dump(io = STDOUT)
    all_vertex_bytes = []
    all_wall_normal_bytes = []
    all_portal_bytes = []
    all_door_bytes = []
    vertex_index_for_segment = {}
    wall_normal_index_for_segment = {}
    portal_index_for_segment = {}
    door_index_for_segment = {}
    total_vertex_count = 0
    total_portal_count = 0
    $segments.each.with_index do |segment, _|
        puts "Segment ##{_}"
        sx, sy = *segment[:offset]
        vertices = []

        vertex_bytes = []
        wall_normal_bytes = []
        total_vertex_count += segment[:vertices].size
        total_portal_count += segment[:portals].size
        segment[:vertices].each_with_index do |p, vertex_index|
            ax, ay = *p
            if ax < 0 || ax > 15 || ay < 0 || ay > 15
                raise 'Coordinates out of range.'
            end
            # TODO: 
            # - test if segment is convex
            # - test if vertices are defined counter-clockwise
            # - test if no more than 8 vertices
            # - test whether it's a closed path
            vertices << [ax, ay]
            vertex_bytes << sprintf('0x%x', (ax << 4 | ay))
            wall_normal_bytes << segment[:wall_normals][vertex_index]
        end
        # combine two wall normal bytes into one
        temp = wall_normal_bytes.dup
        wall_normal_bytes = []
        combined = 0
        (0...temp.size).each do |i|
            combined <<= 4
            combined |= temp[i]
            if i % 2 == 0
                wall_normal_bytes << combined
                combined = 0
            end
        end
        wall_normal_bytes << combined if temp.size % 2 == 1

        portal_bytes = []
        segment[:portals].keys.sort.each do |vertex_index|
            diff = segment[:portals][vertex_index] - _
            if diff >= -8 && diff <= 8
                byte = (vertex_index << 5) | ((diff < 0) ? 8 : 0) | ((diff.abs - 1) & 0x7)    
                portal_bytes << sprintf('0x%x', byte) 
            else
                byte = (vertex_index << 5) | 0x10
                portal_bytes << sprintf('0x%x', byte) 
                byte = segment[:portals][vertex_index]
                portal_bytes << sprintf('0x%x', byte) 
            end
        end

        door_bytes = []
        segment[:doors].keys.sort.each do |vertex_index|
            door_bytes << sprintf('0x%x', ((vertex_index << 4) | (segment[:doors][vertex_index] & 0xf)))
        end
        # here's a trick: instead of just appending the numbers,
        # look whether this sequence of bytes is already present
        # anywhere in the previous stream of numbers
        # this gives us free geometry compression when we have
        # multiple segments with the same geometry
        puts "Vertex bytes: #{vertex_bytes.join(', ')}"
        if array_index_of_array(all_vertex_bytes, vertex_bytes)
            puts "Saved #{vertex_bytes.size} vertex bytes, yeah!"
        else
            all_vertex_bytes += vertex_bytes
        end
        vertex_index_for_segment[_] = array_index_of_array(all_vertex_bytes, vertex_bytes)
        
        puts "Normal bytes: #{wall_normal_bytes.join(', ')}"
        if array_index_of_array(all_wall_normal_bytes, wall_normal_bytes)
            puts "Saved #{wall_normal_bytes.size} wall normal bytes, yeah!"
        else
            all_wall_normal_bytes += wall_normal_bytes
        end
        wall_normal_index_for_segment[_] = array_index_of_array(all_wall_normal_bytes, wall_normal_bytes)
        
        puts "Portal bytes: #{portal_bytes.join(', ')}"
        if array_index_of_array(all_portal_bytes, portal_bytes)
            puts "Saved #{portal_bytes.size} portal bytes (8 bit), yeah!"
        else
            all_portal_bytes += portal_bytes
        end
        portal_index_for_segment[_] = array_index_of_array(all_portal_bytes, portal_bytes)

        puts "Door bytes: #{door_bytes.join(', ')}"
        unless door_bytes.empty?
            if array_index_of_array(all_door_bytes, door_bytes)
                puts "Saved some door bytes, yeah!"
            else
                all_door_bytes += door_bytes
            end
            door_index_for_segment[_] = array_index_of_array(all_door_bytes, door_bytes)
        end
        
    end
    io.print "const static uint8_t wall_normal_templates[] PROGMEM = {"
    io.print $wall_normals.map { |x| x.split(' ').map { |x| sprintf('0x%02x', x.to_i) }.join(', ') }.join(', ')
    io.puts "};"
    io.puts
    io.puts "const static uint8_t vertices[] PROGMEM = {#{all_vertex_bytes.join(', ')}};"
    io.puts
    io.puts "const static uint8_t wall_normals[] PROGMEM = {#{all_wall_normal_bytes.join(', ')}};"
    io.puts
    io.puts "const static uint8_t portals[] PROGMEM = {#{all_portal_bytes.join(', ')}};"
    io.puts
    io.puts "const static uint8_t doors[] PROGMEM = {#{all_door_bytes.join(', ')}};"
    io.puts
    io.puts "const static segment segments[] PROGMEM = {"
    io.puts '//   FH  CH    X    Y  VC PC DC     V     N    P   D'
    $segments.each.with_index do |segment, _|
        io.print "    {#{sprintf('%2d', segment[:floor_height])}, "
        io.print "#{sprintf('%2d', segment[:ceiling_height])}, "
        io.print "#{segment[:offset].map { |x| sprintf('%3d', x)}.join(', ')}, "
        io.print "#{sprintf('%2d', segment[:vertices].size)}, "
        io.print "#{segment[:portals].size}, "
        io.print "#{segment[:doors].size}, "
        io.print "#{sprintf('%4d', vertex_index_for_segment[_])}, "
        io.print "#{sprintf('%4d', wall_normal_index_for_segment[_])}, "
        io.print "#{sprintf('%3d', portal_index_for_segment[_])}, "
        io.print "#{sprintf('%2d', door_index_for_segment[_] || 0)}"
        io.puts "}#{_ < $segments.size - 1 ? ',' : ''}"
    end
    io.puts "};"
    io.puts
    io.puts "#define SEGMENTS_TOUCHED_SIZE #{(($segments.size - 1) >> 3) + 1}"
    io.puts "uint8_t segments_touched[SEGMENTS_TOUCHED_SIZE];"
    io.puts "#ifdef ENABLE_MAP"
    io.puts "    uint8_t segments_seen[SEGMENTS_TOUCHED_SIZE];"
    io.puts "#endif"
    io.puts
    io.puts "#define DOOR_COUNT #{$doors.size}"
    io.puts "int32_t door_state[DOOR_COUNT];"
    puts "Total vertex count: #{total_vertex_count}"
    puts "Total portal count: #{total_portal_count}"
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
        label = "#{segment_index}"
        if segment[:floor_height] != 0 || segment[:ceiling_height] != 16
            label += " (#{segment[:floor_height]} / #{segment[:ceiling_height]})"
        end
        io.puts "<text x='#{cx}' y='#{cy + 0.3}' text-anchor='middle' font-family='Arial' font-weight='bold' font-size='1.0' fill='#fff'>#{label}</text>"
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
#     height 15, 20
    v 0, 2
    v 4, 0
    v 0, -1
    v 0, -1
    v -1, -1
    v -1, 0
    
    door -1, 0
#     v -1, 0
    
#     v -1, 1
end

segment(1, 7) do
#     height 12, 20
    v 1, 0
    v 0, -4
    v -1, 0
    v 0, 4
end

segment(0, 2) do
#     height 0, 4
    v 1, 1
    v 1, 0
    v 8, -1
    v 0, -2
    v -9, 0
    v -1, 1
end

segment(10, 0) do
#     height 2, 4
    v 0, 2
    v 1, 0
    v 1, -2
end

segment(12, 0) do
#     height 2, 4
    v -1, 2
    v 2, 1
    v 1, -2
end

segment(14, 1) do
#     height 2, 4
    v -1, 2
    v 1, 1
    v 2, -1
end

segment(14, 4) do
#     height 2, 4
    v 1, 2
    v 2, -1
    v -1, -2
end

segment(15, 6) do
#     height 2, 4
    v 0, 1
    v 2, 0
    v 0, -2
end

segment(15, 7) do
#     height 12, 24
    v -2, 1
    v -1, 1
    v 0, 1
    v 1, 1
    v 2, 0
    v 2, -4
#     v 1, 0
#     v 0, -1
#     v 0, -2
#     v -3, -1
end

segment(17, 7) do
    v -2, 4
    v 4, 0
    v 1, 0
    v 0, -1
    v -1, -2
    v -2, -1
end

segment(4, 9) do
    v 0, 1
    v 1, 0
    v 6, 0
    v 1, 0
    v 0, -1
end

segment(15, 11) do
    v 0, 4
    v 4, 0
    v 0, -4
end

segment(20, 11) do
#     height 4, 6
    v 2, 0
    v 0, -1
    v -2, 0
end

segment(22, 11) do
#     height 4, 6
    v 3, -1
    v -1, -1
    v -2, 1
end

segment(25, 10) do
#     height 4, 6
    v 1, -1
    v -1, -1
    v -1, 1
end

segment(26, 9) do
#     height 4, 6
    v 1, -3
    v -1, 0
    v -1, 2
end

segment(27, 6) do
#     height 4, 6
    v 0, -1
    v -1, 0
    v 0, 1
end

segment(27, 5) do
#     height 4, 6
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
    door 0, -1
    v -2, -2
end

segment(5, 10) do
    v -1, 1
    v 2, 3
    v 4, 0
    v 2, -3
    v -1, -1
end

segment(4, 11) do
    v 0, 6
    v 2, 0
    v 0, -3
end

segment(10, 14) do
    v 0, 3
    v 1, 1
    v 1, 0
    v 0, -1
    v 0, -6
end

segment(12, 18) do
    v 1, 0
    v 0, -1
    v -1, 0
end

segment(21, 18) do
#     height 0, 16
    v 1, 0
    v 0, -1
    v -1, 0
end

segment(22, 14) do
    height 8, 20
    v 0, 3
    v 0, 1
    v 6, 0
    v 0, -4
end

find_portals()

File::open('map.h', 'w') do |f|
    dump(f)
end

File::open('level.svg', 'w') do |f|
    dump_svg(f)
end

system("inkscape -z -w 2048 -e level.png level.svg > /dev/null")

# links = {}
# 
# $segments.each_with_index do |segment, a|
#     segment[:portals].each_pair do |v, b|
#         if a < b
#             diff = b - a
#             links[a] ||= Set.new()
#             links[a] << b
#             puts "#{a} #{b} #{diff}"
#         end
#     end
# end
# 
# (0..$segments.size).each do |a|
#     (0..$segments.size).each do |b|
#         if links[a] && links[a].include?(b)
#             print sprintf('%-2d ', b - a)
#         else
#             if a < b
#                 print '.  '
#             else
#                 print '   '
#             end
#         end
#     end
#     puts
# end