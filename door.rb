#!/usr/bin/env ruby

class LineTest
    def initialize()
        @lines = []
        @intersections = {}
    end
    
    attr_accessor :lines
    attr_reader :intersections

    def dump(io)
        width = 256
        height = 256
        io.puts '<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">'
        io.puts "<svg version='1.1' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' xml:space='preserve' width='#{width}' height='#{height}' >"
        io.puts "<rect width='#{width}' height='#{height}' fill='#fff' />"
        ic = 0
        @lines.each.with_index do |l0, i0|
            @lines.each.with_index do |l1, i1|
                next if i0 == i1
                next if i0 > i1
                ix = (l0[2]*l1[1] - l1[2]*l0[1]) / (l0[0]*l1[1] - l1[0]*l0[1])
                iy = (l0[2]*l1[0] - l1[2]*l0[0]) / (l0[1]*l1[0] - l1[1]*l0[0])
                @intersections[ic] = [ix, iy]
                io.print "<circle cx='#{ix * 200.0 + 28.0}' cy='#{(1.0 - iy) * 200.0 + 28.0}' r='2' stroke='#aaa' stroke-width='1' fill='none' />"
                io.print "<text font-family='Arial' font-size='10px' x='#{ix * 200.0 + 28.0 - 12.0}' y='#{(1.0 - iy) * 200.0 + 28.0 + 12.0}'>#{ic}</text>"
                ic += 1
            end
        end
        @lines.each do |line|
            nx = line[0]
            ny = line[1]
            d = line[2]
            x0 = 0
            y0 = 0
            x1 = 0
            y1 = 0
            if (nx.abs > ny.abs)
                y0 = -0.1
                y1 = 1.1
                x0 = (d - y0 * ny) / nx
                x1 = (d - y1 * ny) / nx
            else
                x0 = -0.1
                x1 = 1.1
                y0 = (d - x0 * nx) / ny
                y1 = (d - x1 * nx) / ny
            end
            io.print "<line x1='#{x0 * 200.0 + 28.0}' y1='#{(1.0 - y0) * 200.0 + 28.0}' x2='#{x1 * 200.0 + 28.0}' y2='#{(1.0 - y1) * 200.0 + 28.0}' stroke='#777' stroke-width='1' />"
        end
        io.puts '</svg>'
    end
    
    def dump_intersections(*v)
        v.each do |i|
            puts "#{sprintf('%2d', i)} #{(@intersections[i][0] * 128.0).to_i} #{(@intersections[i][1] * 128.0).to_i}"
        end
    end
end

test = LineTest.new()

# from t = 0.0 to 0.5: (0 to 105) d = 105
# - draw line from 3 to 14
# - draw line from 24 to 26
# - draw line from 15 to 4
# - draw line from 25 to 23
# - portal: 23, 24, 26, 25
# from t = 0.5 to 0.83333 (5/6): (106 to 176) d = 70
# - draw line from 9 to 14
# - draw line from 24 to 26
# - draw line from 19 to 4
# - draw line from 25 to 23
# - portal: 23, 24, 26, 25
# from t = ~0.83333 to 1.2: (177 to 255) d = 78
# - draw line from 9 to 14
# - draw line from 17 to 21
# - draw line from 19 to 4
# - draw line from 5 to 11
# - portal: 11, 9, 14, 17, 21, 19, 4, 5
# --------------------------------------
# - watched points: 
# 3  0 - -
# 15 0 - -
# 23 0 1 -
# 24 0 1 -
# 25 0 1 -
# 26 0 1 -
# 4  0 1 2
# 14 0 1 2
# 9  - 1 2
# 19 - 1 2
# 5  - - 2
# 11 - - 2
# 17 - - 2
# 21 - - 2

# def setup(test, t)
#     test.lines = []
#     test.lines << [1.0, 0.0, 0.0]
#     test.lines << [0.0, 1.0, 0.0]
#     test.lines << [1.0, 0.0, 1.0]
#     test.lines << [0.0, 1.0, 1.0]
#     test.lines << [-1.0, 2.0, 0.5 - t]
#     test.lines << [-1.0, 2.0, 0.5 + t]
#     test.lines << [2.0, 1.0, 1.5 - t]
#     test.lines << [2.0, 1.0, 1.5 + t]
# end
# 
# setup(test, 0.0)
# File::open('door.svg', 'w') do |f|
#     test.dump(f)
# end
# test.dump_intersections(3, 15, 23, 24, 25, 26, 4, 14)
# puts '-' * 40
# 
# setup(test, 0.5)
# File::open('door.svg', 'w') do |f|
#     test.dump(f)
# end
# test.dump_intersections(3, 15, 23, 24, 25, 26, 4, 14, 9, 19)
# puts '-' * 40
# 
# setup(test, 0.83333)
# File::open('door.svg', 'w') do |f|
#     test.dump(f)
# end
# test.dump_intersections(23, 24, 25, 26, 4, 14, 9, 19, 5, 11, 17, 21)
# puts '-' * 40
# 
# setup(test, 1.2)
# File::open('door.svg', 'w') do |f|
#     test.dump(f)
# end
# test.dump_intersections(4, 14, 9, 19, 5, 11, 17, 21)

# def setup(test, t)
# #     t2 = t ** 0.8
#     t2 = t
#     t2 *= 0.9
#     t2 = 1.0 if t2 > 1.0
#     test.lines = []
#     test.lines << [1.0, 0.0, 0.0]
#     test.lines << [0.0, 1.0, 0.0]
#     test.lines << [1.0, 0.0, 1.0]
#     test.lines << [0.0, 1.0, 1.0]
#     test.lines << [-1.0 + t2, 2.0 - t2, 0.5 - t2 * 0.5]
#     test.lines << [-1.0 + t2, 2.0 - t2, 0.5 + t2 * 0.5]
#     test.lines << [2.0 - t2, 1.0 - t2, 1.5 - t2 * 1.5]
#     test.lines << [2.0 - t2, 1.0 - t2, 1.5 - t2 * 0.5]
# end
# 
# setup(test, 0.0)
# File::open('door.svg', 'w') do |f|
#     test.dump(f)
# end
# test.dump_intersections(3, 15, 23, 24, 25, 26, 4, 14)
# puts '-' * 40
# 
# setup(test, 1.0)
# File::open('door.svg', 'w') do |f|
#     test.dump(f)
# end
# test.dump_intersections(3, 15, 23, 24, 25, 26, 4, 14)

def setup(test, t)
    test.lines = []
    test.lines << [1.0, 0.0, 0.0]
    test.lines << [0.0, 1.0, 0.0]
    test.lines << [1.0, 0.0, 1.0]
    test.lines << [0.0, 1.0, 1.0]
    test.lines << [-1.0, 5.0, 2.0 - t * 2.0]
    test.lines << [-1.0, 5.0, 2.0 + t * 2.0]
    test.lines << [5.0, 1.0, 3.0 - t * 2.0]
    test.lines << [5.0, 1.0, 3.0 + t * 2.0]
end

setup(test, 0.0)
File::open('door.svg', 'w') do |f|
    test.dump(f)
end
test.dump_intersections(3, 15, 23, 24, 25, 26, 4, 14)
puts '-' * 40

setup(test, 1.0)
File::open('door.svg', 'w') do |f|
    test.dump(f)
end
test.dump_intersections(3, 15, 23, 24, 25, 26, 4, 14)
