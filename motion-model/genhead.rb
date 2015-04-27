# Wooden Plotter
# Copyright (C) 2015 Nicolas Boichat
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

info = File.open("info").to_a.map{|x| x.chomp.to_f}

lenx = info[5];
leny = info[6];

puts "const int START_X = #{(info[0]*10).to_i};"
puts "const int STOP_X = #{(info[1]*10).to_i};"
puts "const int START_Y = #{(info[2]*10).to_i};"
puts "const int STOP_Y = #{(info[3]*10).to_i};"
puts "const int LEN_X = #{lenx.to_i};"
puts "const int LEN_Y = #{leny.to_i};"

puts "const int STEP = #{(info[4]*10).to_i};"

puts "// Assumes step of 1mm"
# Assumes step == 1

puts "//Deci-angle tables"
puts "const int ANGLE1[LEN_Y][LEN_X] PROGMEM = {"
puts File.open("i1").to_a.map{|x|
  "{" + x.split(/ /).map{|y| (y.to_f*10).to_i}.join(", ") + "}"
}.join(",\n")
puts "};"

puts "const int ANGLE2[LEN_Y][LEN_X] PROGMEM = {"
puts File.open("i2").to_a.map{|x|
  "{" + x.split(/ /).map{|y| (y.to_f*10).to_i}.join(", ") + "}"
}.join(",\n")
puts "};"

