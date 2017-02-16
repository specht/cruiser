#!/usr/bin/env ruby

require 'fileutils'
require 'rouge'
require 'redcloth'

def code(path)
    unless File::file?(path)
        path = "#{path}/#{path}.ino"
    end
    source = File.read(path).strip
    formatter = Rouge::Formatters::HTML.new
    lexer = Rouge::Lexers::Cpp.new
    "<notextile><pre class='highlight'>\n#{formatter.format(lexer.lex(source))}</pre></notextile>\n"
end

def stats(flash_usage, min_ram_usage, max_ram_usage = nil)
    
    def add_stats(label, color, amount, max)
        "<tr><td style='white-space: pre;'>#{label}</td><td style='width: 100%; padding: 0 1em;'><div class='barchart-bg'><div class='barchart-fg' style='background-color: #{color}; width: #{amount * 100.0 / max}%;'></div></div></td><td style='text-align: right; white-space: pre;'>#{amount} / #{sprintf('%5d', max)} b</td></tr>"
    end
    
    s = "<table class='barchart'>"
    s += add_stats('Flash usage', '#729fcf', flash_usage, 32256)
    s += add_stats('Min RAM usage', '#ad7fa8', min_ram_usage, 2048)
    if max_ram_usage
        s += add_stats('Max RAM usage', '#f384ae', max_ram_usage, 2048)
    end
    s += "</table>"
    s
end

s = File::read('index.textile').gsub(/#\{.+\}/) do |match|
    eval(match[2, match.size - 3].strip)
end

template = File::read('index.template.html')
template.sub!('{CONTENT_HERE}', RedCloth.new(s).to_html)
template.sub!('{CSS_HERE}', Rouge::Themes::Github.render(scope: '.highlight').gsub("\n", ' ').gsub(/\s+/, ' '))

Dir['images/gb/*.png'].each do |path|
    target = path.sub('/gb/', '/tr/')
    unless FileUtils::uptodate?(target, [path])
        puts "Refreshing #{target}..."
        system("convert #{path} -transparent white -threshold 50% #{target}")
    end
end

File::open('index.html', 'w') do |f|
    f.write(template)
end

