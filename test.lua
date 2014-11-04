local mm = require 'maxminddb'
local db = mm.open('/var/lib/libmaxminddb/GeoLite2-City.mmdb')

local res = db:lookup('8.8.8.8')
print(res:get("country", "names", "en"), res:get("location", "longitude"), res:get("location", "latitude"))
