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

import os
import urllib
from pytz.gae import pytz
import time
from datetime import datetime,timedelta

from google.appengine.ext import ndb
from google.appengine.api import memcache

import jinja2
import webapp2



JINJA_ENVIRONMENT = jinja2.Environment(
    loader=jinja2.FileSystemLoader(os.path.dirname(__file__)),
    extensions=['jinja2.ext.autoescape'],
    autoescape=True)

DEFAULT_DEVICE = "clock1"

def command_set_key(device=DEFAULT_DEVICE):
    """Constructs a Datastore key for a Guestbook entity.

    We use command_set as the key.
    """
    return ndb.Key('CommandSet', device)


class Command(ndb.Model):
    """A main model for representing a command."""
    type = ndb.StringProperty(indexed=False)
    content = ndb.StringProperty(indexed=False)
    date = ndb.DateTimeProperty(auto_now_add=True)

#class Device(ndb.Model):
#    """A main model for representing a device."""
#    tz = ndb.StringProperty(indexed=False)
#    lastseen = ndb.DateTimeProperty(indexed=False)
#    ip = ndb.StringProperty(indexed=False)

class MainPage(webapp2.RequestHandler):

    def get(self):
        device = self.request.get('device', DEFAULT_DEVICE)
        query = Command.query(
            ancestor=command_set_key(device)).order(-Command.date)
        cmds = query.fetch(10)

        # Get last seen
        #deviceent = ndb.Key(Device, device).get()
        devicemem = memcache.get(key="lastseen_" + device)
        tz = "UTC"
        if (devicemem):
            tz = devicemem["tz"]
        ptz = pytz.timezone(tz)

        lastseen = "Unknown"
        if (devicemem):
            dt = pytz.utc.localize(devicemem["time"])
            lastseen = dt.astimezone(ptz).strftime('%Y-%m-%d %H:%M:%S %Z')

        for cmd in cmds:
            cmd.datestr = pytz.utc.localize(cmd.date).astimezone(
                ptz).strftime('%Y-%m-%d %H:%M:%S %Z')
            
        template_values = {
            'cmds': cmds,
            'lastseen': lastseen,
            'device': urllib.quote_plus(device)
        }

        template = JINJA_ENVIRONMENT.get_template('index.html')
        self.response.write(template.render(template_values))


class Add(webapp2.RequestHandler):

    def post(self):
        device = self.request.get('device', DEFAULT_DEVICE)
        command = Command(parent=command_set_key(device))
        command.content = self.request.get('text')
        command.type = "text"
        command.put()

        query_params = {'device': device}
        self.redirect('/?' + urllib.urlencode(query_params))

def gettime(tz):
    try:
        ptz = pytz.timezone(tz);
        return datetime.now(ptz).strftime('%H:%M:%S %Y-%m-%d %Z')
    except:
        return 'Inv. TZ'
        
class Clock(webapp2.RequestHandler):

    def get(self):
        tz = self.request.get('tz', "UTC")
        self.response.headers['Content-Type'] = 'text/plain'
        self.response.write(gettime(tz))
        
class Raw(webapp2.RequestHandler):

    def get(self):
        tz = self.request.get('tz', "UTC")
        device = self.request.get('device', DEFAULT_DEVICE)
        laststr = self.request.get('last', "")
        if (laststr != ""):
            lastdate = datetime.fromtimestamp(float(laststr))
        else:
            lastdate = datetime.now()-timedelta(hours=1)
        
        query = Command.query(
            ancestor=command_set_key(device),
            filters=(Command.date > lastdate)).order(-Command.date)
        cmds = query.fetch(1)
        
        self.response.headers['Content-Type'] = 'text/plain'
        if len(cmds) == 0:
            self.response.write(";clock;%s\n" % gettime(tz))
        else:
            cmd = cmds[0]
            seconds = time.mktime(cmd.date.timetuple()) + cmd.date.microsecond * 0.000001
            self.response.write("%f;%s;%s\n" % (seconds, cmd.type, cmd.content))

        # Update last seen
        memcache.set(key="lastseen_" + device,
                     value={ "time":datetime.now(),
                             "tz":tz,
                             "ip":self.request.remote_addr })
        #deviceent = Device.get_or_insert(device)
        #deviceent.lastseen=datetime.now()
        #deviceent.tz = tz
        #deviceent.ip = self.request.remote_addr
        #deviceent.put()
            
app = webapp2.WSGIApplication([
    ('/', MainPage),
    ('/add', Add),
    ('/clock', Clock),
    ('/raw', Raw)
], debug=True)
