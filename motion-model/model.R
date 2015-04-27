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
# Inter-servo distance

d=9.8
# Length servo-kink
e=9.8
# Length kink-pen
f=9.8

# Angle limit on f segments
famax = 10

d=9.8; e=9.8; f=9.8
boundsx = c(-0.5, d+0.5)
boundsy = c(11, 16)

d=9.8; e=8.8; f=11.8
boundsx = c(-0.5, d+0.5)
boundsy = c(12.5, 17.5)

d=9.8; e=8.8; f=12.8
boundsx = c(-0.5, d+0.5)
boundsy = c(13.5, 18.5)

d=9.8; e=9.8; f=12.8
boundsx = c(-0.5, d+0.5)
boundsy = c(13.5, 18.5)

seqrep = seq(20,160,by=0.1)

# No, but, seriously, there must be a better way...
data = data.frame(a1=rep(seqrep,length(seqrep)), a2=sort(rep(seqrep,length(seqrep))))
#data = data.frame(a1=c(110,80), a2=180-c(80,110))
#data = data.frame(a1=seq(60,90), a2=c(180-75))
#data = data.frame(a1=20, a2=43)

data$x1=-e*cos(data$a1/180*pi)
data$y1=e*sin(data$a1/180*pi)
data$x2=d-e*cos(data$a2/180*pi)
data$y2=e*sin(data$a2/180*pi)

data$ver1 = (data$x1-0)**2+(data$y1-0)**2-e**2
data$ver2 = (data$x2-d)**2+(data$y2-0)**2-e**2

data$xm = (data$x1+data$x2)/2
data$ym = (data$y1+data$y2)/2

data$dm2 = (data$x1-data$x2)**2+(data$y1-data$y2)**2

# Normal vector
data$ndx = -(data$y2-data$y1)
data$ndy = data$x2-data$x1

data$ndnor = sqrt(data$ndx**2+data$ndy**2)
data$ndx = data$ndx/data$ndnor
data$ndy = data$ndy/data$ndnor

# Leftover length
data$dml = sqrt(f**2-data$dm2/4)
data$x = data$xm+data$ndx*data$dml
data$y = data$ym+data$ndy*data$dml

data$ver3 = (data$x1-data$x)**2+(data$y1-data$y)**2-f**2
data$ver4 = (data$x2-data$x)**2+(data$y2-data$y)**2-f**2

data$ang1b = atan2(data$y, -data$x)/pi*180
data$ang2b = atan2(data$y, -(data$x-d))/pi*180

data$convex = data$y > data$y1 & data$y > data$y2
data$convex = data$convex & data$ang1b-data$a1>10 &
              data$a2-data$ang2b>10

data$convex = data$convex & sqrt(data$dm2)/2 < f*cos(10*pi/180)

data = data[data$convex & !is.na(data$x),]

data$col = 1+data$convex
data$col[data$convex &
         data$x >= boundsx[1] & data$x < boundsx[2] &
         data$y >= boundsy[1] & data$y < boundsy[2]] = 3

png(paste("plot_", d, "-", e, "-", f, ".png", sep=""));
plot(data$x, -data$y, xlim=c(-d,2*d), ylim=c(-(e+f), 0), col=data$col, cex=0.03)
points(c(0, d), -c(0, 0), col=5)
dev.off()

# For debugging only, print the arms position
#lines(c(0, data$x1), -c(0,data$y1), col=3)
#lines(c(d, data$x2), -c(0,data$y2), col=3)

#lines(c(data$x, data$x1), -c(data$y,data$y1), col=4)
#lines(c(data$x, data$x2), -c(data$y,data$y2), col=4)

#pp = data[abs(data$x-d/2)<2 & data$y < 8,]
#plot(pp$x, pp$y)

#pp = data[abs(data$x-0)<0.1 & data$y > 9,]
# End of debugging

# Get interpolation database
space=0.2
sx = seq(boundsx[1], boundsx[2], by=space)
sy = seq(boundsy[1], boundsy[2], by=space)
interdata1 = matrix(ncol=length(sx),nrow=length(sy))
interdata2 = matrix(ncol=length(sx),nrow=length(sy))

for (i in seq(sx)) {
  x = sx[i]
  for (j in seq(sy)) {
    y = sy[j]
    minp = which.min((data$x-x)**2+(data$y-y)**2)
    interdata1[j,i] = data$a1[minp]
    interdata2[j,i] = data$a2[minp]
  }
}

write.table(c(boundsx, boundsy, space, length(sx), length(sy)), "info", row.names=FALSE, col.names=FALSE)
write.table(interdata1, "i1", row.names=FALSE, col.names=FALSE)
write.table(interdata2, "i2", row.names=FALSE, col.names=FALSE)

